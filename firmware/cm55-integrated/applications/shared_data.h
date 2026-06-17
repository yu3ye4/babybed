#ifndef SHARED_DATA_H__
#define SHARED_DATA_H__

#include <stdint.h>

#define SHARED_DATA_ADDR    ((shared_data_t *)0x261C0000UL)

typedef struct {
    /* === CM33 -> CM55 (uplink telemetry) === */
    uint32_t version;
    uint32_t data_len;
    char     payload[240];
    uint32_t cm33_heartbeat;

    /* === CM55 -> CM33 (threshold commands) === */
    uint32_t cmd_version;
    int32_t  cmd_temp_min;
    int32_t  cmd_temp_max;
    int32_t  cmd_humi_min;
    int32_t  cmd_humi_max;
} shared_data_t;

#endif /* SHARED_DATA_H__ */
