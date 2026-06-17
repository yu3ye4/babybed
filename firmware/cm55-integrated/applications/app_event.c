#include <rtthread.h>
#include <string.h>
#include "app_event.h"
#include "app_log.h"

#define APP_EVENT_QUEUE_DEPTH 12

static rt_mq_t g_app_event_mq = RT_NULL;

static void app_event_copy(char *dst, rt_size_t dst_size, const char *src)
{
    if (dst == RT_NULL || dst_size == 0)
    {
        return;
    }

    if (src == RT_NULL)
    {
        dst[0] = '\0';
        return;
    }

    rt_strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static rt_err_t app_event_post(app_event_kind_t kind,
                               const char *subtype,
                               const char *state,
                               const char *text)
{
    app_event_msg_t msg;

    if (g_app_event_mq == RT_NULL)
    {
        return -RT_ERROR;
    }

    rt_memset(&msg, 0, sizeof(msg));
    msg.kind = kind;
    msg.tick = rt_tick_get();
    app_event_copy(msg.subtype, sizeof(msg.subtype), subtype);
    app_event_copy(msg.state, sizeof(msg.state), state);
    app_event_copy(msg.text, sizeof(msg.text), text);

    return rt_mq_send(g_app_event_mq, &msg, sizeof(msg));
}

rt_err_t app_event_init(void)
{
    if (g_app_event_mq != RT_NULL)
    {
        return RT_EOK;
    }

    g_app_event_mq = rt_mq_create("app_evt",
                                  sizeof(app_event_msg_t),
                                  APP_EVENT_QUEUE_DEPTH,
                                  RT_IPC_FLAG_FIFO);
    if (g_app_event_mq == RT_NULL)
    {
        APP_LOG("event", "create queue failed");
        return -RT_ENOMEM;
    }

    APP_LOG("event", "queue ready");
    return RT_EOK;
}

rt_err_t app_event_post_xiaozhi_state(const char *state, const char *text)
{
    return app_event_post(APP_EVENT_KIND_XIAOZHI, "state", state, text);
}

rt_err_t app_event_post_xiaozhi_stt(const char *text)
{
    return app_event_post(APP_EVENT_KIND_XIAOZHI, "stt", "", text);
}

rt_err_t app_event_post_xiaozhi_tts(const char *state, const char *text)
{
    return app_event_post(APP_EVENT_KIND_XIAOZHI, "tts", state, text);
}

rt_err_t app_event_post_system(const char *event, const char *detail)
{
    return app_event_post(APP_EVENT_KIND_SYSTEM, "system", event, detail);
}

rt_err_t app_event_recv(app_event_msg_t *msg, rt_int32_t timeout)
{
    rt_ssize_t ret;

    if (g_app_event_mq == RT_NULL || msg == RT_NULL)
    {
        return -RT_ERROR;
    }

    ret = rt_mq_recv(g_app_event_mq, msg, sizeof(*msg), timeout);
    if (ret > 0)
    {
        return RT_EOK;
    }

    return (rt_err_t)ret;
}
