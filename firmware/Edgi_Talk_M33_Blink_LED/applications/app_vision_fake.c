#include <rtthread.h>
#include <string.h>
#include "app_vision_fake.h"
#include "shared_data.h"

#define APP_VISION_FAKE_STACK_SIZE     1024
#define APP_VISION_FAKE_PRIORITY       20
#define APP_VISION_FAKE_TICK           20
#define APP_VISION_FAKE_PERIOD_MS      1000

static rt_thread_t g_vision_fake_thread = RT_NULL;
static shared_data_t *g_vision_shm = SHARED_DATA_ADDR;

static void app_vision_fake_publish(void)
{
    static const char payload[] =
        "vision=on,vision_posture=safe_sleep,vision_confidence=95,"
        "vision_risk=0,face=1,face_stable=1,reason=cm33_fake_vision";
    rt_size_t len = sizeof(payload) - 1;

    if (len >= sizeof(g_vision_shm->payload))
    {
        len = sizeof(g_vision_shm->payload) - 1;
    }

    memcpy(g_vision_shm->payload, payload, len);
    g_vision_shm->payload[len] = '\0';
    g_vision_shm->data_len = (uint32_t)len;
    __asm__ volatile("dsb 0xF" ::: "memory");
    g_vision_shm->version++;
    g_vision_shm->cm33_heartbeat++;
    __asm__ volatile("dsb 0xF" ::: "memory");
}

static void app_vision_fake_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        app_vision_fake_publish();
        rt_thread_mdelay(APP_VISION_FAKE_PERIOD_MS);
    }
}

rt_err_t app_vision_fake_init(void)
{
    if (g_vision_fake_thread != RT_NULL)
    {
        return RT_EOK;
    }

    g_vision_fake_thread = rt_thread_create("vision_fake",
                                            app_vision_fake_thread_entry,
                                            RT_NULL,
                                            APP_VISION_FAKE_STACK_SIZE,
                                            APP_VISION_FAKE_PRIORITY,
                                            APP_VISION_FAKE_TICK);
    if (g_vision_fake_thread == RT_NULL)
    {
        rt_kprintf("[vision_fake] create thread failed\r\n");
        return -RT_ENOMEM;
    }

    rt_thread_startup(g_vision_fake_thread);
    rt_kprintf("[vision_fake] started\r\n");

    return RT_EOK;
}
