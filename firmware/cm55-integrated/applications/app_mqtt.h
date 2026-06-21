#ifndef APP_MQTT_H__
#define APP_MQTT_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MQTT_BROKER_URI         "tcp://192.168.43.9:1883"
#define APP_MQTT_DEVICE_ID          "babybed_01"

#define APP_MQTT_TOPIC_TELEMETRY    "babybed/" APP_MQTT_DEVICE_ID "/telemetry"
#define APP_MQTT_TOPIC_BREATH       "babybed/" APP_MQTT_DEVICE_ID "/breath"
#define APP_MQTT_TOPIC_XIAOZHI      "babybed/" APP_MQTT_DEVICE_ID "/xiaozhi"
#define APP_MQTT_TOPIC_EVENT        "babybed/" APP_MQTT_DEVICE_ID "/event"
#define APP_MQTT_TOPIC_COMMAND      "babybed/" APP_MQTT_DEVICE_ID "/command"

#define APP_MQTT_UPLINK_BUF_SIZE    768
#define APP_MQTT_EVENT_BUF_SIZE     256

#ifndef APP_MQTT_WAIT_XIAOZHI_MS
#define APP_MQTT_WAIT_XIAOZHI_MS    90000
#endif

#ifndef APP_MQTT_ENABLE
#define APP_MQTT_ENABLE             1
#endif

rt_err_t app_mqtt_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MQTT_H__ */
