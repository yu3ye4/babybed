#include <rtthread.h>
#include <stdlib.h>
#include "app_ads1115.h"
#include "app_breath.h"
#include "app_log.h"

#define APP_BREATH_THREAD_STACK_SIZE  2048
#define APP_BREATH_THREAD_PRIORITY    18
#define APP_BREATH_THREAD_TICK        20
#define APP_BREATH_RETRY_DELAY_MS     1000
#define APP_BREATH_Q                  256
#define APP_BREATH_BASELINE_NUM       1
#define APP_BREATH_BASELINE_DEN       200
#define APP_BREATH_FILTER_NUM         15
#define APP_BREATH_FILTER_DEN         100

static rt_mutex_t g_breath_lock = RT_NULL;
static rt_thread_t g_breath_thread = RT_NULL;
static rt_bool_t g_breath_initialized = RT_FALSE;
static rt_bool_t g_breath_sampling_online = RT_FALSE;
static rt_int32_t *g_breath_ring = RT_NULL;
static rt_int32_t *g_breath_filtered_ring = RT_NULL;
static rt_uint16_t g_breath_write_index = 0;
static rt_uint32_t g_breath_sample_count = 0;
static rt_bool_t g_breath_filter_ready = RT_FALSE;
static rt_int32_t g_breath_baseline_q = 0;
static rt_int32_t g_breath_filtered_q = 0;
static rt_tick_t g_breath_last_active_tick = 0;

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

static rt_int32_t breath_abs_i32(rt_int32_t value)
{
    return (value < 0) ? -value : value;
}

static rt_int32_t breath_filtered_sample(rt_int32_t mv)
{
    rt_int32_t signal_q;

    if (!g_breath_filter_ready)
    {
        g_breath_baseline_q = mv * APP_BREATH_Q;
        g_breath_filtered_q = 0;
        g_breath_last_active_tick = rt_tick_get();
        g_breath_filter_ready = RT_TRUE;
        return 0;
    }

    g_breath_baseline_q += ((mv * APP_BREATH_Q - g_breath_baseline_q) *
                            APP_BREATH_BASELINE_NUM) / APP_BREATH_BASELINE_DEN;
    signal_q = mv * APP_BREATH_Q - g_breath_baseline_q;
    g_breath_filtered_q += ((signal_q - g_breath_filtered_q) *
                            APP_BREATH_FILTER_NUM) / APP_BREATH_FILTER_DEN;

    return g_breath_filtered_q / APP_BREATH_Q;
}

