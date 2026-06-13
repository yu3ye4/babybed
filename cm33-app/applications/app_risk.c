#include "app_risk.h"

rt_int32_t g_risk_temp_min_centi = 1800;
rt_int32_t g_risk_temp_max_centi = 3200;
rt_int32_t g_risk_humi_min_centi = 3500;
rt_int32_t g_risk_humi_max_centi = 7500;

app_risk_result_t app_risk_eval_env(const app_env_frame_t *frame)
{
    app_risk_result_t ret;
    rt_uint16_t score = 0;
    const char *reason = "normal";

    if (frame == RT_NULL)
    {
        ret.level = APP_RISK_L3_EMERGENCY;
        ret.score = 100;
        ret.reason = "invalid_frame";
        return ret;
    }

    if ((frame->temp_centi_c < g_risk_temp_min_centi) || (frame->temp_centi_c > g_risk_temp_max_centi))
    {
        score += 30;
        reason = "temp_out_of_range";
    }

    if ((frame->humi_centi_pct < g_risk_humi_min_centi) || (frame->humi_centi_pct > g_risk_humi_max_centi))
    {
        score += 35;
        reason = "humidity_out_of_range";
    }

    /* Smoke threshold demo */
    if (frame->smoke_ppm > 120)
    {
        score += 50;
        reason = "smoke_alert";
    }

    ret.score = score;
    ret.reason = reason;

    if (score >= 80)
    {
        ret.level = APP_RISK_L3_EMERGENCY;
    }
    else if (score >= 50)
    {
        ret.level = APP_RISK_L2_WARNING;
    }
    else if (score >= 20)
    {
        ret.level = APP_RISK_L1_ATTENTION;
    }
    else
    {
        ret.level = APP_RISK_L0_NORMAL;
    }

    return ret;
}
