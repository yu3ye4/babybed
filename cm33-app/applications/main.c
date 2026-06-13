#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "app_task.h"
#include "app_ipc.h"

static rt_bool_t g_app_inited = RT_FALSE;

int app_start(void)
{
    if (g_app_inited)
    {
        rt_kprintf("[app] already started\r\n");
        return 0;
    }
    g_app_inited = RT_TRUE;

    rt_kprintf("\r\n=== BabyBed Safety Node Boot ===\r\n");

    if (app_ipc_init() != RT_EOK)
    {
        rt_kprintf("[app] app_ipc_init failed\r\n");
        return -RT_ERROR;
    }

    if (app_task_init() != RT_EOK)
    {
        rt_kprintf("[app] app_task_init failed\r\n");
        return -RT_ERROR;
    }

    rt_kprintf("[app] task framework started\r\n");
    return 0;
}
#ifdef RT_USING_MSH
    MSH_CMD_EXPORT(app_start, start babybed application tasks);
#endif

int main(void)
{
    rt_kprintf("[TRACE] main() entered\r\n");
    app_start();
    return 0;
}
