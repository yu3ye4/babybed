#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>

#include "vision_i2c.h"

#define VISION_TASK_NAME                  "th_vision"
#define VISION_TASK_STACK_SIZE            1536
#define VISION_TASK_PRIORITY              19
#define VISION_TASK_TICK                  20

#define VISION_READ_PERIOD_MS             100
#define VISION_OFFLINE_TIMEOUT_MS         1200
#define VISION_STABLE_FOUND_THRESHOLD     3

#define VISION_REQ_CMD_FACE_RESULT        0x01
#define VISION_FRAME_PAYLOAD_LEN          4

typedef struct
{
    struct rt_i2c_bus_device *bus;
    rt_uint8_t addr;

    vision_status_t status;
    rt_uint8_t stable_counter;
    rt_uint32_t last_rx_ms;

    rt_mutex_t lock;
} vision_ctx_t;

static vision_ctx_t g_vision = {0};

static rt_uint32_t vision_now_ms(void)
{
    return (rt_uint32_t)((rt_tick_get() * 1000UL) / RT_TICK_PER_SECOND);
}

static rt_err_t vision_i2c_init(void)
{
    g_vision.bus = (struct rt_i2c_bus_device *)rt_device_find(VISION_I2C_DEFAULT_BUS_NAME);
    if (g_vision.bus == RT_NULL)
    {
        rt_kprintf("[vision] i2c bus %s not found\r\n", VISION_I2C_DEFAULT_BUS_NAME);
        return -RT_ERROR;
    }

    g_vision.addr = VISION_I2C_DEFAULT_ADDR;
    g_vision.lock = rt_mutex_create("mx_vision", RT_IPC_FLAG_PRIO);
    if (g_vision.lock == RT_NULL)
    {
        rt_kprintf("[vision] mutex create failed\r\n");
        return -RT_ENOMEM;
    }

    g_vision.status.link = VISION_LINK_OFFLINE;
    g_vision.status.stable_found = RT_FALSE;
    g_vision.status.stable_count = 0;
    memset(&g_vision.status.last_frame, 0, sizeof(g_vision.status.last_frame));

    return RT_EOK;
}

static rt_err_t vision_i2c_read_frame(vision_frame_t *out_frame)
{
    struct rt_i2c_msg msgs[2];
    rt_uint8_t cmd = VISION_REQ_CMD_FACE_RESULT;
    rt_uint8_t payload[VISION_FRAME_PAYLOAD_LEN] = {0};
    rt_size_t transferred;

    RT_ASSERT(out_frame != RT_NULL);

    msgs[0].addr  = g_vision.addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf   = &cmd;
    msgs[0].len   = 1;

    msgs[1].addr  = g_vision.addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf   = payload;
    msgs[1].len   = VISION_FRAME_PAYLOAD_LEN;

    transferred = rt_i2c_transfer(g_vision.bus, msgs, 2);
    if (transferred != 2)
    {
        return -RT_ERROR;
    }

    out_frame->center_x = payload[0];
    out_frame->center_y = payload[1];
    out_frame->width    = payload[2];
    out_frame->height   = payload[3];
    out_frame->found    = (payload[2] > 0 && payload[3] > 0) ? RT_TRUE : RT_FALSE;
    out_frame->ts_ms    = vision_now_ms();

    return RT_EOK;
}

static void vision_status_update_on_frame(const vision_frame_t *frame)
{
    g_vision.last_rx_ms = frame->ts_ms;
    g_vision.status.link = VISION_LINK_ONLINE;
    g_vision.status.last_frame = *frame;

    if (frame->found)
    {
        if (g_vision.stable_counter < 255)
        {
            g_vision.stable_counter++;
        }
    }
    else
    {
        g_vision.stable_counter = 0;
    }

    g_vision.status.stable_count = g_vision.stable_counter;
    g_vision.status.stable_found = (g_vision.stable_counter >= VISION_STABLE_FOUND_THRESHOLD) ? RT_TRUE : RT_FALSE;
}

static void vision_status_update_on_timeout(rt_uint32_t now_ms)
{
    if ((now_ms - g_vision.last_rx_ms) > VISION_OFFLINE_TIMEOUT_MS)
    {
        g_vision.status.link = VISION_LINK_OFFLINE;
        g_vision.stable_counter = 0;
        g_vision.status.stable_count = 0;
        g_vision.status.stable_found = RT_FALSE;
    }
}

static void vision_task_entry(void *parameter)
{
    vision_frame_t frame;

    RT_UNUSED(parameter);

    while (1)
    {
        rt_err_t read_ret = vision_i2c_read_frame(&frame);

        rt_mutex_take(g_vision.lock, RT_WAITING_FOREVER);
        if (read_ret == RT_EOK)
        {
            vision_status_update_on_frame(&frame);
        }
        else
        {
            vision_status_update_on_timeout(vision_now_ms());
        }
        rt_mutex_release(g_vision.lock);

        rt_thread_mdelay(VISION_READ_PERIOD_MS);
    }
}

rt_err_t vision_i2c_start(void)
{
    rt_thread_t th;
    rt_err_t ret = vision_i2c_init();

    if (ret != RT_EOK)
    {
        return ret;
    }

    th = rt_thread_create(VISION_TASK_NAME,
                          vision_task_entry,
                          RT_NULL,
                          VISION_TASK_STACK_SIZE,
                          VISION_TASK_PRIORITY,
                          VISION_TASK_TICK);
    if (th == RT_NULL)
    {
        rt_kprintf("[vision] create task failed\r\n");
        return -RT_ENOMEM;
    }

    rt_thread_startup(th);
    rt_kprintf("[vision] task started on %s addr=0x%02X\r\n", VISION_I2C_DEFAULT_BUS_NAME, VISION_I2C_DEFAULT_ADDR);

    return RT_EOK;
}

rt_err_t vision_i2c_get_status(vision_status_t *out_status)
{
    if (out_status == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (g_vision.lock == RT_NULL)
    {
        return -RT_ERROR;
    }

    rt_mutex_take(g_vision.lock, RT_WAITING_FOREVER);
    *out_status = g_vision.status;
    rt_mutex_release(g_vision.lock);

    return RT_EOK;
}
