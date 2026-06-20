#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/wlan.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "paho_mqtt.h"
#include "app_mqtt.h"
#include "app_ipc.h"
#include "app_event.h"
#include "app_log.h"
#include "app_sensor_aht20.h"
#include "app_alert.h"
#include "app_breath.h"

#define APP_MQTT_THREAD_STACK_SIZE   4096
#define APP_MQTT_THREAD_PRIORITY     15
#define APP_MQTT_THREAD_TICK         20

#define APP_MQTT_KEEPALIVE_SEC       30
#define APP_MQTT_BUF_SIZE            1024
#define APP_MQTT_RECONNECT_DELAY_MS  15000
#define APP_MQTT_LOOP_DELAY_MS       100
#define APP_MQTT_NET_POLL_MS         1000
#define APP_MQTT_NET_LOG_INTERVAL    30
#define APP_MQTT_SENSOR_PUBLISH_MS   2000
#define APP_MQTT_BREATH_PUBLISH_MS   1000
#define APP_MQTT_BREATH_POINTS       APP_BREATH_SAMPLE_HZ
#define APP_MQTT_RAW_HOST            "192.168.43.9"
#define APP_MQTT_RAW_PORT            1883
#define APP_MQTT_RAW_CLIENT_ID       "babybed_01_telemetry"
#define APP_MQTT_RAW_ERR_SOCKET      1
#define APP_MQTT_RAW_ERR_CONNECT     2
#define APP_MQTT_RAW_ERR_CONN_SEND   3
#define APP_MQTT_RAW_ERR_CONNACK     4
#define APP_MQTT_RAW_ERR_TOO_LARGE   5
#define APP_MQTT_RAW_ERR_PUB_SEND    6

#ifndef APP_MQTT_ENABLE_EVENTS
#define APP_MQTT_ENABLE_EVENTS       0
#endif

extern int xiaozhi_is_connected(void);

static MQTTClient g_mqtt_client;
static rt_bool_t g_mqtt_online = RT_FALSE;
static rt_bool_t g_mqtt_need_reconnect = RT_FALSE;
static rt_bool_t g_mqtt_net_ready = RT_FALSE;
static rt_bool_t g_mqtt_first_telemetry_logged = RT_FALSE;
static rt_bool_t g_mqtt_first_breath_logged = RT_FALSE;
static rt_bool_t g_mqtt_vision_ready = RT_FALSE;
static char g_mqtt_vision_payload[APP_MQTT_UPLINK_BUF_SIZE];
static int g_mqtt_raw_sock = -1;
static int g_mqtt_raw_last_error = 0;

static void mqtt_client_deinit(void);

static void mqtt_raw_close(void)
{
    if (g_mqtt_raw_sock >= 0)
    {
        close(g_mqtt_raw_sock);
        g_mqtt_raw_sock = -1;
    }
}

static void mqtt_wait_network_ready(void)
{
    int wait_count = 0;

    while (!rt_wlan_is_ready())
    {
        if ((wait_count % APP_MQTT_NET_LOG_INTERVAL) == 0)
        {
            APP_LOG("mqtt", "wait wlan ready");
        }
        wait_count++;
        rt_thread_mdelay(APP_MQTT_NET_POLL_MS);
    }

    g_mqtt_net_ready = RT_TRUE;
    APP_LOG("mqtt", "wlan ready");
}

static void mqtt_wait_xiaozhi_ready(void)
{
    rt_tick_t start;
    rt_tick_t timeout;

    if (APP_MQTT_WAIT_XIAOZHI_MS <= 0 || xiaozhi_is_connected())
    {
        return;
    }

    start = rt_tick_get();
    timeout = rt_tick_from_millisecond(APP_MQTT_WAIT_XIAOZHI_MS);
    APP_LOG("mqtt", "wait xiaozhi websocket");

    while (!xiaozhi_is_connected())
    {
        if ((rt_tick_get() - start) >= timeout)
        {
            APP_LOG("mqtt", "xiaozhi wait timeout, start mqtt");
            return;
        }
        rt_thread_mdelay(1000);
    }

    APP_LOG("mqtt", "xiaozhi websocket ready");
}

static void mqtt_connect_cb(MQTTClient *c)
{
    RT_UNUSED(c);
}

