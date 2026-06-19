#ifndef APP_BREATH_H__
#define APP_BREATH_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_BREATH_SAMPLE_HZ          50
#define APP_BREATH_RING_SIZE          500
#define APP_BREATH_DEFAULT_WINDOW     300
#define APP_BREATH_ACTIVE_PP_MV       20

typedef struct
{
    rt_size_t count;
    rt_int32_t base_mv;
    rt_int32_t min_mv;
    rt_int32_t max_mv;
    rt_int32_t pp_mv;
    rt_bool_t active;
} app_breath_stats_t;

rt_err_t app_breath_init(void);
rt_err_t app_breath_get_stats(rt_size_t window, app_breath_stats_t *stats);
rt_size_t app_breath_copy_recent(rt_int32_t *mv_buf, rt_size_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* APP_BREATH_H__ */
