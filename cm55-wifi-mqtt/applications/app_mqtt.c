#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/wlan.h>
#include <string.h>
#include <stdlib.h>
#include "paho_mqtt.h"
#include "app_mqtt.h"
#include "app_ipc.h"

#define APP_MQTT_THREAD_STACK_SIZE   4096
#define APP_MQTT_THREAD_PRIORITY     15
#define APP_MQTT_THREAD_TICK         20

#define APP_MQTT_KEEPALIVE_SEC       30
#define APP_MQTT_BUF_SIZE            1024
#define APP_MQTT_RECONNECT_DELAY_MS  15000

static MQTTClient g_mqtt_client;
static rt_bool_t g_mqtt_online = RT_FALSE;
static rt_bool_t g_mqtt_need_reconnect = RT_FALSE;
static rt_bool_t g_mqtt_net_ready = RT_FALSE;
static rt_sem_t g_mqtt_net_ready_sem = RT_NULL;

static void mqtt_client_deinit(void);

static void mqtt_wlan_ready_cb(int event, struct rt_wlan_buff *buff, void *parameter)
{
    RT_UNUSED(event);
    RT_UNUSED(buff);
    RT_UNUSED(parameter);

    g_mqtt_net_ready = RT_TRUE;
    if (g_mqtt_net_ready_sem != RT_NULL)
    {
        rt_sem_release(g_mqtt_net_ready_sem);
    }
}

static void mqtt_wlan_disconnect_cb(int event, struct rt_wlan_buff *buff, void *parameter)
{
    RT_UNUSED(event);
    RT_UNUSED(buff);
    RT_UNUSED(parameter);

    g_mqtt_net_ready = RT_FALSE;
    g_mqtt_online = RT_FALSE;
    g_mqtt_need_reconnect = RT_TRUE;
}

static void mqtt_wait_network_ready(void)
{
    while (!g_mqtt_net_ready && !rt_wlan_is_ready())
    {
        rt_kprintf("[mqtt] wait wlan ready\r\n");
        rt_sem_take(g_mqtt_net_ready_sem, RT_WAITING_FOREVER);
    }

    g_mqtt_net_ready = RT_TRUE;
}

static void mqtt_connect_cb(MQTTClient *c)
{
    rt_kprintf("[mqtt] connect_cb\r\n");
}

static void mqtt_online_cb(MQTTClient *c)
{
    g_mqtt_online = RT_TRUE;
    rt_kprintf("[mqtt] online, subscriptions ready\r\n");
}

static void mqtt_offline_cb(MQTTClient *c)
{
    g_mqtt_online = RT_FALSE;
    rt_kprintf("[mqtt] offline\r\n");
}

static void mqtt_sub_default_cb(MQTTClient *c, MessageData *data)
{
    rt_kprintf("[mqtt] unexpected msg on topic %.*s\r\n",
               data->topicName->lenstring.len,
               data->topicName->lenstring.data);
}

static void mqtt_command_cb(MQTTClient *c, MessageData *data)
{
    char *payload;
    rt_int32_t val;

    if (data == RT_NULL || data->message == RT_NULL || data->message->payload == RT_NULL)
        return;

    payload = (char *)rt_malloc(data->message->payloadlen + 1);
    if (payload == RT_NULL)
        return;

    rt_memcpy(payload, data->message->payload, data->message->payloadlen);
    payload[data->message->payloadlen] = '\0';

    rt_kprintf("[mqtt] cmd recv: %s\r\n", payload);

    if (rt_strncmp(payload, "SET_THRESH ", 11) == 0)
    {
        char *kv = payload + 11;
        char *eq = strchr(kv, '=');
        if (eq != RT_NULL)
        {
            *eq = '\0';
            val = (rt_int32_t)(atof(eq + 1) * 100.0);
            app_ipc_put_command(kv, val);
        }
    }

    rt_free(payload);
}

static int mqtt_client_init(void)
{
    MQTTPacket_connectData condata = MQTTPacket_connectData_initializer;
    static char client_id[32];

    rt_memset(&g_mqtt_client, 0, sizeof(g_mqtt_client));

    rt_snprintf(client_id, sizeof(client_id), "babybed_%s", APP_MQTT_DEVICE_ID);

    g_mqtt_client.uri = APP_MQTT_BROKER_URI;
    rt_memcpy(&g_mqtt_client.condata, &condata, sizeof(condata));
    g_mqtt_client.condata.clientID.cstring = client_id;
    g_mqtt_client.condata.keepAliveInterval = APP_MQTT_KEEPALIVE_SEC;
    g_mqtt_client.condata.cleansession = 1;

    g_mqtt_client.buf_size = APP_MQTT_BUF_SIZE;
    g_mqtt_client.readbuf_size = APP_MQTT_BUF_SIZE;
    g_mqtt_client.reconnect_interval = APP_MQTT_RECONNECT_DELAY_MS;
    g_mqtt_client.buf = rt_malloc(APP_MQTT_BUF_SIZE);
    g_mqtt_client.readbuf = rt_malloc(APP_MQTT_BUF_SIZE);
    if (g_mqtt_client.buf == RT_NULL || g_mqtt_client.readbuf == RT_NULL)
    {
        rt_kprintf("[mqtt] buffer alloc failed\r\n");
        mqtt_client_deinit();
        return -RT_ENOMEM;
    }

    g_mqtt_client.connect_callback = mqtt_connect_cb;
    g_mqtt_client.online_callback  = mqtt_online_cb;
    g_mqtt_client.offline_callback = mqtt_offline_cb;
    g_mqtt_client.defaultMessageHandler = mqtt_sub_default_cb;

    g_mqtt_client.messageHandlers[0].topicFilter = rt_strdup(APP_MQTT_TOPIC_COMMAND);
    if (g_mqtt_client.messageHandlers[0].topicFilter == RT_NULL)
    {
        rt_kprintf("[mqtt] topic alloc failed\r\n");
        mqtt_client_deinit();
        return -RT_ENOMEM;
    }
    g_mqtt_client.messageHandlers[0].callback    = mqtt_command_cb;
    g_mqtt_client.messageHandlers[0].qos         = QOS1;

    return RT_EOK;
}