static void mqtt_online_cb(MQTTClient *c)
{
    RT_UNUSED(c);

    g_mqtt_online = RT_TRUE;
    g_mqtt_first_telemetry_logged = RT_FALSE;
    g_mqtt_first_breath_logged = RT_FALSE;
    APP_LOG("mqtt", "connected");
}

static void mqtt_offline_cb(MQTTClient *c)
{
    RT_UNUSED(c);

    if (g_mqtt_online)
    {
        APP_LOG("mqtt", "offline");
    }
    g_mqtt_online = RT_FALSE;
}

static void mqtt_sub_default_cb(MQTTClient *c, MessageData *data)
{
    RT_UNUSED(c);

    if (data == RT_NULL || data->topicName == RT_NULL)
    {
        return;
    }

    APP_LOG("mqtt", "unexpected topic %.*s",
            data->topicName->lenstring.len,
            data->topicName->lenstring.data);
}

static void mqtt_command_cb(MQTTClient *c, MessageData *data)
{
    char *payload;
    rt_int32_t val;

    RT_UNUSED(c);

    if (data == RT_NULL || data->message == RT_NULL || data->message->payload == RT_NULL)
    {
        return;
    }

    payload = (char *)rt_malloc(data->message->payloadlen + 1);
    if (payload == RT_NULL)
    {
        return;
    }

    rt_memcpy(payload, data->message->payload, data->message->payloadlen);
    payload[data->message->payloadlen] = '\0';

    APP_LOG("mqtt", "cmd recv: %s", payload);

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
    int publish_block = 0;

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
        APP_LOG("mqtt", "buffer alloc failed");
        mqtt_client_deinit();
        return -RT_ENOMEM;
    }

    g_mqtt_client.connect_callback = mqtt_connect_cb;
    g_mqtt_client.online_callback  = mqtt_online_cb;
    g_mqtt_client.offline_callback = mqtt_offline_cb;
    g_mqtt_client.defaultMessageHandler = mqtt_sub_default_cb;
    paho_mqtt_control(&g_mqtt_client, MQTT_CTRL_PUBLISH_BLOCK, &publish_block);

    g_mqtt_client.messageHandlers[0].topicFilter = rt_strdup(APP_MQTT_TOPIC_COMMAND);
    if (g_mqtt_client.messageHandlers[0].topicFilter == RT_NULL)
    {
        APP_LOG("mqtt", "topic alloc failed");
        mqtt_client_deinit();
        return -RT_ENOMEM;
    }
    g_mqtt_client.messageHandlers[0].callback    = mqtt_command_cb;
    g_mqtt_client.messageHandlers[0].qos         = QOS1;

    return RT_EOK;
}

