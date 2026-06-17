#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include <wavplayer.h>
#include "drv_gpio.h"
#include "app_alert.h"
#include "app_log.h"

#define APP_ALERT_GREEN_LED_PIN      GET_PIN(16, 6)
#define APP_ALERT_BABY_MUSIC_URI     "/webnet/baby_music.wav"
#define APP_ALERT_BLINK_DURATION_MS  10000
#define APP_ALERT_BLINK_PERIOD_MS    250
#define APP_ALERT_CRY_COOLDOWN_MS    60000
#define APP_ALERT_THREAD_STACK_SIZE  1024
#define APP_ALERT_THREAD_PRIORITY    22
#define APP_ALERT_THREAD_TICK        10
#define APP_ALERT_MUSIC_STACK_SIZE   2048
#define APP_ALERT_MUSIC_PRIORITY     23
#define APP_ALERT_MUSIC_TICK         10
#define APP_ALERT_MUSIC_POLL_MS      1000

static rt_mutex_t g_alert_lock = RT_NULL;
static rt_sem_t g_alert_blink_sem = RT_NULL;
static rt_sem_t g_alert_music_sem = RT_NULL;
static rt_thread_t g_alert_blink_thread = RT_NULL;
static rt_thread_t g_alert_music_thread = RT_NULL;
static rt_bool_t g_alert_initialized = RT_FALSE;
static rt_bool_t g_cry_pending = RT_FALSE;
static rt_bool_t g_cry_has_last = RT_FALSE;
static rt_bool_t g_music_should_play = RT_FALSE;
static rt_tick_t g_cry_last_tick = 0;
static rt_int32_t g_cry_confidence_centi = 0;

static rt_int32_t app_alert_confidence_to_centi(float confidence)
{
    rt_int32_t centi;

    if (confidence < 0.0f)
    {
        confidence = 0.0f;
    }
    if (confidence > 1.0f)
    {
        confidence = 1.0f;
    }

    centi = (rt_int32_t)(confidence * 100.0f + 0.5f);
    return centi;
}

static void app_alert_blink_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        rt_tick_t start;
        rt_tick_t duration;

        rt_sem_take(g_alert_blink_sem, RT_WAITING_FOREVER);

        start = rt_tick_get();
        duration = rt_tick_from_millisecond(APP_ALERT_BLINK_DURATION_MS);

        while ((rt_tick_get() - start) < duration)
        {
            rt_pin_write(APP_ALERT_GREEN_LED_PIN, PIN_HIGH);
            rt_thread_mdelay(APP_ALERT_BLINK_PERIOD_MS);
            rt_pin_write(APP_ALERT_GREEN_LED_PIN, PIN_LOW);
            rt_thread_mdelay(APP_ALERT_BLINK_PERIOD_MS);
        }

        rt_pin_write(APP_ALERT_GREEN_LED_PIN, PIN_LOW);
    }
}

static rt_bool_t app_alert_is_baby_music_uri(const char *uri)
{
    return (uri != RT_NULL && strcmp(uri, APP_ALERT_BABY_MUSIC_URI) == 0) ? RT_TRUE : RT_FALSE;
}

static void app_alert_music_start_or_resume(void)
{
    int state = wavplayer_state_get();
    char *uri = wavplayer_uri_get();

    if (state == PLAYER_STATE_PLAYING && app_alert_is_baby_music_uri(uri))
    {
        return;
    }

    if (state == PLAYER_STATE_PAUSED && app_alert_is_baby_music_uri(uri))
    {
        if (wavplayer_resume() == 0)
        {
            APP_LOG("alert", "baby music play/resume");
        }
        else
        {
            APP_LOG("alert", "baby music resume failed");
        }
        return;
    }

    if (wavplayer_play((char *)APP_ALERT_BABY_MUSIC_URI) == 0)
    {
        APP_LOG("alert", "baby music play/resume");
    }
    else
    {
        APP_LOG("alert", "baby music play failed");
    }
}

static void app_alert_music_pause(void)
{
    int state = wavplayer_state_get();
    char *uri = wavplayer_uri_get();

    if (state == PLAYER_STATE_PLAYING && app_alert_is_baby_music_uri(uri))
    {
        if (wavplayer_pause() == 0)
        {
            APP_LOG("alert", "baby music pause");
        }
        else
        {
            APP_LOG("alert", "baby music pause failed");
        }
    }
}

static void app_alert_music_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while (1)
    {
        rt_bool_t should_play;

        rt_sem_take(g_alert_music_sem, rt_tick_from_millisecond(APP_ALERT_MUSIC_POLL_MS));

        rt_mutex_take(g_alert_lock, RT_WAITING_FOREVER);
        should_play = g_music_should_play;
        rt_mutex_release(g_alert_lock);

        if (should_play)
        {
            app_alert_music_start_or_resume();
        }
        else
        {
            app_alert_music_pause();
        }
    }
}

