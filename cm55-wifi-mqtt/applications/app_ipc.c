#include <rtthread.h>
#include <string.h>
#include "app_ipc.h"
#include "shared_data.h"

static shared_data_t *g_shm = RT_NULL;
static uint32_t g_last_version = 0;

rt_err_t app_ipc_init(void)
{
    g_shm = SHARED_DATA_ADDR;
    g_last_version = 0;
    rt_kprintf("[ipc] shared memory mapped at %p\r\n", (void *)g_shm);
    return RT_EOK;
}

rt_err_t app_ipc_read_uplink(char *buf, rt_size_t size)
{
    uint32_t ver;
    uint32_t len;

    if (g_shm == RT_NULL || buf == RT_NULL || size == 0)
        return -RT_ERROR;

    __asm__ volatile("dsb 0xF" ::: "memory");
    ver = g_shm->version;

    if (ver == g_last_version)
        return -RT_ETIMEOUT;

    g_last_version = ver;
    len = g_shm->data_len;
    if (len > size - 1)
        len = size - 1;
    rt_memcpy(buf, g_shm->payload, len);
    buf[len] = '\0';

    return RT_EOK;
}

void app_ipc_put_command(const char *key, rt_int32_t val_centi)
{
    if (g_shm == RT_NULL || key == RT_NULL)
        return;

    if (rt_strcmp(key, "temp_min") == 0)
        g_shm->cmd_temp_min = val_centi;
    else if (rt_strcmp(key, "temp_max") == 0)
        g_shm->cmd_temp_max = val_centi;
    else if (rt_strcmp(key, "humi_min") == 0)
        g_shm->cmd_humi_min = val_centi;
    else if (rt_strcmp(key, "humi_max") == 0)
        g_shm->cmd_humi_max = val_centi;
    else
        return;

    __asm__ volatile("dsb 0xF" ::: "memory");
    g_shm->cmd_version++;
    __asm__ volatile("dsb 0xF" ::: "memory");

    rt_kprintf("[ipc] command sent: %s=%d.%02d\r\n",
               key, val_centi / 100, val_centi % 100);
}

#ifdef RT_USING_MSH
static void app_ipc_dump(int argc, char **argv)
{
    uint32_t len;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    if (g_shm == RT_NULL)
    {
        rt_kprintf("[ipc] shared memory not mapped\r\n");
        return;
    }

    __asm__ volatile("dsb 0xF" ::: "memory");

    len = g_shm->data_len;
    rt_kprintf("[ipc] addr=%p version=%lu last=%lu len=%lu heartbeat=%lu cmd_version=%lu\r\n",
               (void *)g_shm,
               (unsigned long)g_shm->version,
               (unsigned long)g_last_version,
               (unsigned long)len,
               (unsigned long)g_shm->cm33_heartbeat,
               (unsigned long)g_shm->cmd_version);

    if (len > 0 && len < sizeof(g_shm->payload))
    {
        char buf[sizeof(g_shm->payload) + 1];

        rt_memcpy(buf, g_shm->payload, len);
        buf[len] = '\0';
        rt_kprintf("[ipc] payload=%s\r\n", buf);
    }
}
MSH_CMD_EXPORT(app_ipc_dump, dump CM33/CM55 shared memory);
#endif
