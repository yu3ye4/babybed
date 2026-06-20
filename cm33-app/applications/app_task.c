#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "app_task.h"
#include "app_risk.h"
#include "app_proto.h"
#include "vision_i2c.h"
#include "app_aht20.h"
#include "app_ipc.h"
#include "ai_model.h"
#include "app_ai_mode.h"

#define APP_LED_PIN                  GET_PIN(16, 5)

#define APP_ENV_MQ_ITEM_COUNT        8
#define APP_RISK_MQ_ITEM_COUNT       8

#define APP_ENV_THREAD_STACK_SIZE    2048
#define APP_FUSION_THREAD_STACK_SIZE 2048
#define APP_ALARM_THREAD_STACK_SIZE  1024

typedef struct
{
    app_env_frame_t env;
} app_env_msg_t;

typedef struct
{
    app_risk_result_t risk;
} app_risk_msg_t;

static rt_mq_t g_mq_env = RT_NULL;
static rt_mq_t g_mq_risk = RT_NULL;
static const char *risk_level_text(app_risk_level_t level)
{
    switch (level)
    {
    case APP_RISK_L0_NORMAL: return "L0";
    case APP_RISK_L1_ATTENTION: return "L1";
    case APP_RISK_L2_WARNING: return "L2";
    case APP_RISK_L3_EMERGENCY: return "L3";
    default: return "UNKNOWN";
    }
}

static rt_uint32_t app_now_ms(void)
{
    return (rt_uint32_t)((rt_tick_get() * 1000UL) / RT_TICK_PER_SECOND);
}

static app_risk_result_t app_risk_fusion_with_vision(app_risk_result_t base_risk,
                                                    const vision_status_t *vision)
{
    if (vision == RT_NULL)
    {
        return base_risk;
    }

    if (vision->link != VISION_LINK_ONLINE)
    {
        if (base_risk.level < APP_RISK_L2_WARNING)
        {
            base_risk.level = APP_RISK_L2_WARNING;
        }

        if (base_risk.score < 55)
        {
            base_risk.score = 55;
        }

        base_risk.reason = "vision_offline";
        return base_risk;
    }

    if (vision->stable_found)
    {
        if (base_risk.level < APP_RISK_L1_ATTENTION)
        {
            base_risk.level = APP_RISK_L1_ATTENTION;
        }

        if (base_risk.score < 25)
        {
            base_risk.score = 25;
        }

        base_risk.reason = "face_stable_detected";
    }

    return base_risk;
}

static void th_env_entry(void *parameter)
{
    app_aht20_data_t aht20;
    app_env_msg_t msg;
    rt_err_t mq_ret;
    rt_bool_t first_sample_trace = RT_TRUE;
    rt_bool_t first_send_trace = RT_TRUE;

    RT_UNUSED(parameter);

    app_ipc_debug_mark("env thread entered", 0x33000200UL);

    while (1)
    {
        msg.env.ts_ms = app_now_ms();

        if (app_aht20_read(&aht20) == RT_EOK && aht20.valid)
        {
            msg.env.temp_centi_c = (rt_int32_t)(aht20.temperature * 100.0f);
            msg.env.humi_centi_pct = (rt_int32_t)(aht20.humidity * 100.0f);
            if (first_sample_trace)
            {
                rt_kprintf("[env] sample temp=%d.%02dC humi=%d.%02d%%\r\n",
                           msg.env.temp_centi_c / 100,
                           msg.env.temp_centi_c % 100,
                           msg.env.humi_centi_pct / 100,
                           msg.env.humi_centi_pct % 100);
                first_sample_trace = RT_FALSE;
            }
        }
        else
        {
            msg.env.temp_centi_c = 0;
            msg.env.humi_centi_pct = 0;
            rt_kprintf("[env] aht20 read failed\r\n");
        }

        msg.env.smoke_ppm = 0;

        mq_ret = rt_mq_send(g_mq_env, &msg, sizeof(msg));
        if (first_send_trace)
        {
            rt_kprintf("[env] mq_send ret=%d\r\n", mq_ret);
            first_send_trace = RT_FALSE;
        }

        if (mq_ret != RT_EOK)
        {
            rt_kprintf("[env] mq full, drop sample\r\n");
        }

        rt_thread_mdelay(2000);
    }
}

