#ifndef APP_RISK_H__
#define APP_RISK_H__

#include <rtthread.h>

typedef enum
{
    APP_RISK_L0_NORMAL = 0,
    APP_RISK_L1_ATTENTION,
    APP_RISK_L2_WARNING,
    APP_RISK_L3_EMERGENCY
} app_risk_level_t;

typedef struct
{
    rt_int32_t temp_centi_c;
    rt_int32_t humi_centi_pct;
    rt_int32_t smoke_ppm;
    rt_uint32_t ts_ms;
} app_env_frame_t;

typedef struct
{
    app_risk_level_t level;
    rt_uint16_t score;
    const char *reason;
} app_risk_result_t;

extern rt_int32_t g_risk_temp_min_centi;
extern rt_int32_t g_risk_temp_max_centi;
extern rt_int32_t g_risk_humi_min_centi;
extern rt_int32_t g_risk_humi_max_centi;

app_risk_result_t app_risk_eval_env(const app_env_frame_t *frame);

#endif /* APP_RISK_H__ */
