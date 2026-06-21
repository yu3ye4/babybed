#include <string.h>
#include "app_env_monitor.h"
#include "app_log.h"

#define APP_ENV_TEMP_MIN_DEFAULT_CENTI       1800
#define APP_ENV_TEMP_MAX_DEFAULT_CENTI       3000
#define APP_ENV_HUMI_MIN_DEFAULT_CENTI       3000
#define APP_ENV_HUMI_MAX_DEFAULT_CENTI       7000

#define APP_ENV_TEMP_LOW_CRITICAL_CENTI      1500
#define APP_ENV_TEMP_HIGH_CRITICAL_CENTI     3500
#define APP_ENV_HUMI_HIGH_CRITICAL_CENTI     8000

#define APP_ENV_SCORE_WARN                   40
#define APP_ENV_SCORE_ALERT                  70
#define APP_ENV_WET_SCORE                    80
#define APP_ENV_WET_DELTA_CENTI              800
#define APP_ENV_WET_HIGH_HUMI_CENTI          8000
#define APP_ENV_WET_SECONDS                  30
#define APP_ENV_BASELINE_EMA_DEN             60

static app_env_thresholds_t g_thresholds = {
    APP_ENV_TEMP_MIN_DEFAULT_CENTI,
    APP_ENV_TEMP_MAX_DEFAULT_CENTI,
    APP_ENV_HUMI_MIN_DEFAULT_CENTI,
    APP_ENV_HUMI_MAX_DEFAULT_CENTI,
};

static rt_bool_t g_wet_baseline_ready = RT_FALSE;
static rt_int32_t g_wet_base_humi_centi = 0;
static rt_bool_t g_wet_condition_active = RT_FALSE;
static rt_tick_t g_wet_condition_start = 0;

static rt_int32_t env_abs_i32(rt_int32_t value)
{
    return value < 0 ? -value : value;
}

void app_env_monitor_init(void)
{
    g_thresholds.temp_min_centi = APP_ENV_TEMP_MIN_DEFAULT_CENTI;
    g_thresholds.temp_max_centi = APP_ENV_TEMP_MAX_DEFAULT_CENTI;
    g_thresholds.humi_min_centi = APP_ENV_HUMI_MIN_DEFAULT_CENTI;
    g_thresholds.humi_max_centi = APP_ENV_HUMI_MAX_DEFAULT_CENTI;
    g_wet_baseline_ready = RT_FALSE;
    g_wet_base_humi_centi = 0;
    g_wet_condition_active = RT_FALSE;
    g_wet_condition_start = 0;
    APP_LOG("env", "thresholds temp=%d.%02d..%d.%02d humi=%d.%02d..%d.%02d",
            g_thresholds.temp_min_centi / 100,
            env_abs_i32(g_thresholds.temp_min_centi % 100),
            g_thresholds.temp_max_centi / 100,
            env_abs_i32(g_thresholds.temp_max_centi % 100),
            g_thresholds.humi_min_centi / 100,
            env_abs_i32(g_thresholds.humi_min_centi % 100),
            g_thresholds.humi_max_centi / 100,
            env_abs_i32(g_thresholds.humi_max_centi % 100));
}

rt_bool_t app_env_monitor_set_threshold(const char *key, rt_int32_t value_centi)
{
    if (key == RT_NULL)
    {
        return RT_FALSE;
    }

    if (strcmp(key, "temp_min") == 0)
    {
        g_thresholds.temp_min_centi = value_centi;
    }
    else if (strcmp(key, "temp_max") == 0)
    {
        g_thresholds.temp_max_centi = value_centi;
    }
    else if (strcmp(key, "humi_min") == 0)
    {
        g_thresholds.humi_min_centi = value_centi;
    }
    else if (strcmp(key, "humi_max") == 0)
    {
        g_thresholds.humi_max_centi = value_centi;
    }
    else
    {
        return RT_FALSE;
    }

    APP_LOG("env", "threshold updated: %s=%d.%02d",
            key,
            value_centi / 100,
            env_abs_i32(value_centi % 100));
    return RT_TRUE;
}

void app_env_monitor_get_thresholds(app_env_thresholds_t *thresholds)
{
    if (thresholds == RT_NULL)
    {
        return;
    }
    *thresholds = g_thresholds;
}

