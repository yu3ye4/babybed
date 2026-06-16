#include <rtthread.h>
#include <string.h>
#include "app_ipc.h"
#include "app_risk.h"
#include "app_ai_mode.h"
#include "shared_data.h"

static shared_data_t *g_shm = RT_NULL;
static uint32_t g_last_cmd_version = 0;

rt_err_t app_ipc_init(void)
{
    static const char init_msg[] = "cm33 app ipc init";

    g_shm = SHARED_DATA_ADDR;
    rt_memset(g_shm, 0, sizeof(shared_data_t));
    g_shm->cmd_ai_mode = -1;
    rt_memcpy(g_shm->payload, init_msg, sizeof(init_msg) - 1);
    g_shm->data_len = sizeof(init_msg) - 1;
    g_shm->cm33_heartbeat = 0x33000002UL;
    __asm__ volatile("dsb 0xF" ::: "memory");
    g_shm->version = 0x33000002UL;
    __asm__ volatile("dsb 0xF" ::: "memory");
    g_last_cmd_version = 0;
    rt_kprintf("[ipc] shared memory init at %p\r\n", (void *)g_shm);
    return RT_EOK;
}

rt_err_t app_ipc_put_uplink(const char *text, rt_size_t len)
{
    if (g_shm == RT_NULL || text == RT_NULL || len == 0 || len > sizeof(g_shm->payload))
        return -RT_ERROR;

    rt_memcpy(g_shm->payload, text, len);
    g_shm->data_len = (uint32_t)len;
    g_shm->cm33_heartbeat++;
    __asm__ volatile("dsb 0xF" ::: "memory");
    g_shm->version++;
    __asm__ volatile("dsb 0xF" ::: "memory");

    return RT_EOK;
}

void app_ipc_debug_mark(const char *text, uint32_t code)
{
    rt_size_t len;

    if (g_shm == RT_NULL || text == RT_NULL)
        return;

    len = rt_strlen(text);
    if (len >= sizeof(g_shm->payload))
        len = sizeof(g_shm->payload) - 1;

    rt_memcpy(g_shm->payload, text, len);
    g_shm->payload[len] = '\0';
    g_shm->data_len = (uint32_t)len;
    g_shm->cm33_heartbeat = code;
    __asm__ volatile("dsb 0xF" ::: "memory");
    g_shm->version = code;
    __asm__ volatile("dsb 0xF" ::: "memory");
}

void app_ipc_get_commands(void)
{
    int32_t val;

    if (g_shm == RT_NULL)
        return;

    __asm__ volatile("dsb 0xF" ::: "memory");
    if (g_shm->cmd_version == g_last_cmd_version)
        return;

    g_last_cmd_version = g_shm->cmd_version;

    val = g_shm->cmd_temp_min;
    if (val != 0)
    {
        g_risk_temp_min_centi = val;
        rt_kprintf("[ipc] temp_min updated to %d.%02dC\r\n", val / 100, val % 100);
    }

    val = g_shm->cmd_temp_max;
    if (val != 0)
    {
        g_risk_temp_max_centi = val;
        rt_kprintf("[ipc] temp_max updated to %d.%02dC\r\n", val / 100, val % 100);
    }

    val = g_shm->cmd_humi_min;
    if (val != 0)
    {
        g_risk_humi_min_centi = val;
        rt_kprintf("[ipc] humi_min updated to %d.%02d%%\r\n", val / 100, val % 100);
    }

    val = g_shm->cmd_humi_max;
    if (val != 0)
    {
        g_risk_humi_max_centi = val;
        rt_kprintf("[ipc] humi_max updated to %d.%02d%%\r\n", val / 100, val % 100);
    }

    val = g_shm->cmd_ai_mode;
    if (val >= 0)
    {
        app_ai_mode_set((app_ai_mode_t)val);
    }
}
