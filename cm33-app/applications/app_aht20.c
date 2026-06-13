#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>

#include "app_aht20.h"
#include "aht10.h"

static aht10_device_t g_aht20_dev = RT_NULL;
static rt_mutex_t g_aht20_lock = RT_NULL;

static rt_uint32_t aht20_now_ms(void)
{
    return (rt_uint32_t)((rt_tick_get() * 1000UL) / RT_TICK_PER_SECOND);
}

rt_err_t app_aht20_init(const char *i2c_bus_name)
{
    if (g_aht20_dev != RT_NULL)
    {
        return RT_EOK;
    }

    g_aht20_lock = rt_mutex_create("mx_aht20", RT_IPC_FLAG_PRIO);
    if (g_aht20_lock == RT_NULL)
    {
        rt_kprintf("[aht20] mutex create failed\r\n");
        return -RT_ENOMEM;
    }

    g_aht20_dev = aht10_init(i2c_bus_name);
    if (g_aht20_dev == RT_NULL)
    {
        rt_kprintf("[aht20] package init failed on %s\r\n", i2c_bus_name);
        rt_mutex_delete(g_aht20_lock);
        g_aht20_lock = RT_NULL;
        return -RT_ERROR;
    }

    rt_kprintf("[aht20] package init ok on %s addr=0x%02X\r\n",
               i2c_bus_name,
               APP_AHT20_DEFAULT_ADDR);

    return RT_EOK;
}

rt_err_t app_aht20_read(app_aht20_data_t *out_data)
{
    float humidity;
    float temperature;
    static rt_bool_t first_trace = RT_TRUE;

    if (out_data == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (g_aht20_dev == RT_NULL || g_aht20_lock == RT_NULL)
    {
        out_data->valid = RT_FALSE;
        return -RT_ERROR;
    }

    if (first_trace)
    {
        rt_kprintf("[aht20] read begin\r\n");
    }

    rt_mutex_take(g_aht20_lock, RT_WAITING_FOREVER);
    humidity = aht10_read_humidity(g_aht20_dev);
    if (first_trace)
    {
        rt_kprintf("[aht20] humidity read ok\r\n");
    }
    temperature = aht10_read_temperature(g_aht20_dev);
    rt_mutex_release(g_aht20_lock);

    if (first_trace)
    {
        rt_kprintf("[aht20] temperature read ok\r\n");
        first_trace = RT_FALSE;
    }

    out_data->humidity = humidity;
    out_data->temperature = temperature;
    out_data->valid = RT_TRUE;
    out_data->ts_ms = aht20_now_ms();

    return RT_EOK;
}

#ifdef RT_USING_MSH
static void i2c_scan(int argc, char **argv)
{
    const char *bus_name = (argc > 1) ? argv[1] : APP_AHT20_DEFAULT_BUS_NAME;
    struct rt_i2c_bus_device *bus;
    rt_uint8_t addr;
    rt_uint8_t found = 0;

    bus = (struct rt_i2c_bus_device *)rt_device_find(bus_name);
    if (bus == RT_NULL)
    {
        rt_kprintf("[i2c] bus %s not found\r\n", bus_name);
        return;
    }

    rt_kprintf("[i2c] scan %s\r\n", bus_name);
    for (addr = 0x03; addr < 0x78; addr++)
    {
        struct rt_i2c_msg msg;

        rt_memset(&msg, 0, sizeof(msg));
        msg.addr = addr;
        msg.flags = RT_I2C_WR;
        msg.buf = RT_NULL;
        msg.len = 0;

        if (rt_i2c_transfer(bus, &msg, 1) == 1)
        {
            rt_kprintf("[i2c] found addr 0x%02X\r\n", addr);
            found++;
        }
    }

    if (found == 0)
    {
        rt_kprintf("[i2c] no device found on %s\r\n", bus_name);
    }
}
MSH_CMD_EXPORT(i2c_scan, scan i2c bus default i2c1);
#endif
