#include "sensor_temp_humi.h"
#include <rtthread.h>
#include <rtdevice.h>
#include "aht10.h"

#ifndef PKG_AHT10_I2C_BUS_NAME
#define PKG_AHT10_I2C_BUS_NAME "i2c1"
#endif

static aht10_device_t aht20_dev = RT_NULL;

int sensor_temp_humi_init(void)
{
    if (aht20_dev != RT_NULL)
    {
        return 0;
    }

    aht20_dev = aht10_init(PKG_AHT10_I2C_BUS_NAME);
    if (aht20_dev == RT_NULL)
    {
        rt_kprintf("AHT20 init failed on %s\n", PKG_AHT10_I2C_BUS_NAME);
        return -1;
    }

    rt_kprintf("AHT20 init success on %s\n", PKG_AHT10_I2C_BUS_NAME);
    return 0;
}

int sensor_temp_humi_read(float *temp, float *humi)
{
    if (temp == RT_NULL || humi == RT_NULL)
    {
        return -1;
    }

    if (aht20_dev == RT_NULL)
    {
        return -1;
    }

    *humi = aht10_read_humidity(aht20_dev);
    *temp = aht10_read_temperature(aht20_dev);

    return 0;
}
