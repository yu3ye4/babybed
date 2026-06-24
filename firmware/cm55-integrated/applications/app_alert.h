#ifndef APP_ALERT_H__
#define APP_ALERT_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

rt_err_t app_alert_init(void);
rt_err_t app_alert_baby_cry(float confidence);
rt_err_t app_alert_baby_cry_start(float confidence);
rt_err_t app_alert_baby_cry_stop(void);
rt_bool_t app_alert_get_baby_cry(rt_int32_t *confidence_centi);
rt_bool_t app_alert_get_baby_cry_stop(void);
void app_alert_clear_baby_cry(void);
void app_alert_clear_baby_cry_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_ALERT_H__ */