static void breath_push_sample(rt_int32_t mv)
{
    rt_int32_t filtered_mv = breath_filtered_sample(mv);

    rt_mutex_take(g_breath_lock, RT_WAITING_FOREVER);
    g_breath_ring[g_breath_write_index] = mv;
    g_breath_filtered_ring[g_breath_write_index] = filtered_mv;
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
    g_breath_filtered_ring = (rt_int32_t *)rt_calloc(APP_BREATH_RING_SIZE, sizeof(rt_int32_t));
    if (g_breath_ring == RT_NULL || g_breath_filtered_ring == RT_NULL)
    {
        if (g_breath_ring != RT_NULL)
        {
            rt_free(g_breath_ring);
        }
        if (g_breath_filtered_ring != RT_NULL)
        {
            rt_free(g_breath_filtered_ring);
        }
        rt_mutex_delete(g_breath_lock);
        g_breath_ring = RT_NULL;
        g_breath_filtered_ring = RT_NULL;
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
        rt_free(g_breath_filtered_ring);
        rt_mutex_delete(g_breath_lock);
        g_breath_ring = RT_NULL;
        g_breath_filtered_ring = RT_NULL;
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

rt_size_t app_breath_copy_recent_filtered(rt_int32_t *mv_buf, rt_size_t max_count)
{
    rt_size_t available;
    rt_size_t count;
    rt_uint16_t oldest;
    rt_size_t start_offset;
    rt_size_t i;

    if (mv_buf == RT_NULL || max_count == 0 || g_breath_lock == RT_NULL || g_breath_filtered_ring == RT_NULL)
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
        mv_buf[i] = g_breath_filtered_ring[index];
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
    rt_int32_t energy_sum = 0;
    rt_size_t last_motion_i = APP_BREATH_RING_SIZE;
    rt_tick_t now;
    rt_tick_t last_active_tick;

    if (stats == RT_NULL)
    {
        return -RT_EINVAL;
    }
    if (g_breath_lock == RT_NULL || g_breath_ring == RT_NULL || g_breath_filtered_ring == RT_NULL)
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
        rt_int32_t filtered_mv = g_breath_filtered_ring[index];

        if (i == 0)
        {
            stats->min_mv = mv;
            stats->max_mv = mv;
            stats->filtered_min_mv = filtered_mv;
            stats->filtered_max_mv = filtered_mv;
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
            if (filtered_mv < stats->filtered_min_mv)
            {
                stats->filtered_min_mv = filtered_mv;
            }
            if (filtered_mv > stats->filtered_max_mv)
            {
                stats->filtered_max_mv = filtered_mv;
            }
        }
        sum += mv;
        energy_sum += breath_abs_i32(filtered_mv);

        if (i > 0 && i + 1 < count)
        {
            rt_uint16_t prev_index = (rt_uint16_t)((oldest + start_offset + i - 1) % APP_BREATH_RING_SIZE);
            rt_uint16_t next_index = (rt_uint16_t)((oldest + start_offset + i + 1) % APP_BREATH_RING_SIZE);
            rt_int32_t prev_filtered = g_breath_filtered_ring[prev_index];
            rt_int32_t next_filtered = g_breath_filtered_ring[next_index];
            rt_int32_t local_min = filtered_mv;
            rt_int32_t local_max = filtered_mv;
            rt_size_t j;

            for (j = (i > 10) ? (i - 10) : 0; j < count && j <= i + 10; j++)
            {
                rt_uint16_t local_index = (rt_uint16_t)((oldest + start_offset + j) % APP_BREATH_RING_SIZE);
                rt_int32_t local_mv = g_breath_filtered_ring[local_index];

                if (local_mv < local_min)
                {
                    local_min = local_mv;
                }
                if (local_mv > local_max)
                {
                    local_max = local_mv;
                }
            }

            if ((last_motion_i == APP_BREATH_RING_SIZE ||
                 i - last_motion_i >= APP_BREATH_MOTION_MIN_GAP) &&
                ((filtered_mv >= prev_filtered && filtered_mv > next_filtered &&
                  filtered_mv - local_min >= APP_BREATH_MOTION_PROM_MV) ||
                 (filtered_mv <= prev_filtered && filtered_mv < next_filtered &&
                  local_max - filtered_mv >= APP_BREATH_MOTION_PROM_MV)))
            {
                stats->motion_count++;
                last_motion_i = i;
            }
        }
    }

    stats->count = count;
    stats->base_mv = sum / (rt_int32_t)count;
    stats->pp_mv = stats->max_mv - stats->min_mv;
    stats->filtered_pp_mv = stats->filtered_max_mv - stats->filtered_min_mv;
    stats->energy_mv = energy_sum / (rt_int32_t)count;
    stats->periodic = (stats->motion_count >= APP_BREATH_PERIODIC_MIN_COUNT &&
                       stats->motion_count <= APP_BREATH_PERIODIC_MAX_COUNT) ? RT_TRUE : RT_FALSE;
    stats->active = (stats->pp_mv >= APP_BREATH_PRESSURE_PP_MV ||
                     stats->filtered_pp_mv >= APP_BREATH_ACTIVE_PP_MV ||
                     stats->energy_mv >= APP_BREATH_ACTIVE_ENERGY_MV) ? RT_TRUE : RT_FALSE;
    if (stats->active && window <= APP_BREATH_ACTIVITY_WINDOW)
    {
        g_breath_last_active_tick = rt_tick_get();
    }
    last_active_tick = g_breath_last_active_tick;
    rt_mutex_release(g_breath_lock);

    now = rt_tick_get();
    stats->apnea_seconds = (rt_int32_t)((now - last_active_tick) / RT_TICK_PER_SECOND);
    if (stats->apnea_seconds >= APP_BREATH_APNEA_SECONDS)
    {
        stats->state = APP_BREATH_STATE_APNEA_SUSPECT;
    }
    else if (stats->apnea_seconds >= APP_BREATH_WEAK_SECONDS)
    {
        stats->state = APP_BREATH_STATE_WEAK_OR_MISSING;
    }
    else
    {
        stats->state = APP_BREATH_STATE_NORMAL;
    }
    return RT_EOK;
}

const char *app_breath_state_name(app_breath_state_t state)
{
    switch (state)
    {
    case APP_BREATH_STATE_NORMAL:
        return "normal";
    case APP_BREATH_STATE_WEAK_OR_MISSING:
        return "weak_or_missing";
    case APP_BREATH_STATE_APNEA_SUSPECT:
        return "apnea_suspect";
    default:
        return "unknown";
    }
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

    rt_kprintf("[app][breath] count=%u base_mv=%d min_mv=%d max_mv=%d pp_mv=%d filtered_pp_mv=%d energy_mv=%d motion_count=%d periodic=%d active=%d apnea_seconds=%d state=%s\r\n",
               (unsigned int)stats.count,
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
