#ifndef APP_ENV_MONITOR_H__
#define APP_ENV_MONITOR_H__

#include <rtthread.h>

typedef struct
{
    rt_int32_t temp_min_centi;
    rt_int32_t temp_max_centi;
    rt_int32_t humi_min_centi;
    rt_int32_t humi_max_centi;
} app_env_thresholds_t;

typedef struct
{
    rt_int32_t env_risk;
    rt_int32_t env_score;
    const char *env_reason;
    rt_int32_t wet_suspect;
    rt_int32_t wet_score;
    rt_int32_t wet_delta_humi_centi;
    rt_int32_t wet_seconds;
    const char *wet_reason;
    rt_int32_t total_risk;
    rt_int32_t total_score;
    const char *total_reason;
} app_env_status_t;

void app_env_monitor_init(void);
rt_bool_t app_env_monitor_set_threshold(const char *key, rt_int32_t value_centi);
void app_env_monitor_get_thresholds(app_env_thresholds_t *thresholds);
void app_env_monitor_evaluate(rt_int32_t temp_centi,
                              rt_int32_t humi_centi,
                              app_env_status_t *status);

#endif /* APP_ENV_MONITOR_H__ */
