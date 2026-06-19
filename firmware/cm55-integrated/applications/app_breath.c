#include <rtthread.h>
#include <stdlib.h>
#include "app_ads1115.h"
#include "app_breath.h"
#include "app_log.h"

#define APP_BREATH_THREAD_STACK_SIZE  2048
#define APP_BREATH_THREAD_PRIORITY    18
#define APP_BREATH_THREAD_TICK        20
#define APP_BREATH_RETRY_DELAY_MS     1000

static rt_mutex_t g_breath_lock = RT_NULL;
static rt_thread_t g_breath_thread = RT_NULL;
static rt_bool_t g_breath_initialized = RT_FALSE;
static rt_bool_t g_breath_sampling_online = RT_FALSE;
static rt_int32_t *g_breath_ring = RT_NULL;
static rt_uint16_t g_breath_write_index = 0;
static rt_uint32_t g_breath_sample_count = 0;

static rt_size_t breath_available_locked(void)
{
    return (g_breath_sample_count > APP_BREATH_RING_SIZE) ?
           APP_BREATH_RING_SIZE :
           (rt_size_t)g_breath_sample_count;
}

static rt_uint16_t breath_oldest_index_locked(rt_size_t available)
{
    return (rt_uint16_t)((g_breath_write_index + APP_BREATH_RING_SIZE - available) %
                         APP_BREATH_RING_SIZE);
}

static rt_size_t breath_clamp_window(rt_size_t value)
{
    if (value == 0)
    {
        return APP_BREATH_DEFAULT_WINDOW;
    }
    if (value > APP_BREATH_RING_SIZE)
    {
        return APP_BREATH_RING_SIZE;
    }
    return value;
}

static void breath_push_sample(rt_int32_t mv)
{
    rt_mutex_take(g_breath_lock, RT_WAITING_FOREVER);
    g_breath_ring[g_breath_write_index] = mv;
    g_breath_write_index = (rt_uint16_t)((g_breath_write_index + 1) % APP_BREATH_RING_SIZE);
    g_breath_sample_count++;
    rt_mutex_release(g_breath_lock);
}

static void breath_thread_entry(void *parameter)
{
    rt_tick_t sample_delay = rt_tick_from_millisecond(1000 / APP_BREATH_SAMPLE_HZ);

    RT_UNUSED(parameter);

    if (sample_delay == 0)
    {
        sample_delay = 1;
    }

    while (1)
    {
        rt_int32_t mv;

        if (app_ads1115_read_a0_mv(&mv) != RT_EOK)
        {
            if (g_breath_sampling_online)
            {
                g_breath_sampling_online = RT_FALSE;
                APP_LOG("breath", "ads1115 read failed, retrying");
            }
            rt_thread_mdelay(APP_BREATH_RETRY_DELAY_MS);
            continue;
        }

        if (!g_breath_sampling_online)
        {
            g_breath_sampling_online = RT_TRUE;
            APP_LOG("breath", "sampling online hz=%d window=%d",
                    APP_BREATH_SAMPLE_HZ,
                    APP_BREATH_RING_SIZE);
        }

        breath_push_sample(mv);
        rt_thread_delay(sample_delay);
    }
}

