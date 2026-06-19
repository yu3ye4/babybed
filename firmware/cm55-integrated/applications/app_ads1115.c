#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>
#include "app_ads1115.h"

#define ADS1115_REG_CONVERSION        0x00
#define ADS1115_REG_CONFIG            0x01

#define ADS1115_CONFIG_A0_CONT_128SPS 0xC283
#define ADS1115_FSR_MV                4096
#define ADS1115_MAX_RAW               32768

#define ADS1115_STREAM_DEFAULT_HZ     50
#define ADS1115_STREAM_DEFAULT_COUNT  100
#define ADS1115_STREAM_MIN_HZ         1
#define ADS1115_STREAM_MAX_HZ         128
#define ADS1115_STREAM_MAX_COUNT      10000

static struct rt_i2c_bus_device *g_ads1115_bus = RT_NULL;
static rt_bool_t g_ads1115_configured = RT_FALSE;

static rt_err_t ads1115_write_reg(rt_uint8_t reg, rt_uint16_t value)
{
    rt_uint8_t buf[3];
    struct rt_i2c_msg msg;

    if (g_ads1115_bus == RT_NULL)
    {
        return -RT_ERROR;
    }

    buf[0] = reg;
    buf[1] = (rt_uint8_t)(value >> 8);
    buf[2] = (rt_uint8_t)(value & 0xff);

    msg.addr = APP_ADS1115_ADDR;
    msg.flags = RT_I2C_WR;
    msg.buf = buf;
    msg.len = sizeof(buf);

    return (rt_i2c_transfer(g_ads1115_bus, &msg, 1) == 1) ? RT_EOK : -RT_ERROR;
}

static rt_err_t ads1115_read_reg(rt_uint8_t reg, rt_uint16_t *value)
{
    rt_uint8_t data[2];
    struct rt_i2c_msg msgs[2];

    if (g_ads1115_bus == RT_NULL || value == RT_NULL)
    {
        return -RT_ERROR;
    }

    msgs[0].addr = APP_ADS1115_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &reg;
    msgs[0].len = 1;

    msgs[1].addr = APP_ADS1115_ADDR;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = data;
    msgs[1].len = sizeof(data);

    if (rt_i2c_transfer(g_ads1115_bus, msgs, 2) != 2)
    {
        return -RT_ERROR;
    }

    *value = ((rt_uint16_t)data[0] << 8) | data[1];
    return RT_EOK;
}

static rt_int32_t ads1115_raw_to_mv(rt_int16_t raw)
{
    return ((rt_int32_t)raw * ADS1115_FSR_MV) / ADS1115_MAX_RAW;
}

rt_err_t app_ads1115_init(void)
{
    rt_err_t ret;

    if (g_ads1115_configured)
    {
        return RT_EOK;
    }

    g_ads1115_bus = rt_i2c_bus_device_find(APP_ADS1115_I2C_BUS_NAME);
    if (g_ads1115_bus == RT_NULL)
    {
        rt_kprintf("[app][ads1115] i2c bus %s not found\r\n", APP_ADS1115_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    ret = ads1115_write_reg(ADS1115_REG_CONFIG, ADS1115_CONFIG_A0_CONT_128SPS);
    if (ret != RT_EOK)
    {
        rt_kprintf("[app][ads1115] init failed on %s addr=0x%02x\r\n",
                   APP_ADS1115_I2C_BUS_NAME,
                   APP_ADS1115_ADDR);
        return ret;
    }

    g_ads1115_configured = RT_TRUE;
    rt_kprintf("[app][ads1115] init ok on %s addr=0x%02x\r\n",
               APP_ADS1115_I2C_BUS_NAME,
               APP_ADS1115_ADDR);
    return RT_EOK;
}

rt_err_t app_ads1115_read_a0_raw(rt_int16_t *raw)
{
    rt_uint16_t value;

    if (raw == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (!g_ads1115_configured && app_ads1115_init() != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (ads1115_read_reg(ADS1115_REG_CONVERSION, &value) != RT_EOK)
    {
        return -RT_ERROR;
    }

    *raw = (rt_int16_t)value;
    return RT_EOK;
}

rt_err_t app_ads1115_read_a0_mv(rt_int32_t *mv)
{
    rt_int16_t raw;
    rt_err_t ret;

    if (mv == RT_NULL)
    {
        return -RT_EINVAL;
    }

    ret = app_ads1115_read_a0_raw(&raw);
    if (ret != RT_EOK)
    {
        return ret;
    }

    *mv = ads1115_raw_to_mv(raw);
    return RT_EOK;
}

#ifdef RT_USING_MSH
static int ads1115_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static void ads1115_read_cmd(int argc, char **argv)
{
    rt_int16_t raw;
    rt_int32_t mv;
    rt_err_t ret;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    ret = app_ads1115_read_a0_raw(&raw);
    if (ret != RT_EOK)
    {
        rt_kprintf("[app][ads1115] read raw failed: %d\r\n", ret);
        return;
    }

    mv = ads1115_raw_to_mv(raw);

    rt_kprintf("[app][ads1115] raw=%d mv=%d\r\n", raw, mv);
}
MSH_CMD_EXPORT_ALIAS(ads1115_read_cmd, ads1115_read, read ADS1115 A0 once);

static void ads1115_stream_cmd(int argc, char **argv)
{
    int hz = ADS1115_STREAM_DEFAULT_HZ;
    int count = ADS1115_STREAM_DEFAULT_COUNT;
    rt_tick_t delay_tick;
    int i;

    if (argc > 1)
    {
        hz = atoi(argv[1]);
    }
    if (argc > 2)
    {
        count = atoi(argv[2]);
    }

    hz = ads1115_clamp_int(hz, ADS1115_STREAM_MIN_HZ, ADS1115_STREAM_MAX_HZ);
    count = ads1115_clamp_int(count, 1, ADS1115_STREAM_MAX_COUNT);
    delay_tick = rt_tick_from_millisecond((1000 + hz / 2) / hz);
    if (delay_tick == 0)
    {
        delay_tick = 1;
    }

    rt_kprintf("[app][ads1115] stream hz=%d count=%d\r\n", hz, count);
    rt_kprintf("idx,raw,mv\r\n");

    for (i = 0; i < count; i++)
    {
        rt_int16_t raw;
        rt_int32_t mv;

        if (app_ads1115_read_a0_raw(&raw) != RT_EOK)
        {
            rt_kprintf("[app][ads1115] stream read failed at %d\r\n", i);
            return;
        }

        mv = ads1115_raw_to_mv(raw);
        rt_kprintf("%d,%d,%d\r\n", i, raw, mv);
        rt_thread_delay(delay_tick);
    }
}
MSH_CMD_EXPORT_ALIAS(ads1115_stream_cmd, ads1115_stream, stream ADS1115 A0 samples);
#endif
