#ifndef APP_EVENT_H__
#define APP_EVENT_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_EVENT_SUBTYPE_MAX 24
#define APP_EVENT_STATE_MAX   32
#define APP_EVENT_TEXT_MAX    128

typedef enum
{
    APP_EVENT_KIND_XIAOZHI = 0,
    APP_EVENT_KIND_SYSTEM  = 1,
} app_event_kind_t;

typedef struct
{
    app_event_kind_t kind;
    rt_tick_t tick;
    char subtype[APP_EVENT_SUBTYPE_MAX];
    char state[APP_EVENT_STATE_MAX];
    char text[APP_EVENT_TEXT_MAX];
} app_event_msg_t;

rt_err_t app_event_init(void);
rt_err_t app_event_post_xiaozhi_state(const char *state, const char *text);
rt_err_t app_event_post_xiaozhi_stt(const char *text);
rt_err_t app_event_post_xiaozhi_tts(const char *state, const char *text);
rt_err_t app_event_post_system(const char *event, const char *detail);
rt_err_t app_event_recv(app_event_msg_t *msg, rt_int32_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENT_H__ */