static void th_fusion_entry(void *parameter)
{
    app_env_msg_t env_msg;
    app_risk_msg_t risk_msg;
    app_risk_result_t risk;
    vision_status_t vision_status;
    app_ai_mode_t ai_mode;
    char frame_text[APP_PROTO_MAX_TEXT_LEN];
    rt_size_t text_len;
    rt_ssize_t recv_len;
    rt_bool_t first_recv_trace = RT_TRUE;

    RT_UNUSED(parameter);

    app_ipc_debug_mark("fusion thread entered", 0x33000300UL);
    rt_kprintf("[fusion] thread entered, wait env mq\r\n");

    while (1)
    {
        recv_len = rt_mq_recv(g_mq_env, &env_msg, sizeof(env_msg), RT_WAITING_FOREVER);
        if (recv_len < 0)
        {
            continue;
        }

        if (first_recv_trace)
        {
            rt_kprintf("[fusion] recv env temp=%d.%02dC humi=%d.%02d%%\r\n",
                       env_msg.env.temp_centi_c / 100,
                       env_msg.env.temp_centi_c % 100,
                       env_msg.env.humi_centi_pct / 100,
                       env_msg.env.humi_centi_pct % 100);
            first_recv_trace = RT_FALSE;
        }

        app_ipc_get_commands();
        ai_mode = app_ai_mode_get();

        risk = app_risk_eval_env(&env_msg.env);

        if (ai_mode == APP_AI_MODE_EXTERNAL_VISION && vision_i2c_get_status(&vision_status) == RT_EOK)
        {
            risk = app_risk_fusion_with_vision(risk, &vision_status);
            text_len = app_proto_format_uplink_with_vision(frame_text,
                                                           sizeof(frame_text),
                                                           &env_msg.env,
                                                           &risk,
                                                           &vision_status,
                                                           ai_mode);
        }
        else if (ai_mode == APP_AI_MODE_LOCAL_MODEL)
        {
            if (risk.reason == RT_NULL || rt_strcmp(risk.reason, "normal") == 0)
            {
                risk.reason = "local_ai_model_linked";
            }

            text_len = app_proto_format_uplink_with_vision(frame_text,
                                                           sizeof(frame_text),
                                                           &env_msg.env,
                                                           &risk,
                                                           RT_NULL,
                                                           ai_mode);
        }
        else if (ai_mode == APP_AI_MODE_EXTERNAL_VISION)
        {
            text_len = app_proto_format_uplink_with_vision(frame_text,
                                                           sizeof(frame_text),
                                                           &env_msg.env,
                                                           &risk,
                                                           RT_NULL,
                                                           ai_mode);
        }
        else
        {
            text_len = app_proto_format_uplink(frame_text, sizeof(frame_text), &env_msg.env, &risk);
        }

        risk_msg.risk = risk;

        if (text_len > 0)
        {
            rt_kprintf("[uplink] %s\r\n", frame_text);
            app_ipc_put_uplink(frame_text, text_len);
        }

        if (rt_mq_send(g_mq_risk, &risk_msg, sizeof(risk_msg)) != RT_EOK)
        {
            rt_kprintf("[fusion] risk mq full, drop risk\r\n");
        }
    }
}

static void th_alarm_entry(void *parameter)
{
    app_risk_msg_t risk_msg;
    app_risk_level_t current_level = APP_RISK_L0_NORMAL;
    rt_ssize_t recv_len;

    RT_UNUSED(parameter);
    rt_pin_mode(APP_LED_PIN, PIN_MODE_OUTPUT);

    while (1)
    {
        recv_len = rt_mq_recv(g_mq_risk, &risk_msg, sizeof(risk_msg), rt_tick_from_millisecond(100));
        if (recv_len >= 0)
        {
            current_level = risk_msg.risk.level;
            rt_kprintf("[alarm] level=%s score=%d reason=%s\r\n",
                       risk_level_text(current_level),
                       risk_msg.risk.score,
                       risk_msg.risk.reason);
        }

        if (current_level >= APP_RISK_L2_WARNING)
        {
            rt_pin_write(APP_LED_PIN, PIN_HIGH);
            rt_thread_mdelay(80);
            rt_pin_write(APP_LED_PIN, PIN_LOW);
            rt_thread_mdelay(80);
        }
        else if (current_level == APP_RISK_L1_ATTENTION)
        {
            rt_pin_write(APP_LED_PIN, PIN_HIGH);
            rt_thread_mdelay(80);
            rt_pin_write(APP_LED_PIN, PIN_LOW);
            rt_thread_mdelay(500);
        }
        else
        {
            rt_pin_write(APP_LED_PIN, PIN_LOW);
            rt_thread_mdelay(500);
        }
    }
}

