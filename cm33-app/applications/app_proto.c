#include <rtdevice.h>
#include "app_proto.h"

rt_size_t app_proto_format_uplink(char *buf,
                                  rt_size_t buf_size,
                                  const app_env_frame_t *env,
                                  const app_risk_result_t *risk)
{
    return app_proto_format_uplink_with_vision(buf, buf_size, env, risk, RT_NULL);
}

rt_size_t app_proto_format_uplink_with_vision(char *buf,
                                              rt_size_t buf_size,
                                              const app_env_frame_t *env,
                                              const app_risk_result_t *risk,
                                              const vision_status_t *vision)
{
    rt_int32_t n;
    const char *vision_link_text = "na";
    rt_uint8_t face = 0;
    rt_uint8_t face_stable = 0;
    rt_uint8_t cx = 0;
    rt_uint8_t cy = 0;
    rt_uint8_t w = 0;
    rt_uint8_t h = 0;

    if ((buf == RT_NULL) || (env == RT_NULL) || (risk == RT_NULL) || (buf_size == 0))
    {
        return 0;
    }

    if (vision != RT_NULL)
    {
        vision_link_text = (vision->link == VISION_LINK_ONLINE) ? "on" : "off";
        face = vision->last_frame.found ? 1U : 0U;
        face_stable = vision->stable_found ? 1U : 0U;
        cx = vision->last_frame.center_x;
        cy = vision->last_frame.center_y;
        w = vision->last_frame.width;
        h = vision->last_frame.height;
    }

    n = rt_snprintf(buf, buf_size,
                    "ts=%lu,temp=%d.%02dC,humi=%d.%02d%%,smoke=%dppm,risk=%d,score=%u,reason=%s,vision=%s,face=%u,face_stable=%u,cx=%u,cy=%u,w=%u,h=%u",
                    (unsigned long)env->ts_ms,
                    env->temp_centi_c / 100, env->temp_centi_c % 100,
                    env->humi_centi_pct / 100, env->humi_centi_pct % 100,
                    env->smoke_ppm,
                    (int)risk->level,
                    (unsigned int)risk->score,
                    risk->reason,
                    vision_link_text,
                    (unsigned int)face,
                    (unsigned int)face_stable,
                    (unsigned int)cx,
                    (unsigned int)cy,
                    (unsigned int)w,
                    (unsigned int)h);

    if (n <= 0)
    {
        return 0;
    }

    if ((rt_size_t)n >= buf_size)
    {
        return buf_size - 1;
    }

    return (rt_size_t)n;
}
