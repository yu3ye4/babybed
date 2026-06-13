#ifndef APP_AHT20_H__
#define APP_AHT20_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_AHT20_DEFAULT_BUS_NAME    "i2c1"
#define APP_AHT20_DEFAULT_ADDR        0x38

typedef struct
{
    float temperature;
    float humidity;
    rt_bool_t valid;
    rt_uint32_t ts_ms;
} app_aht20_data_t;

rt_err_t app_aht20_init(const char *i2c_bus_name);
rt_err_t app_aht20_read(app_aht20_data_t *out_data);

#ifdef __cplusplus
}
#endif

#endif /* APP_AHT20_H__ */