static void mqtt_client_deinit(void)
{
    if (g_mqtt_client.messageHandlers[0].topicFilter != RT_NULL)
    {
        rt_free(g_mqtt_client.messageHandlers[0].topicFilter);
        g_mqtt_client.messageHandlers[0].topicFilter = RT_NULL;
    }
    if (g_mqtt_client.buf != RT_NULL)
    {
        rt_free(g_mqtt_client.buf);
        g_mqtt_client.buf = RT_NULL;
    }
    if (g_mqtt_client.readbuf != RT_NULL)
    {
        rt_free(g_mqtt_client.readbuf);
        g_mqtt_client.readbuf = RT_NULL;
    }
}

static void th_mqtt_entry(void *parameter)
{
    char uplink_buf[APP_MQTT_UPLINK_BUF_SIZE];
    int ret;

    RT_UNUSED(parameter);

    while (1)
    {
        mqtt_wait_network_ready();

        if (mqtt_client_init() != RT_EOK)
        {
            rt_thread_mdelay(APP_MQTT_RECONNECT_DELAY_MS);
            continue;
        }

        g_mqtt_need_reconnect = RT_FALSE;

        ret = paho_mqtt_start(&g_mqtt_client);
        if (ret != PAHO_SUCCESS)
        {
            rt_kprintf("[mqtt] start failed (no network?) retry in %d ms\r\n",
                       APP_MQTT_RECONNECT_DELAY_MS);
            mqtt_client_deinit();
            g_mqtt_net_ready = rt_wlan_is_ready();
            if (!g_mqtt_net_ready)
            {
                continue;
            }
            rt_thread_mdelay(APP_MQTT_RECONNECT_DELAY_MS);
            continue;
        }
        rt_kprintf("[mqtt] worker started\r\n");

        while (!g_mqtt_need_reconnect)
        {
            if (app_ipc_read_uplink(uplink_buf, sizeof(uplink_buf)) == RT_EOK)
            {
                rt_kprintf("[mqtt] publish: %s\r\n", uplink_buf);
                if (g_mqtt_online)
                {
                    ret = paho_mqtt_publish(&g_mqtt_client, QOS1,
                                            APP_MQTT_TOPIC_TELEMETRY, uplink_buf);
                    if (ret != PAHO_SUCCESS)
                    {
                        rt_kprintf("[mqtt] publish fail: %d\r\n", ret);
                    }
                }
            }
            rt_thread_mdelay(100);
        }

        rt_kprintf("[mqtt] reconnecting...\r\n");
        paho_mqtt_stop(&g_mqtt_client);
        mqtt_client_deinit();
        g_mqtt_net_ready = rt_wlan_is_ready();
        if (g_mqtt_net_ready)
        {
            rt_thread_mdelay(APP_MQTT_RECONNECT_DELAY_MS);
        }
    }
}

rt_err_t app_mqtt_init(void)
{
    rt_thread_t th;
    rt_err_t err;

    g_mqtt_net_ready_sem = rt_sem_create("mqtt_net", 0, RT_IPC_FLAG_PRIO);
    if (g_mqtt_net_ready_sem == RT_NULL)
    {
        rt_kprintf("[mqtt] create net semaphore failed\r\n");
        return -RT_ENOMEM;
    }

    err = rt_wlan_register_event_handler(RT_WLAN_EVT_READY, mqtt_wlan_ready_cb, RT_NULL);
    if (err != RT_EOK)
    {
        rt_kprintf("[mqtt] register wlan ready event failed: %d\r\n", err);
        rt_sem_delete(g_mqtt_net_ready_sem);
        g_mqtt_net_ready_sem = RT_NULL;
        return err;
    }

    err = rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, mqtt_wlan_disconnect_cb, RT_NULL);
    if (err != RT_EOK)
    {
        rt_kprintf("[mqtt] register wlan disconnect event failed: %d\r\n", err);
        rt_wlan_unregister_event_handler(RT_WLAN_EVT_READY);
        rt_sem_delete(g_mqtt_net_ready_sem);
        g_mqtt_net_ready_sem = RT_NULL;
        return err;
    }

    g_mqtt_net_ready = rt_wlan_is_ready();

    th = rt_thread_create("th_mqtt",
                          th_mqtt_entry,
                          RT_NULL,
                          APP_MQTT_THREAD_STACK_SIZE,
                          APP_MQTT_THREAD_PRIORITY,
                          APP_MQTT_THREAD_TICK);
    if (th == RT_NULL)
    {
        rt_kprintf("[mqtt] create thread failed\r\n");
        rt_wlan_unregister_event_handler(RT_WLAN_EVT_READY);
        rt_wlan_unregister_event_handler(RT_WLAN_EVT_STA_DISCONNECTED);
        rt_sem_delete(g_mqtt_net_ready_sem);
        g_mqtt_net_ready_sem = RT_NULL;
        return -RT_ENOMEM;
    }

    rt_thread_startup(th);
    rt_kprintf("[mqtt] thread started\r\n");

    return RT_EOK;
}