static void env_evaluate_limits(rt_int32_t temp_centi,
                                rt_int32_t humi_centi,
                                app_env_status_t *status)
{
    status->env_risk = 0;
    status->env_score = 0;
    status->env_reason = "ok";

    if (temp_centi > APP_ENV_TEMP_HIGH_CRITICAL_CENTI)
    {
        status->env_risk = 2;
        status->env_score = APP_ENV_SCORE_ALERT;
        status->env_reason = "temp_high";
    }
    else if (temp_centi < APP_ENV_TEMP_LOW_CRITICAL_CENTI)
    {
        status->env_risk = 2;
        status->env_score = APP_ENV_SCORE_ALERT;
        status->env_reason = "temp_low";
    }
    else if (humi_centi > APP_ENV_HUMI_HIGH_CRITICAL_CENTI)
    {
        status->env_risk = 2;
        status->env_score = APP_ENV_SCORE_ALERT;
        status->env_reason = "humi_high";
    }
    else if (temp_centi > g_thresholds.temp_max_centi)
    {
        status->env_risk = 1;
        status->env_score = APP_ENV_SCORE_WARN;
        status->env_reason = "temp_high";
    }
    else if (temp_centi < g_thresholds.temp_min_centi)
    {
        status->env_risk = 1;
        status->env_score = APP_ENV_SCORE_WARN;
        status->env_reason = "temp_low";
    }
    else if (humi_centi > g_thresholds.humi_max_centi)
    {
        status->env_risk = 1;
        status->env_score = APP_ENV_SCORE_WARN;
        status->env_reason = "humi_high";
    }
    else if (humi_centi < g_thresholds.humi_min_centi)
    {
        status->env_risk = 1;
        status->env_score = APP_ENV_SCORE_WARN;
        status->env_reason = "humi_low";
    }
}

static void env_evaluate_wetness(rt_int32_t humi_centi, app_env_status_t *status)
{
    rt_bool_t rising_condition;
    rt_bool_t high_condition;
    rt_bool_t wet_condition;
    rt_tick_t now = rt_tick_get();

    if (!g_wet_baseline_ready)
    {
        g_wet_base_humi_centi = humi_centi;
        g_wet_baseline_ready = RT_TRUE;
    }

    status->wet_delta_humi_centi = humi_centi - g_wet_base_humi_centi;
    rising_condition = (humi_centi >= g_thresholds.humi_max_centi &&
                        status->wet_delta_humi_centi >= APP_ENV_WET_DELTA_CENTI) ?
                       RT_TRUE :
                       RT_FALSE;
    high_condition = (humi_centi >= APP_ENV_WET_HIGH_HUMI_CENTI) ? RT_TRUE : RT_FALSE;
    wet_condition = (rising_condition || high_condition) ? RT_TRUE : RT_FALSE;

    if (wet_condition)
    {
        if (!g_wet_condition_active)
        {
            g_wet_condition_active = RT_TRUE;
            g_wet_condition_start = now;
        }
        status->wet_seconds = (rt_int32_t)((now - g_wet_condition_start) / RT_TICK_PER_SECOND);
    }
    else
    {
        g_wet_condition_active = RT_FALSE;
        g_wet_condition_start = 0;
        status->wet_seconds = 0;
        g_wet_base_humi_centi += (humi_centi - g_wet_base_humi_centi) / APP_ENV_BASELINE_EMA_DEN;
        status->wet_delta_humi_centi = humi_centi - g_wet_base_humi_centi;
    }

    status->wet_suspect = 0;
    status->wet_score = 0;
    status->wet_reason = "none";

    if (wet_condition)
    {
        status->wet_reason = high_condition ? "high_humi" : "humi_rise";
        if (status->wet_seconds >= APP_ENV_WET_SECONDS)
        {
            status->wet_suspect = 1;
            status->wet_score = APP_ENV_WET_SCORE;
        }
    }
}

void app_env_monitor_evaluate(rt_int32_t temp_centi,
                              rt_int32_t humi_centi,
                              app_env_status_t *status)
{
    if (status == RT_NULL)
    {
        return;
    }

    memset(status, 0, sizeof(*status));
    env_evaluate_limits(temp_centi, humi_centi, status);
    env_evaluate_wetness(humi_centi, status);

    status->total_risk = status->env_risk;
    status->total_score = status->env_score;
    status->total_reason = status->env_risk > 0 ? status->env_reason : "m55_aht20";

    if (status->wet_suspect)
    {
        status->total_risk = 2;
        status->total_score = status->wet_score;
        status->total_reason = "wet_suspect";
    }
}