rt_err_t app_alert_init(void)
{
    if (g_alert_initialized)
    {
        return RT_EOK;
    }

    g_alert_lock = rt_mutex_create("alert_lk", RT_IPC_FLAG_PRIO);
    if (g_alert_lock == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    g_alert_blink_sem = rt_sem_create("alert_blink", 0, RT_IPC_FLAG_FIFO);
    if (g_alert_blink_sem == RT_NULL)
    {
        rt_mutex_delete(g_alert_lock);
        g_alert_lock = RT_NULL;
        return -RT_ENOMEM;
    }

    g_alert_music_sem = rt_sem_create("alert_music", 0, RT_IPC_FLAG_FIFO);
    if (g_alert_music_sem == RT_NULL)
    {
        rt_sem_delete(g_alert_blink_sem);
        rt_mutex_delete(g_alert_lock);
        g_alert_blink_sem = RT_NULL;
        g_alert_lock = RT_NULL;
        return -RT_ENOMEM;
    }

    rt_pin_mode(APP_ALERT_GREEN_LED_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(APP_ALERT_GREEN_LED_PIN, PIN_LOW);

    g_alert_blink_thread = rt_thread_create("alert_led",
                                            app_alert_blink_entry,
                                            RT_NULL,
                                            APP_ALERT_THREAD_STACK_SIZE,
                                            APP_ALERT_THREAD_PRIORITY,
                                            APP_ALERT_THREAD_TICK);
    if (g_alert_blink_thread == RT_NULL)
    {
        rt_sem_delete(g_alert_music_sem);
        rt_sem_delete(g_alert_blink_sem);
        rt_mutex_delete(g_alert_lock);
        g_alert_music_sem = RT_NULL;
        g_alert_blink_sem = RT_NULL;
        g_alert_lock = RT_NULL;
        return -RT_ENOMEM;
    }

    g_alert_music_thread = rt_thread_create("alert_music",
                                            app_alert_music_entry,
                                            RT_NULL,
                                            APP_ALERT_MUSIC_STACK_SIZE,
                                            APP_ALERT_MUSIC_PRIORITY,
                                            APP_ALERT_MUSIC_TICK);
    if (g_alert_music_thread == RT_NULL)
    {
        rt_thread_delete(g_alert_blink_thread);
        rt_sem_delete(g_alert_music_sem);
        rt_sem_delete(g_alert_blink_sem);
        rt_mutex_delete(g_alert_lock);
        g_alert_blink_thread = RT_NULL;
        g_alert_music_sem = RT_NULL;
        g_alert_blink_sem = RT_NULL;
        g_alert_lock = RT_NULL;
        return -RT_ENOMEM;
    }

    rt_thread_startup(g_alert_blink_thread);
    rt_thread_startup(g_alert_music_thread);
    g_alert_initialized = RT_TRUE;
    APP_LOG("alert", "ready");

    return RT_EOK;
}

rt_err_t app_alert_baby_cry_start(float confidence)
{
    rt_tick_t now;
    rt_int32_t confidence_centi = 0;
    rt_bool_t send_alert = RT_FALSE;

    if (!g_alert_initialized && app_alert_init() != RT_EOK)
    {
        return -RT_ERROR;
    }

    now = rt_tick_get();

    rt_mutex_take(g_alert_lock, RT_WAITING_FOREVER);
    if (g_cry_has_last &&
        (now - g_cry_last_tick) < rt_tick_from_millisecond(APP_ALERT_CRY_COOLDOWN_MS))
    {
        g_music_should_play = RT_TRUE;
    }
    else
    {
        g_cry_has_last = RT_TRUE;
        g_cry_last_tick = now;
        g_cry_confidence_centi = app_alert_confidence_to_centi(confidence);
        confidence_centi = g_cry_confidence_centi;
        g_cry_pending = RT_TRUE;
        g_music_should_play = RT_TRUE;
        send_alert = RT_TRUE;
    }
    rt_mutex_release(g_alert_lock);

    rt_sem_release(g_alert_music_sem);

    if (send_alert)
    {
        APP_LOG("alert", "baby crying alert confidence=%d.%02d",
                confidence_centi / 100,
                confidence_centi % 100);
        rt_sem_release(g_alert_blink_sem);
    }

    return RT_EOK;
}

rt_err_t app_alert_baby_cry(float confidence)
{
    return app_alert_baby_cry_start(confidence);
}

rt_err_t app_alert_baby_cry_stop(void)
{
    if (!g_alert_initialized && app_alert_init() != RT_EOK)
    {
        return -RT_ERROR;
    }

    rt_mutex_take(g_alert_lock, RT_WAITING_FOREVER);
    g_music_should_play = RT_FALSE;
    rt_mutex_release(g_alert_lock);

    APP_LOG("alert", "baby crying stopped");
    rt_sem_release(g_alert_music_sem);

    return RT_EOK;
}

rt_bool_t app_alert_get_baby_cry(rt_int32_t *confidence_centi)
{
    rt_bool_t pending;

    if (!g_alert_initialized || confidence_centi == RT_NULL)
    {
        return RT_FALSE;
    }

    rt_mutex_take(g_alert_lock, RT_WAITING_FOREVER);
    pending = g_cry_pending;
    if (pending)
    {
        *confidence_centi = g_cry_confidence_centi;
    }
    rt_mutex_release(g_alert_lock);

    return pending;
}

void app_alert_clear_baby_cry(void)
{
    if (!g_alert_initialized)
    {
        return;
    }

    rt_mutex_take(g_alert_lock, RT_WAITING_FOREVER);
    g_cry_pending = RT_FALSE;
    rt_mutex_release(g_alert_lock);
}
