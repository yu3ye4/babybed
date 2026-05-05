#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "app_task.h"
#include "app_risk.h"
#include "app_proto.h"
#include "vision_i2c.h"

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
    rt_int32_t base_temp = 2600;
    rt_int32_t base_humi = 5200;
    rt_int32_t base_smoke = 25;
    rt_int32_t idx = 0;
    app_env_msg_t msg;

    RT_UNUSED(parameter);

    while (1)
    {
        msg.env.ts_ms = app_now_ms();
        msg.env.temp_centi_c = base_temp + ((idx % 8) - 4) * 12;
        msg.env.humi_centi_pct = base_humi + ((idx % 10) - 5) * 30;
        msg.env.smoke_ppm = base_smoke + (idx % 5) * 3;

        /* Inject a periodic abnormal sample to test risk flow */
        if ((idx % 25) == 0)
        {
            msg.env.humi_centi_pct = 8200;
        }

        if (rt_mq_send(g_mq_env, &msg, sizeof(msg)) != RT_EOK)
        {
            rt_kprintf("[env] mq full, drop sample\r\n");
        }

        idx++;
        rt_thread_mdelay(1000);
    }
}

static void th_fusion_entry(void *parameter)
{
    app_env_msg_t env_msg;
    app_risk_msg_t risk_msg;
    app_risk_result_t risk;
    vision_status_t vision_status;
    char frame_text[APP_PROTO_MAX_TEXT_LEN];
    rt_size_t text_len;

    RT_UNUSED(parameter);

    while (1)
    {
        if (rt_mq_recv(g_mq_env, &env_msg, sizeof(env_msg), RT_WAITING_FOREVER) != RT_EOK)
        {
            continue;
        }

        risk = app_risk_eval_env(&env_msg.env);

        if (vision_i2c_get_status(&vision_status) == RT_EOK)
        {
            risk = app_risk_fusion_with_vision(risk, &vision_status);
            text_len = app_proto_format_uplink_with_vision(frame_text,
                                                           sizeof(frame_text),
                                                           &env_msg.env,
                                                           &risk,
                                                           &vision_status);
        }
        else
        {
            text_len = app_proto_format_uplink(frame_text, sizeof(frame_text), &env_msg.env, &risk);
        }

        risk_msg.risk = risk;

        if (text_len > 0)
        {
            rt_kprintf("[uplink] %s\r\n", frame_text);
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

    RT_UNUSED(parameter);
    rt_pin_mode(APP_LED_PIN, PIN_MODE_OUTPUT);

    while (1)
    {
        if (rt_mq_recv(g_mq_risk, &risk_msg, sizeof(risk_msg), rt_tick_from_millisecond(100)) == RT_EOK)
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

    g_mq_env = rt_mq_create("mq_env", sizeof(app_env_msg_t), APP_ENV_MQ_ITEM_COUNT, RT_IPC_FLAG_FIFO);
    if (g_mq_env == RT_NULL)
    {
        rt_kprintf("[task] create mq_env failed\r\n");
        return -RT_ENOMEM;
    }

    g_mq_risk = rt_mq_create("mq_risk", sizeof(app_risk_msg_t), APP_RISK_MQ_ITEM_COUNT, RT_IPC_FLAG_FIFO);
    if (g_mq_risk == RT_NULL)
    {
        rt_kprintf("[task] create mq_risk failed\r\n");
        return -RT_ENOMEM;
    }

    th = rt_thread_create("th_env",
                          th_env_entry,
                          RT_NULL,
                          APP_ENV_THREAD_STACK_SIZE,
                          18,
                          20);
    if (th == RT_NULL)
    {
        rt_kprintf("[task] create th_env failed\r\n");
        return -RT_ENOMEM;
    }
    rt_thread_startup(th);

    th = rt_thread_create("th_fusion",
                          th_fusion_entry,
                          RT_NULL,
                          APP_FUSION_THREAD_STACK_SIZE,
                          17,
                          20);
    if (th == RT_NULL)
    {
        rt_kprintf("[task] create th_fusion failed\r\n");
        return -RT_ENOMEM;
    }
    rt_thread_startup(th);

    th = rt_thread_create("th_alarm",
                          th_alarm_entry,
                          RT_NULL,
                          APP_ALARM_THREAD_STACK_SIZE,
                          16,
                          20);
    if (th == RT_NULL)
    {
        rt_kprintf("[task] create th_alarm failed\r\n");
        return -RT_ENOMEM;
    }
    rt_thread_startup(th);

    if (vision_i2c_start() != RT_EOK)
    {
        rt_kprintf("[task] vision_i2c_start failed\r\n");
    }

    return RT_EOK;
}
