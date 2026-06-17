#include <rtthread.h>
#include <stdarg.h>
#include <stdio.h>
#include "app_log.h"

static rt_mutex_t g_app_log_mutex = RT_NULL;

rt_err_t app_log_init(void)
{
    if (g_app_log_mutex != RT_NULL)
    {
        return RT_EOK;
    }

    g_app_log_mutex = rt_mutex_create("app_log", RT_IPC_FLAG_PRIO);
    if (g_app_log_mutex == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    return RT_EOK;
}

void app_log_printf(const char *tag, const char *fmt, ...)
{
    char line[APP_LOG_LINE_MAX];
    va_list args;

    if (tag == RT_NULL)
    {
        tag = "log";
    }

    if (fmt == RT_NULL)
    {
        fmt = "";
    }

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    line[sizeof(line) - 1] = '\0';
    va_end(args);

    if (g_app_log_mutex != RT_NULL)
    {
        rt_mutex_take(g_app_log_mutex, RT_WAITING_FOREVER);
    }

    rt_kprintf("[app][%s] %s\r\n", tag, line);

    if (g_app_log_mutex != RT_NULL)
    {
        rt_mutex_release(g_app_log_mutex);
    }
}
