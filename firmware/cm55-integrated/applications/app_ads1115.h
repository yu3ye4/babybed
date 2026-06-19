#ifndef APP_ADS1115_H__
#define APP_ADS1115_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_ADS1115_I2C_BUS_NAME      "i2c1"
#define APP_ADS1115_ADDR              0x48

rt_err_t app_ads1115_init(void);
rt_err_t app_ads1115_read_a0_raw(rt_int16_t *raw);
rt_err_t app_ads1115_read_a0_mv(rt_int32_t *mv);

#ifdef __cplusplus
}
#endif

#endif /* APP_ADS1115_H__ */
