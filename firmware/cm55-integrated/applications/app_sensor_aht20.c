#include <rtthread.h>
#include "aht10.h"
#include "app_sensor_aht20.h"

#ifndef PKG_AHT10_I2C_BUS_NAME
#define PKG_AHT10_I2C_BUS_NAME "i2c1"
#endif

static aht10_device_t g_aht20_dev = RT_NULL;
static rt_mutex_t g_aht20_lock = RT_NULL;

static rt_int32_t sensor_float_to_centi(float value)
{
    if (value >= 0.0f)
    {
        return (rt_int32_t)(value * 100.0f + 0.5f);
    }

    return (rt_int32_t)(value * 100.0f - 0.5f);
}

static rt_int32_t sensor_abs_i32(rt_int32_t value)
{
    return (value < 0) ? -value : value;
}

rt_err_t app_sensor_aht20_init(void)
{
    if (g_aht20_dev != RT_NULL)
    {
        return RT_EOK;
    }

    if (g_aht20_lock == RT_NULL)
    {
        g_aht20_lock = rt_mutex_create("aht20", RT_IPC_FLAG_PRIO);
        if (g_aht20_lock == RT_NULL)
        {
            rt_kprintf("[app][aht20] mutex create failed\r\n");
            return -RT_ENOMEM;
        }
    }

    g_aht20_dev = aht10_init(PKG_AHT10_I2C_BUS_NAME);
    if (g_aht20_dev == RT_NULL)
    {
        rt_kprintf("[app][aht20] init failed on %s\r\n", PKG_AHT10_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    rt_kprintf("[app][aht20] init ok on %s addr=0x38\r\n", PKG_AHT10_I2C_BUS_NAME);
    return RT_EOK;
}

rt_err_t app_sensor_aht20_read(float *temperature, float *humidity)
{
    float temp;
    float humi;
    rt_err_t ret = RT_EOK;

    if (temperature == RT_NULL || humidity == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (g_aht20_dev == RT_NULL && app_sensor_aht20_init() != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (rt_mutex_take(g_aht20_lock, rt_tick_from_millisecond(1000)) != RT_EOK)
    {
        return -RT_ETIMEOUT;
    }

    temp = aht10_read_temperature(g_aht20_dev);
    humi = aht10_read_humidity(g_aht20_dev);

    if (temp < -40.0f || temp > 85.0f || humi < 0.0f || humi > 100.0f)
    {
        ret = -RT_ERROR;
        goto out;
    }

    *temperature = temp;
    *humidity = humi;

out:
    rt_mutex_release(g_aht20_lock);
    return ret;
}

rt_err_t app_sensor_aht20_read_centi(rt_int32_t *temp_centi, rt_int32_t *humi_centi)
{
    float temp;
    float humi;
    rt_err_t ret;

    if (temp_centi == RT_NULL || humi_centi == RT_NULL)
    {
        return -RT_EINVAL;
    }

    ret = app_sensor_aht20_read(&temp, &humi);
    if (ret != RT_EOK)
    {
        return ret;
    }

    *temp_centi = sensor_float_to_centi(temp);
    *humi_centi = sensor_float_to_centi(humi);
    return RT_EOK;
}

#ifdef RT_USING_MSH
static void aht20_read_cmd(int argc, char **argv)
{
    rt_int32_t temp;
    rt_int32_t humi;
    rt_err_t ret;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    ret = app_sensor_aht20_read_centi(&temp, &humi);
    if (ret != RT_EOK)
    {
        rt_kprintf("[app][aht20] read failed: %d\r\n", ret);
        return;
    }

    rt_kprintf("[app][aht20] temp=%d.%02dC humi=%d.%02d%%\r\n",
               temp / 100,
               sensor_abs_i32(temp % 100),
               humi / 100,
               sensor_abs_i32(humi % 100));
}
MSH_CMD_EXPORT_ALIAS(aht20_read_cmd, aht20_read, read AHT20 temperature and humidity once);
#endif
