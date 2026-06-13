#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "app_ipc.h"
#include "app_mqtt.h"

int main(void)
{
    rt_kprintf("[CM55] Boot\r\n");

    if (app_ipc_init() != RT_EOK)
    {
        rt_kprintf("[CM55] app_ipc_init failed\r\n");
        return -1;
    }

#if APP_MQTT_ENABLE
    if (app_mqtt_init() != RT_EOK)
    {
        rt_kprintf("[CM55] app_mqtt_init failed\r\n");
        return -1;
    }
#endif

    rt_kprintf("[CM55] ready, waiting for CM33 data\r\n");
    return 0;
}
