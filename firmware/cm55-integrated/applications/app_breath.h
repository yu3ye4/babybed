#ifndef APP_BREATH_H__
#define APP_BREATH_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_BREATH_SAMPLE_HZ          50
#define APP_BREATH_RING_SIZE          500
#define APP_BREATH_DEFAULT_WINDOW     300
#define APP_BREATH_ACTIVITY_WINDOW    100
#define APP_BREATH_ACTIVE_PP_MV       8
#define APP_BREATH_ACTIVE_ENERGY_MV   3
#define APP_BREATH_WEAK_SECONDS       5
#define APP_BREATH_APNEA_SECONDS      10

typedef enum
{
    APP_BREATH_STATE_NORMAL = 0,
    APP_BREATH_STATE_WEAK_OR_MISSING,
    APP_BREATH_STATE_APNEA_SUSPECT,
} app_breath_state_t;

typedef struct
{
    rt_size_t count;
    rt_int32_t base_mv;
    rt_int32_t min_mv;
    rt_int32_t max_mv;
    rt_int32_t pp_mv;
    rt_bool_t active;
    rt_int32_t filtered_min_mv;
    rt_int32_t filtered_max_mv;
    rt_int32_t filtered_pp_mv;
    rt_int32_t energy_mv;
    rt_int32_t apnea_seconds;
    app_breath_state_t state;
} app_breath_stats_t;

rt_err_t app_breath_init(void);
rt_err_t app_breath_get_stats(rt_size_t window, app_breath_stats_t *stats);
rt_size_t app_breath_copy_recent(rt_int32_t *mv_buf, rt_size_t max_count);
rt_size_t app_breath_copy_recent_filtered(rt_int32_t *mv_buf, rt_size_t max_count);
const char *app_breath_state_name(app_breath_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* APP_BREATH_H__ */