static void mqtt_client_deinit(void)
{
    mqtt_raw_close();

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

static int mqtt_raw_send_all(const rt_uint8_t *buf, int len)
{
    int sent = 0;

    while (sent < len)
    {
        int ret = send(g_mqtt_raw_sock, buf + sent, len - sent, 0);
        if (ret <= 0)
        {
            return -RT_ERROR;
        }
        sent += ret;
    }

    return RT_EOK;
}

static int mqtt_raw_encode_remaining_length(rt_uint8_t *buf, int value)
{
    int count = 0;

    do
    {
        rt_uint8_t encoded = value % 128;
        value /= 128;
        if (value > 0)
        {
            encoded |= 128;
        }
        buf[count++] = encoded;
    } while (value > 0 && count < 4);

    return count;
}

static int mqtt_raw_write_utf8(rt_uint8_t *buf, const char *text)
{
    rt_uint16_t len = (rt_uint16_t)rt_strlen(text);

    buf[0] = (rt_uint8_t)(len >> 8);
    buf[1] = (rt_uint8_t)(len & 0xff);
    rt_memcpy(buf + 2, text, len);

    return len + 2;
}

static int mqtt_raw_connect(void)
{
    struct sockaddr_in addr;
    rt_uint8_t pkt[128];
    rt_uint8_t resp[4];
    int pos = 0;
    int rem_pos;
    int rem_len;
    int client_len;
    int ret;

    if (g_mqtt_raw_sock >= 0)
    {
        return RT_EOK;
    }

    g_mqtt_raw_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_mqtt_raw_sock < 0)
    {
        g_mqtt_raw_last_error = APP_MQTT_RAW_ERR_SOCKET;
        return -RT_ERROR;
    }

    rt_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(APP_MQTT_RAW_PORT);
    addr.sin_addr.s_addr = inet_addr(APP_MQTT_RAW_HOST);

    if (connect(g_mqtt_raw_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        g_mqtt_raw_last_error = APP_MQTT_RAW_ERR_CONNECT;
        mqtt_raw_close();
        return -RT_ERROR;
    }

    client_len = rt_strlen(APP_MQTT_RAW_CLIENT_ID);
    rem_len = 10 + 2 + client_len;

    pkt[pos++] = 0x10;
    rem_pos = pos;
    pos += mqtt_raw_encode_remaining_length(pkt + pos, rem_len);
    pos += mqtt_raw_write_utf8(pkt + pos, "MQTT");
    pkt[pos++] = 4;
    pkt[pos++] = 2;
    pkt[pos++] = 0;
    pkt[pos++] = APP_MQTT_KEEPALIVE_SEC;
    pos += mqtt_raw_write_utf8(pkt + pos, APP_MQTT_RAW_CLIENT_ID);

    RT_UNUSED(rem_pos);

    if (mqtt_raw_send_all(pkt, pos) != RT_EOK)
    {
        g_mqtt_raw_last_error = APP_MQTT_RAW_ERR_CONN_SEND;
        mqtt_raw_close();
        return -RT_ERROR;
    }

    ret = recv(g_mqtt_raw_sock, resp, sizeof(resp), 0);
    if (ret < 4 || resp[0] != 0x20 || resp[1] != 0x02 || resp[3] != 0x00)
    {
        g_mqtt_raw_last_error = APP_MQTT_RAW_ERR_CONNACK;
        mqtt_raw_close();
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int mqtt_raw_publish(const char *topic, const char *payload)
{
    rt_uint8_t pkt[APP_MQTT_BUF_SIZE];
    int topic_len = rt_strlen(topic);
    int payload_len = rt_strlen(payload);
    int rem_len = 2 + topic_len + payload_len;
    int pos = 0;

    if (rem_len + 5 > (int)sizeof(pkt))
    {
        g_mqtt_raw_last_error = APP_MQTT_RAW_ERR_TOO_LARGE;
        return -RT_ERROR;
    }

    if (mqtt_raw_connect() != RT_EOK)
    {
        return -RT_ERROR;
    }

    pkt[pos++] = 0x30;
    pos += mqtt_raw_encode_remaining_length(pkt + pos, rem_len);
    pos += mqtt_raw_write_utf8(pkt + pos, topic);
    rt_memcpy(pkt + pos, payload, payload_len);
    pos += payload_len;

    if (mqtt_raw_send_all(pkt, pos) != RT_EOK)
    {
        g_mqtt_raw_last_error = APP_MQTT_RAW_ERR_PUB_SEND;
        mqtt_raw_close();
        return -RT_ERROR;
    }

    g_mqtt_raw_last_error = 0;
    mqtt_raw_close();
    return RT_EOK;
}

static void mqtt_json_append_raw(char *buf, rt_size_t size, rt_size_t *pos, const char *text)
{
    while (buf != RT_NULL && text != RT_NULL && *text != '\0' && *pos + 1 < size)
    {
        buf[*pos] = *text;
        (*pos)++;
        text++;
    }
    if (buf != RT_NULL && size > 0)
    {
        buf[*pos < size ? *pos : size - 1] = '\0';
    }
}

static void mqtt_json_append_escaped(char *buf, rt_size_t size, rt_size_t *pos, const char *text)
{
    mqtt_json_append_raw(buf, size, pos, "\"");

    while (buf != RT_NULL && text != RT_NULL && *text != '\0' && *pos + 2 < size)
    {
        unsigned char ch = (unsigned char)*text++;

        if (ch == '"' || ch == '\\')
        {
            buf[(*pos)++] = '\\';
            buf[(*pos)++] = (char)ch;
        }
        else if (ch == '\r')
        {
            mqtt_json_append_raw(buf, size, pos, "\\r");
        }
        else if (ch == '\n')
        {
            mqtt_json_append_raw(buf, size, pos, "\\n");
        }
        else if (ch == '\t')
        {
            mqtt_json_append_raw(buf, size, pos, "\\t");
        }
        else if (ch < 0x20)
        {
            buf[(*pos)++] = ' ';
        }
        else
        {
            buf[(*pos)++] = (char)ch;
        }
    }

    mqtt_json_append_raw(buf, size, pos, "\"");
}

static void mqtt_event_to_json(const app_event_msg_t *event, char *buf, rt_size_t size)
{
    rt_size_t pos = 0;
    char tmp[32];

    if (buf == RT_NULL || size == 0)
    {
        return;
    }

    buf[0] = '\0';

    mqtt_json_append_raw(buf, size, &pos, "{\"type\":");
    mqtt_json_append_escaped(buf, size, &pos, event->subtype);

    if (event->state[0] != '\0')
    {
        mqtt_json_append_raw(buf, size, &pos, ",\"state\":");
        mqtt_json_append_escaped(buf, size, &pos, event->state);
    }

    if (event->text[0] != '\0')
    {
        mqtt_json_append_raw(buf, size, &pos, ",\"text\":");
        mqtt_json_append_escaped(buf, size, &pos, event->text);
    }

    rt_snprintf(tmp, sizeof(tmp), ",\"ts\":%lu}", (unsigned long)event->tick);
    mqtt_json_append_raw(buf, size, &pos, tmp);
}

static rt_bool_t mqtt_publish_cry_alert(void)
{
    char alert_buf[APP_MQTT_UPLINK_BUF_SIZE];
    rt_int32_t confidence_centi;
    rt_int32_t temp_centi;
    rt_int32_t humi_centi;
    int ret;

    if (!app_alert_get_baby_cry(&confidence_centi))
    {
        return RT_FALSE;
    }

    if (app_sensor_aht20_read_centi(&temp_centi, &humi_centi) == RT_EOK)
    {
        rt_snprintf(alert_buf, sizeof(alert_buf),
                    "event=baby_crying,temp=%d.%02d,humi=%d.%02d,risk=3,score=90,reason=baby_crying,confidence=%d.%02d,message=婴儿哭了",
                    temp_centi / 100,
                    abs(temp_centi % 100),
                    humi_centi / 100,
                    abs(humi_centi % 100),
                    confidence_centi / 100,
                    abs(confidence_centi % 100));
    }
    else
    {
        rt_snprintf(alert_buf, sizeof(alert_buf),
                    "event=baby_crying,risk=3,score=90,reason=baby_crying,confidence=%d.%02d,message=婴儿哭了",
                    confidence_centi / 100,
                    abs(confidence_centi % 100));
    }

    ret = mqtt_raw_publish(APP_MQTT_TOPIC_TELEMETRY, alert_buf);
    if (ret != PAHO_SUCCESS)
    {
        APP_LOG("mqtt", "publish cry alert fail: %d raw=%d", ret, g_mqtt_raw_last_error);
        return RT_TRUE;
    }

    app_alert_clear_baby_cry();
    APP_LOG("mqtt", "publish cry alert ok");
    return RT_TRUE;
}

static void mqtt_append_breath_stats(char *buf, rt_size_t size)
{
    app_breath_stats_t stats;
    app_breath_stats_t periodic_stats;
    rt_size_t len;

    if (buf == RT_NULL || size == 0)
    {
        return;
    }

    if (app_breath_get_stats(APP_BREATH_ACTIVITY_WINDOW, &stats) != RT_EOK)
    {
        return;
    }
    if (app_breath_get_stats(APP_BREATH_RING_SIZE, &periodic_stats) == RT_EOK)
    {
        stats.motion_count = periodic_stats.motion_count;
        stats.periodic = periodic_stats.periodic;
    }

    len = rt_strlen(buf);
    if (len >= size)
    {
        return;
    }

    rt_snprintf(buf + len,
                size - len,
                ",breath_base_mv=%d,breath_pp_mv=%d,breath_active=%d,breath_filtered_pp_mv=%d,breath_energy_mv=%d,breath_motion_count=%d,breath_periodic=%d,breath_apnea_seconds=%d,breath_state=%s",
                stats.base_mv,
                stats.pp_mv,
                stats.active ? 1 : 0,
                stats.filtered_pp_mv,
                stats.energy_mv,
                stats.motion_count,
                stats.periodic ? 1 : 0,
                stats.apnea_seconds,
                app_breath_state_name(stats.state));
}

static rt_bool_t mqtt_cache_vision_payload(void)
{
    char uplink_buf[APP_MQTT_UPLINK_BUF_SIZE];

    if (app_ipc_read_uplink(uplink_buf, sizeof(uplink_buf)) != RT_EOK)
    {
        return RT_FALSE;
    }

    if (strstr(uplink_buf, "vision=") == RT_NULL)
    {
        return RT_FALSE;
    }

    rt_snprintf(g_mqtt_vision_payload, sizeof(g_mqtt_vision_payload), "%s", uplink_buf);
    g_mqtt_vision_ready = RT_TRUE;

    return RT_TRUE;
}

static void mqtt_append_vision_payload(char *buf, rt_size_t size)
{
    rt_size_t len;
    rt_size_t vision_len;

    if (buf == RT_NULL || size == 0)
    {
        return;
    }

    mqtt_cache_vision_payload();

    if (!g_mqtt_vision_ready)
    {
        return;
    }

    len = rt_strlen(buf);
    vision_len = rt_strlen(g_mqtt_vision_payload);
    if (vision_len == 0 || len >= size)
    {
        return;
    }

    if (len == 0)
    {
        rt_snprintf(buf, size, "%s", g_mqtt_vision_payload);
        return;
    }

    if (len + 1 + vision_len >= size)
    {
        APP_LOG("mqtt", "skip vision payload, telemetry full");
        return;
    }

    rt_snprintf(buf + len, size - len, ",%s", g_mqtt_vision_payload);
}

static void mqtt_publish_breath_waveform(void)
{
    rt_int32_t samples[APP_MQTT_BREATH_POINTS];
    rt_int32_t filtered[APP_MQTT_BREATH_POINTS];
    app_breath_stats_t stats;
    app_breath_stats_t periodic_stats;
    char payload[APP_MQTT_BUF_SIZE];
    rt_size_t count;
    rt_size_t filtered_count;
    rt_size_t pos = 0;
    rt_size_t i;
    int ret;

    if (!g_mqtt_online)
    {
        return;
    }

    if (app_breath_get_stats(APP_BREATH_ACTIVITY_WINDOW, &stats) != RT_EOK)
    {
        return;
    }
    if (app_breath_get_stats(APP_BREATH_RING_SIZE, &periodic_stats) == RT_EOK)
    {
        stats.motion_count = periodic_stats.motion_count;
        stats.periodic = periodic_stats.periodic;
    }

    count = app_breath_copy_recent(samples, APP_MQTT_BREATH_POINTS);
    filtered_count = app_breath_copy_recent_filtered(filtered, APP_MQTT_BREATH_POINTS);
    if (count == 0 || filtered_count == 0)
    {
        return;
    }
    if (filtered_count < count)
    {
        count = filtered_count;
    }

    rt_snprintf(payload,
                sizeof(payload),
                "{\"type\":\"breath\",\"hz\":%d,\"count\":%u,\"base_mv\":%d,\"min_mv\":%d,\"max_mv\":%d,\"pp_mv\":%d,\"filtered_pp_mv\":%d,\"energy_mv\":%d,\"motion_count\":%d,\"periodic\":%d,\"active\":%d,\"apnea_seconds\":%d,\"state\":\"%s\",\"samples\":[",
                APP_BREATH_SAMPLE_HZ,
                (unsigned int)count,
                stats.base_mv,
                stats.min_mv,
                stats.max_mv,
                stats.pp_mv,
                stats.filtered_pp_mv,
                stats.energy_mv,
                stats.motion_count,
                stats.periodic ? 1 : 0,
                stats.active ? 1 : 0,
                stats.apnea_seconds,
                app_breath_state_name(stats.state));
    pos = rt_strlen(payload);

    for (i = 0; i < count && pos + 16 < sizeof(payload); i++)
    {
        pos += rt_snprintf(payload + pos,
                           sizeof(payload) - pos,
                           "%s%d",
                           (i == 0) ? "" : ",",
                           samples[i]);
    }

    if (pos + 16 >= sizeof(payload))
    {
        return;
    }
    pos += rt_snprintf(payload + pos, sizeof(payload) - pos, "],\"raw\":[");

    for (i = 0; i < count && pos + 16 < sizeof(payload); i++)
    {
        pos += rt_snprintf(payload + pos,
                           sizeof(payload) - pos,
                           "%s%d",
                           (i == 0) ? "" : ",",
                           samples[i]);
    }

    if (pos + 20 >= sizeof(payload))
    {
        return;
    }
    pos += rt_snprintf(payload + pos, sizeof(payload) - pos, "],\"filtered\":[");

    for (i = 0; i < count && pos + 16 < sizeof(payload); i++)
    {
        pos += rt_snprintf(payload + pos,
                           sizeof(payload) - pos,
                           "%s%d",
                           (i == 0) ? "" : ",",
                           filtered[i]);
    }

    if (pos + 3 >= sizeof(payload))
    {
        return;
    }

    rt_snprintf(payload + pos, sizeof(payload) - pos, "]}");

    ret = mqtt_raw_publish(APP_MQTT_TOPIC_BREATH, payload);
    if (ret != PAHO_SUCCESS)
    {
        APP_LOG("mqtt", "publish breath fail: %d raw=%d", ret, g_mqtt_raw_last_error);
    }
    else if (!g_mqtt_first_breath_logged)
    {
        g_mqtt_first_breath_logged = RT_TRUE;
        APP_LOG("mqtt", "publish breath ok");
    }
}

static void mqtt_publish_ipc_telemetry(void)
{
    char uplink_buf[APP_MQTT_UPLINK_BUF_SIZE];
    rt_int32_t temp_centi;
    rt_int32_t humi_centi;
    int ret;

    if (!g_mqtt_online)
    {
        return;
    }

    if (mqtt_publish_cry_alert())
    {
        return;
    }

    if (app_sensor_aht20_read_centi(&temp_centi, &humi_centi) == RT_EOK)
    {
        rt_snprintf(uplink_buf, sizeof(uplink_buf),
                    "temp=%d.%02d,humi=%d.%02d,risk=0,score=0,reason=m55_aht20",
                    temp_centi / 100,
                    abs(temp_centi % 100),
                    humi_centi / 100,
                    abs(humi_centi % 100));
        mqtt_append_breath_stats(uplink_buf, sizeof(uplink_buf));
        mqtt_append_vision_payload(uplink_buf, sizeof(uplink_buf));

        ret = mqtt_raw_publish(APP_MQTT_TOPIC_TELEMETRY, uplink_buf);
        if (ret != PAHO_SUCCESS)
        {
            APP_LOG("mqtt", "publish telemetry fail: %d raw=%d", ret, g_mqtt_raw_last_error);
        }
        else if (!g_mqtt_first_telemetry_logged)
        {
            g_mqtt_first_telemetry_logged = RT_TRUE;
            APP_LOG("mqtt", "publish telemetry ok: %s", uplink_buf);
        }
        return;
    }

    uplink_buf[0] = '\0';
    mqtt_append_vision_payload(uplink_buf, sizeof(uplink_buf));
    if (uplink_buf[0] != '\0')
    {
        mqtt_append_breath_stats(uplink_buf, sizeof(uplink_buf));
        ret = mqtt_raw_publish(APP_MQTT_TOPIC_TELEMETRY, uplink_buf);
        if (ret != PAHO_SUCCESS)
        {
            APP_LOG("mqtt", "publish telemetry fail: %d raw=%d", ret, g_mqtt_raw_last_error);
        }
        else if (!g_mqtt_first_telemetry_logged)
        {
            g_mqtt_first_telemetry_logged = RT_TRUE;
            APP_LOG("mqtt", "publish telemetry ok: %s", uplink_buf);
        }
    }
}

static void mqtt_publish_app_event(void)
{
    app_event_msg_t event;
    char event_buf[APP_MQTT_EVENT_BUF_SIZE];
    const char *topic;
    int ret;

    if (!g_mqtt_online)
    {
        return;
    }

    if (app_event_recv(&event, rt_tick_from_millisecond(APP_MQTT_LOOP_DELAY_MS)) != RT_EOK)
    {
        return;
    }

    mqtt_event_to_json(&event, event_buf, sizeof(event_buf));
    topic = (event.kind == APP_EVENT_KIND_SYSTEM) ? APP_MQTT_TOPIC_EVENT : APP_MQTT_TOPIC_XIAOZHI;

    ret = paho_mqtt_publish(&g_mqtt_client, QOS1, topic, event_buf);
    if (ret != PAHO_SUCCESS)
    {
        APP_LOG("mqtt", "publish %s fail: %d", event.subtype, ret);
    }
    else if (event.kind == APP_EVENT_KIND_XIAOZHI)
    {
        APP_LOG("mqtt", "publish xiaozhi ok");
    }
}

static void th_mqtt_entry(void *parameter)
{
    int ret;

    RT_UNUSED(parameter);

    while (1)
    {
        rt_tick_t last_sensor_publish = 0;
        rt_tick_t last_breath_publish = 0;

        mqtt_wait_network_ready();
        mqtt_wait_xiaozhi_ready();

        if (mqtt_client_init() != RT_EOK)
        {
            rt_thread_mdelay(APP_MQTT_RECONNECT_DELAY_MS);
            continue;
        }

        g_mqtt_need_reconnect = RT_FALSE;

        APP_LOG("mqtt", "connecting %s", APP_MQTT_BROKER_URI);
        ret = paho_mqtt_start(&g_mqtt_client);
        if (ret != PAHO_SUCCESS)
        {
            APP_LOG("mqtt", "start failed: %d, retry in %d ms",
                    ret, APP_MQTT_RECONNECT_DELAY_MS);
            mqtt_client_deinit();
            g_mqtt_net_ready = rt_wlan_is_ready();
            if (g_mqtt_net_ready)
            {
                rt_thread_mdelay(APP_MQTT_RECONNECT_DELAY_MS);
            }
            continue;
        }

        APP_LOG("mqtt", "worker started");

        while (!g_mqtt_need_reconnect)
        {
            if (!g_mqtt_online)
            {
                rt_thread_mdelay(APP_MQTT_LOOP_DELAY_MS);
                continue;
            }

            if ((rt_tick_get() - last_breath_publish) >= rt_tick_from_millisecond(APP_MQTT_BREATH_PUBLISH_MS))
            {
                mqtt_publish_breath_waveform();
                last_breath_publish = rt_tick_get();
            }

            mqtt_publish_ipc_telemetry();
            last_sensor_publish = rt_tick_get();
#if APP_MQTT_ENABLE_EVENTS
            mqtt_publish_app_event();

            while (!g_mqtt_need_reconnect &&
                   (rt_tick_get() - last_sensor_publish) < rt_tick_from_millisecond(APP_MQTT_SENSOR_PUBLISH_MS))
            {
                if (!g_mqtt_online)
                {
                    break;
                }
                if ((rt_tick_get() - last_breath_publish) >= rt_tick_from_millisecond(APP_MQTT_BREATH_PUBLISH_MS))
                {
                    mqtt_publish_breath_waveform();
                    last_breath_publish = rt_tick_get();
                }
                mqtt_publish_app_event();
            }
#else
            while (!g_mqtt_need_reconnect &&
                   (rt_tick_get() - last_sensor_publish) < rt_tick_from_millisecond(APP_MQTT_SENSOR_PUBLISH_MS))
            {
                if (!g_mqtt_online)
                {
                    break;
                }
                if ((rt_tick_get() - last_breath_publish) >= rt_tick_from_millisecond(APP_MQTT_BREATH_PUBLISH_MS))
                {
                    mqtt_publish_breath_waveform();
                    last_breath_publish = rt_tick_get();
                }
                rt_thread_mdelay(APP_MQTT_LOOP_DELAY_MS);
            }
#endif
        }

        APP_LOG("mqtt", "reconnecting");
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

    g_mqtt_net_ready = rt_wlan_is_ready();

    th = rt_thread_create("th_mqtt",
                          th_mqtt_entry,
                          RT_NULL,
                          APP_MQTT_THREAD_STACK_SIZE,
                          APP_MQTT_THREAD_PRIORITY,
                          APP_MQTT_THREAD_TICK);
    if (th == RT_NULL)
    {
        APP_LOG("mqtt", "create thread failed");
        return -RT_ENOMEM;
    }

    rt_thread_startup(th);
    APP_LOG("mqtt", "thread started");

    return RT_EOK;
}
