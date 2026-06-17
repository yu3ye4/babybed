/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-01-01     RT-Thread    First version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "app_log.h"
#include "app_ipc.h"
#include "app_event.h"
#include "app_mqtt.h"
#include "app_sensor_aht20.h"
#include "app_alert.h"

/*****************************************************************************
 * Macro Definitions
 *****************************************************************************/
#define DBG_TAG    "main"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/* LED Pin */
#define LED_PIN_GREEN       GET_PIN(16, 6)

/* UI initialization timeout (ms) */
#define UI_INIT_TIMEOUT_MS  5000

/*****************************************************************************
 * External Function Declarations
 *****************************************************************************/
extern void xiaozhi_ui_init(void);
extern rt_err_t xiaozhi_ui_wait_ready(rt_int32_t timeout);
extern void wifi_manager_init(void);

/*****************************************************************************
 * Main Entry
 *****************************************************************************/

int main(void)
{
    LOG_I("Cortex-M55 started");
#ifdef BSP_USING_XiaoZhi
    /* Initialize UI subsystem */
    xiaozhi_ui_init();

    /* Wait for UI initialization to complete */
    if (xiaozhi_ui_wait_ready(rt_tick_from_millisecond(UI_INIT_TIMEOUT_MS)) != RT_EOK)
    {
        LOG_W("UI initialization timeout");
    }

    /* Initialize WiFi manager using the original Xiaozhi startup path. */
    wifi_manager_init();

    if (app_log_init() != RT_EOK)
    {
        LOG_E("app_log_init failed");
        return -1;
    }
    APP_LOG("boot", "cm55 integrated start");

    if (app_sensor_aht20_init() != RT_EOK)
    {
        APP_LOG("boot", "app_sensor_aht20_init failed");
    }

    if (app_ipc_init() != RT_EOK)
    {
        APP_LOG("boot", "app_ipc_init failed");
        return -1;
    }

    if (app_event_init() != RT_EOK)
    {
        APP_LOG("boot", "app_event_init failed");
        return -1;
    }

    if (app_alert_init() != RT_EOK)
    {
        APP_LOG("boot", "app_alert_init failed");
    }

#if APP_MQTT_ENABLE
    if (app_mqtt_init() != RT_EOK)
    {
        APP_LOG("boot", "app_mqtt_init failed");
        return -1;
    }
#endif
#endif
    return 0;
}
