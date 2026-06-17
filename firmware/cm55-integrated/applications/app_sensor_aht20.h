#ifndef APP_SENSOR_AHT20_H__
#define APP_SENSOR_AHT20_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

rt_err_t app_sensor_aht20_init(void);
rt_err_t app_sensor_aht20_read(float *temperature, float *humidity);
rt_err_t app_sensor_aht20_read_centi(rt_int32_t *temp_centi, rt_int32_t *humi_centi);

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_AHT20_H__ */