rt_err_t app_breath_init(void)
{
    if (g_breath_initialized)
    {
        return RT_EOK;
    }

    g_breath_lock = rt_mutex_create("breath_lk", RT_IPC_FLAG_PRIO);
    if (g_breath_lock == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    g_breath_ring = (rt_int32_t *)rt_calloc(APP_BREATH_RING_SIZE, sizeof(rt_int32_t));
    if (g_breath_ring == RT_NULL)
    {
        rt_mutex_delete(g_breath_lock);
        g_breath_lock = RT_NULL;
        return -RT_ENOMEM;
    }

    g_breath_thread = rt_thread_create("breath",
                                       breath_thread_entry,
                                       RT_NULL,
                                       APP_BREATH_THREAD_STACK_SIZE,
                                       APP_BREATH_THREAD_PRIORITY,
                                       APP_BREATH_THREAD_TICK);
    if (g_breath_thread == RT_NULL)
    {
        rt_free(g_breath_ring);
        rt_mutex_delete(g_breath_lock);
        g_breath_ring = RT_NULL;
        g_breath_lock = RT_NULL;
        return -RT_ENOMEM;
    }

    rt_thread_startup(g_breath_thread);
    g_breath_initialized = RT_TRUE;
    APP_LOG("breath", "thread started");
    return RT_EOK;
}

rt_size_t app_breath_copy_recent(rt_int32_t *mv_buf, rt_size_t max_count)
{
    rt_size_t available;
    rt_size_t count;
    rt_uint16_t oldest;
    rt_size_t start_offset;
    rt_size_t i;

    if (mv_buf == RT_NULL || max_count == 0 || g_breath_lock == RT_NULL || g_breath_ring == RT_NULL)
    {
        return 0;
    }

    rt_mutex_take(g_breath_lock, RT_WAITING_FOREVER);
    available = breath_available_locked();
    count = (max_count < available) ? max_count : available;
    oldest = breath_oldest_index_locked(available);
    start_offset = available - count;

    for (i = 0; i < count; i++)
    {
        rt_uint16_t index = (rt_uint16_t)((oldest + start_offset + i) % APP_BREATH_RING_SIZE);
        mv_buf[i] = g_breath_ring[index];
    }

    rt_mutex_release(g_breath_lock);
    return count;
}

rt_err_t app_breath_get_stats(rt_size_t window, app_breath_stats_t *stats)
{
    rt_size_t available;
    rt_size_t count;
    rt_uint16_t oldest;
    rt_size_t start_offset;
    rt_size_t i;
    rt_int32_t sum = 0;

    if (stats == RT_NULL)
    {
        return -RT_EINVAL;
    }
    if (g_breath_lock == RT_NULL || g_breath_ring == RT_NULL)
    {
        return -RT_ERROR;
    }

    rt_memset(stats, 0, sizeof(*stats));
    window = breath_clamp_window(window);

    rt_mutex_take(g_breath_lock, RT_WAITING_FOREVER);
    available = breath_available_locked();
    if (available == 0)
    {
        rt_mutex_release(g_breath_lock);
        return -RT_ERROR;
    }

    count = (window < available) ? window : available;
    oldest = breath_oldest_index_locked(available);
    start_offset = available - count;

    for (i = 0; i < count; i++)
    {
        rt_uint16_t index = (rt_uint16_t)((oldest + start_offset + i) % APP_BREATH_RING_SIZE);
        rt_int32_t mv = g_breath_ring[index];

        if (i == 0)
        {
            stats->min_mv = mv;
            stats->max_mv = mv;
        }
        else
        {
            if (mv < stats->min_mv)
            {
                stats->min_mv = mv;
            }
            if (mv > stats->max_mv)
            {
                stats->max_mv = mv;
            }
        }
        sum += mv;
    }
    rt_mutex_release(g_breath_lock);

    stats->count = count;
    stats->base_mv = sum / (rt_int32_t)count;
    stats->pp_mv = stats->max_mv - stats->min_mv;
    stats->active = (stats->pp_mv >= APP_BREATH_ACTIVE_PP_MV) ? RT_TRUE : RT_FALSE;
    return RT_EOK;
}

#ifdef RT_USING_MSH
static rt_size_t breath_arg_window(int argc, char **argv, int index, rt_size_t default_value)
{
    int value;

    if (argc <= index)
    {
        return default_value;
    }

    value = atoi(argv[index]);
    if (value <= 0)
    {
        return default_value;
    }

    return breath_clamp_window((rt_size_t)value);
}

static void breath_stats_cmd(int argc, char **argv)
{
    app_breath_stats_t stats;
    rt_size_t window = breath_arg_window(argc, argv, 1, APP_BREATH_DEFAULT_WINDOW);
    rt_err_t ret;

    ret = app_breath_get_stats(window, &stats);
    if (ret != RT_EOK)
    {
        rt_kprintf("[app][breath] stats failed: %d\r\n", ret);
        return;
    }

    rt_kprintf("[app][breath] count=%u base_mv=%d min_mv=%d max_mv=%d pp_mv=%d active=%d\r\n",
               (unsigned int)stats.count,
               stats.base_mv,
               stats.min_mv,
               stats.max_mv,
               stats.pp_mv,
               stats.active ? 1 : 0);
}
MSH_CMD_EXPORT_ALIAS(breath_stats_cmd, breath_stats, show recent breath signal stats);

static void breath_stream_cmd(int argc, char **argv)
{
    rt_size_t request = breath_arg_window(argc, argv, 1, APP_BREATH_DEFAULT_WINDOW);
    rt_int32_t *samples;
    rt_size_t count;
    rt_size_t i;

    samples = (rt_int32_t *)rt_malloc(sizeof(rt_int32_t) * request);
    if (samples == RT_NULL)
    {
        rt_kprintf("[app][breath] stream alloc failed\r\n");
        return;
    }

    count = app_breath_copy_recent(samples, request);
    rt_kprintf("[app][breath] stream count=%u\r\n", (unsigned int)count);
    rt_kprintf("idx,mv\r\n");
    for (i = 0; i < count; i++)
    {
        rt_kprintf("%u,%d\r\n", (unsigned int)i, samples[i]);
    }

    rt_free(samples);
}
MSH_CMD_EXPORT_ALIAS(breath_stream_cmd, breath_stream, print recent breath signal samples);
#endif