rt_err_t app_task_init(void)
{
    rt_thread_t th;

    app_ipc_debug_mark("app_task_init enter", 0x33000100UL);
    rt_kprintf("[ai] model linked size=%lu input=%ux%ux%u\r\n",
               (unsigned long)ai_model_get_size(),
               (unsigned int)AI_MODEL_INPUT_WIDTH,
               (unsigned int)AI_MODEL_INPUT_HEIGHT,
               (unsigned int)AI_MODEL_INPUT_CHANNELS);
    app_ai_mode_init();

    g_mq_env = rt_mq_create("mq_env", sizeof(app_env_msg_t), APP_ENV_MQ_ITEM_COUNT, RT_IPC_FLAG_FIFO);
    if (g_mq_env == RT_NULL)
    {
        app_ipc_debug_mark("mq_env create failed", 0x33000101UL);
        rt_kprintf("[task] create mq_env failed\r\n");
        return -RT_ENOMEM;
    }
    app_ipc_debug_mark("mq_env created", 0x33000102UL);

    g_mq_risk = rt_mq_create("mq_risk", sizeof(app_risk_msg_t), APP_RISK_MQ_ITEM_COUNT, RT_IPC_FLAG_FIFO);
    if (g_mq_risk == RT_NULL)
    {
        app_ipc_debug_mark("mq_risk create failed", 0x33000103UL);
        rt_kprintf("[task] create mq_risk failed\r\n");
        return -RT_ENOMEM;
    }
    app_ipc_debug_mark("mq_risk created", 0x33000104UL);

    if (app_aht20_init(APP_AHT20_DEFAULT_BUS_NAME) != RT_EOK)
    {
        app_ipc_debug_mark("aht20 init failed", 0x3300010BUL);
        rt_kprintf("[task] app_aht20_init failed\r\n");
    }
    else
    {
        app_ipc_debug_mark("aht20 init ok", 0x3300010CUL);
    }

    th = rt_thread_create("th_fusion",
                          th_fusion_entry,
                          RT_NULL,
                          APP_FUSION_THREAD_STACK_SIZE,
                          17,
                          20);
    if (th == RT_NULL)
    {
        app_ipc_debug_mark("th_fusion create failed", 0x33000107UL);
        rt_kprintf("[task] create th_fusion failed\r\n");
        return -RT_ENOMEM;
    }
    rt_thread_startup(th);
    app_ipc_debug_mark("fusion thread started", 0x33000108UL);

    th = rt_thread_create("th_env",
                          th_env_entry,
                          RT_NULL,
                          APP_ENV_THREAD_STACK_SIZE,
                          18,
                          20);
    if (th == RT_NULL)
    {
        app_ipc_debug_mark("th_env create failed", 0x33000105UL);
        rt_kprintf("[task] create th_env failed\r\n");
        return -RT_ENOMEM;
    }
    rt_thread_startup(th);
    app_ipc_debug_mark("env thread started", 0x33000106UL);

    th = rt_thread_create("th_alarm",
                          th_alarm_entry,
                          RT_NULL,
                          APP_ALARM_THREAD_STACK_SIZE,
                          16,
                          20);
    if (th == RT_NULL)
    {
        app_ipc_debug_mark("th_alarm create failed", 0x33000109UL);
        rt_kprintf("[task] create th_alarm failed\r\n");
        return -RT_ENOMEM;
    }
    rt_thread_startup(th);
    app_ipc_debug_mark("alarm thread started", 0x3300010AUL);

    if (vision_i2c_start() != RT_EOK)
    {
        app_ipc_debug_mark("vision start failed", 0x3300010DUL);
        rt_kprintf("[task] vision_i2c_start failed\r\n");
    }
    else
    {
        app_ipc_debug_mark("vision start ok", 0x3300010EUL);
    }

    app_ipc_debug_mark("app_task_init done", 0x3300010FUL);
    return RT_EOK;
}
