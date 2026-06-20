#include "app_vision_uvc.h"

#include <rtdevice.h>
#include <board.h>
#include <stdlib.h>
#include <string.h>

#include "usbh_core.h"
#include "usbh_video.h"
#include "usbh_uvc_stream.h"

#define DBG_TAG "app.vision_uvc"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define APP_UVC_SNAP_WIDTH       320U
#define APP_UVC_SNAP_HEIGHT      240U
#define APP_UVC_MAX_WIDTH        320U
#define APP_UVC_MAX_HEIGHT       240U
#define APP_UVC_BYTES_PER_PIXEL  2U
#define APP_UVC_FRAME_BUF_SIZE   (APP_UVC_MAX_WIDTH * APP_UVC_MAX_HEIGHT * APP_UVC_BYTES_PER_PIXEL)
#define APP_UVC_FRAME_BUF_COUNT  4U
#define APP_UVC_SNAP_TIMEOUT_MS  3000U
#define APP_UVC_WORKER_STACK     8192U
#define APP_UVC_WORKER_PRIORITY  22U
#define APP_UVC_WORKER_TICK      10U

typedef struct
{
    rt_bool_t valid;
    rt_uint16_t width;
    rt_uint16_t height;
    rt_uint8_t format;
    rt_uint32_t frame_count;
    rt_uint32_t drop_count;
    rt_uint32_t frame_len;
    rt_uint32_t min_v;
    rt_uint32_t max_v;
    rt_uint32_t mean_v;
    rt_uint32_t nonzero;
    rt_tick_t tick;
} app_vision_uvc_stats_t;

static struct usbh_video *g_uvc_video;
static rt_mutex_t g_uvc_lock;
static rt_bool_t g_uvc_started;
static rt_bool_t g_uvc_stream_created;
static rt_thread_t g_uvc_worker;
static rt_uint16_t g_uvc_width = APP_UVC_SNAP_WIDTH;
static rt_uint16_t g_uvc_height = APP_UVC_SNAP_HEIGHT;
static app_vision_uvc_stats_t g_uvc_stats;

static rt_uint8_t g_uvc_frame_buffer[APP_UVC_FRAME_BUF_COUNT][APP_UVC_FRAME_BUF_SIZE]
    __attribute__((aligned(32), section(".bss.usbh_uvc_frame")));
static struct usbh_videoframe g_uvc_frame_pool[APP_UVC_FRAME_BUF_COUNT];

void usbh_video_run(struct usbh_video *video_class)
{
    LOG_I("UVC device connected");

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
    }

    g_uvc_video = video_class;

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_release(g_uvc_lock);
    }

    usbh_video_list_info(video_class);
}

void usbh_video_stop(struct usbh_video *video_class)
{
    LOG_I("UVC device disconnected");

    usbh_video_stream_stop();

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
    }

    if (g_uvc_video == video_class)
    {
        g_uvc_video = RT_NULL;
    }
    g_uvc_started = RT_FALSE;
    g_uvc_stream_created = RT_FALSE;

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_release(g_uvc_lock);
    }
}

static rt_bool_t app_vision_uvc_is_connected(void)
{
    return g_uvc_video != RT_NULL ? RT_TRUE : RT_FALSE;
}

static void app_vision_uvc_init_frame_pool(void)
{
    for (rt_size_t i = 0; i < APP_UVC_FRAME_BUF_COUNT; i++)
    {
        memset(&g_uvc_frame_pool[i], 0, sizeof(g_uvc_frame_pool[i]));
        g_uvc_frame_pool[i].frame_buf = g_uvc_frame_buffer[i];
        g_uvc_frame_pool[i].frame_bufsize = APP_UVC_FRAME_BUF_SIZE;
    }
}

static const char *app_vision_uvc_format_name(rt_uint8_t format)
{
    return format == USBH_VIDEO_FORMAT_MJPEG ? "mjpeg" : "uncompressed";
}

static int app_vision_uvc_parse_options(int argc, char **argv, rt_uint8_t *format, rt_uint16_t *width, rt_uint16_t *height)
{
    if (argc >= 2)
    {
        *format = (rt_uint8_t)atoi(argv[1]);
        if (*format != USBH_VIDEO_FORMAT_UNCOMPRESSED && *format != USBH_VIDEO_FORMAT_MJPEG)
        {
            rt_kprintf("[app][vision_uvc] invalid format: %u, use 0=uncompressed or 1=mjpeg\r\n", *format);
            return -RT_EINVAL;
        }
    }
    if (argc >= 4)
    {
        *width = (rt_uint16_t)atoi(argv[2]);
        *height = (rt_uint16_t)atoi(argv[3]);
    }

    if ((*width == 0U) || (*height == 0U) || ((rt_uint32_t)(*width) * (*height) * APP_UVC_BYTES_PER_PIXEL > APP_UVC_FRAME_BUF_SIZE))
    {
        rt_kprintf("[app][vision_uvc] invalid size: %ux%u, max raw buffer is %ux%u\r\n",
                   *width,
                   *height,
                   APP_UVC_MAX_WIDTH,
                   APP_UVC_MAX_HEIGHT);
        return -RT_EINVAL;
    }

    return 0;
}

static int app_vision_uvc_ensure_stream(void)
{
    int ret;

    if (g_uvc_stream_created == RT_TRUE)
    {
        return 0;
    }

    app_vision_uvc_init_frame_pool();
    ret = usbh_video_stream_create(g_uvc_frame_pool, APP_UVC_FRAME_BUF_COUNT);
    if (ret < 0)
    {
        LOG_E("create UVC stream failed: %d", ret);
        return ret;
    }

    g_uvc_stream_created = RT_TRUE;
    return 0;
}

static void app_vision_uvc_update_stats(struct usbh_videoframe *frame)
{
    rt_uint8_t min_v = 255;
    rt_uint8_t max_v = 0;
    rt_uint32_t nonzero = 0;
    rt_uint64_t sum = 0;
    rt_uint32_t mean = 0;
    rt_uint32_t frame_size = frame->frame_size;
    const rt_uint8_t *buf = frame->frame_buf;

    for (rt_uint32_t i = 0; i < frame_size; i++)
    {
        rt_uint8_t v = buf[i];

        if (v < min_v)
        {
            min_v = v;
        }
        if (v > max_v)
        {
            max_v = v;
        }
        if (v != 0U)
        {
            nonzero++;
        }
        sum += v;
    }

    if (frame_size > 0U)
    {
        mean = (rt_uint32_t)(sum / frame_size);
    }

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
    }

    g_uvc_stats.valid = RT_TRUE;
    g_uvc_stats.width = g_uvc_width;
    g_uvc_stats.height = g_uvc_height;
    g_uvc_stats.format = (rt_uint8_t)frame->frame_format;
    g_uvc_stats.frame_count++;
    g_uvc_stats.frame_len = frame_size;
    g_uvc_stats.min_v = min_v;
    g_uvc_stats.max_v = max_v;
    g_uvc_stats.mean_v = mean;
    g_uvc_stats.nonzero = nonzero;
    g_uvc_stats.tick = rt_tick_get();

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_release(g_uvc_lock);
    }
}

static void app_vision_uvc_keep_latest(struct usbh_videoframe **frame)
{
    struct usbh_videoframe *latest = *frame;
    struct usbh_videoframe *queued = RT_NULL;

    while (usbh_video_stream_dequeue(&queued, 0) == 0)
    {
        usbh_video_stream_enqueue(latest);
        latest = queued;

        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
        }
        g_uvc_stats.drop_count++;
        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_release(g_uvc_lock);
        }
    }

    *frame = latest;
}

static void app_vision_uvc_print_stats(const app_vision_uvc_stats_t *stats)
{
    if (stats->valid == RT_FALSE)
    {
        rt_kprintf("[app][vision_uvc] stream=%s waiting for frame\r\n", g_uvc_started ? "on" : "off");
        return;
    }

    rt_kprintf("[app][vision_uvc] stream=%s width=%u height=%u format=%s frames=%u drops=%u frame_len=%u min=%u max=%u mean=%u nonzero=%u age_ms=%u\r\n",
               g_uvc_started ? "on" : "off",
               stats->width,
               stats->height,
               app_vision_uvc_format_name(stats->format),
               stats->frame_count,
               stats->drop_count,
               stats->frame_len,
               stats->min_v,
               stats->max_v,
               stats->mean_v,
               stats->nonzero,
               (rt_uint32_t)((rt_tick_get() - stats->tick) * 1000U / RT_TICK_PER_SECOND));
}

static void app_vision_uvc_worker_entry(void *parameter)
{
    struct usbh_videoframe *frame = RT_NULL;
    int ret;

    RT_UNUSED(parameter);

    while (g_uvc_started == RT_TRUE)
    {
        ret = usbh_video_stream_dequeue(&frame, RT_TICK_PER_SECOND);
        if (ret < 0 || frame == RT_NULL)
        {
            continue;
        }

        app_vision_uvc_keep_latest(&frame);
        app_vision_uvc_update_stats(frame);
        usbh_video_stream_enqueue(frame);
    }

    g_uvc_worker = RT_NULL;
}

static int app_vision_uvc_start_stream(rt_uint8_t format, rt_uint16_t width, rt_uint16_t height)
{
    int ret;

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
    }

    if (app_vision_uvc_is_connected() == RT_FALSE)
    {
        rt_kprintf("[app][vision_uvc] no UVC device connected\r\n");
        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_release(g_uvc_lock);
        }
        return -RT_ERROR;
    }

    if (g_uvc_started == RT_TRUE)
    {
        rt_kprintf("[app][vision_uvc] UVC stream already running\r\n");
        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_release(g_uvc_lock);
        }
        return -RT_EBUSY;
    }

    ret = app_vision_uvc_ensure_stream();
    if (ret < 0)
    {
        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_release(g_uvc_lock);
        }
        return ret;
    }

    memset(&g_uvc_stats, 0, sizeof(g_uvc_stats));
    g_uvc_width = width;
    g_uvc_height = height;
    g_uvc_started = RT_TRUE;

    g_uvc_worker = rt_thread_create("uvc_frm",
                                    app_vision_uvc_worker_entry,
                                    RT_NULL,
                                    APP_UVC_WORKER_STACK,
                                    APP_UVC_WORKER_PRIORITY,
                                    APP_UVC_WORKER_TICK);
    if (g_uvc_worker == RT_NULL)
    {
        g_uvc_started = RT_FALSE;
        rt_kprintf("[app][vision_uvc] create frame worker failed\r\n");
        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_release(g_uvc_lock);
        }
        return -RT_ENOMEM;
    }

    rt_thread_startup(g_uvc_worker);

    rt_kprintf("[app][vision_uvc] stream start width=%u height=%u format=%s\r\n",
               width,
               height,
               app_vision_uvc_format_name(format));

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_release(g_uvc_lock);
    }

    usbh_video_stream_start(width, height, format);
    return 0;
}

static int app_vision_uvc_stop_stream(void)
{
    if (g_uvc_started == RT_FALSE)
    {
        rt_kprintf("[app][vision_uvc] UVC stream not running\r\n");
        return 0;
    }

    g_uvc_started = RT_FALSE;
    rt_thread_mdelay(100);
    usbh_video_stream_stop();
    rt_kprintf("[app][vision_uvc] stream stopped\r\n");
    return 0;
}

static int vision_uvc_info(int argc, char **argv)
{
    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
    }

    if (app_vision_uvc_is_connected() == RT_FALSE)
    {
        rt_kprintf("[app][vision_uvc] no UVC device connected\r\n");
    }
    else
    {
        rt_kprintf("[app][vision_uvc] connected /dev/video%u\r\n", g_uvc_video->minor);
        usbh_video_list_info(g_uvc_video);
    }

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_release(g_uvc_lock);
    }

    return 0;
}
MSH_CMD_EXPORT(vision_uvc_info, Print USB UVC camera information);

static int vision_uvc_snap(int argc, char **argv)
{
    rt_uint8_t format = USBH_VIDEO_FORMAT_UNCOMPRESSED;
    rt_uint16_t width = APP_UVC_SNAP_WIDTH;
    rt_uint16_t height = APP_UVC_SNAP_HEIGHT;
    int ret;
    rt_tick_t deadline;

    ret = app_vision_uvc_parse_options(argc, argv, &format, &width, &height);
    if (ret < 0)
    {
        return ret;
    }

    if (g_uvc_started == RT_FALSE)
    {
        ret = app_vision_uvc_start_stream(format, width, height);
        if (ret < 0)
        {
            return ret;
        }
    }

    deadline = rt_tick_get() + rt_tick_from_millisecond(APP_UVC_SNAP_TIMEOUT_MS);
    while (rt_tick_get() < deadline)
    {
        app_vision_uvc_stats_t stats;

        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
        }

        stats = g_uvc_stats;

        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_release(g_uvc_lock);
        }

        if (stats.valid == RT_TRUE)
        {
            app_vision_uvc_print_stats(&stats);
            return 0;
        }

        rt_thread_mdelay(20);
    }

    rt_kprintf("[app][vision_uvc] snap waiting for first frame timed out; stream remains running\r\n");
    return -RT_ETIMEOUT;
}
MSH_CMD_EXPORT(vision_uvc_snap, Start UVC stream and print first frame stats);

static int vision_uvc_start(int argc, char **argv)
{
    rt_uint8_t format = USBH_VIDEO_FORMAT_UNCOMPRESSED;
    rt_uint16_t width = APP_UVC_SNAP_WIDTH;
    rt_uint16_t height = APP_UVC_SNAP_HEIGHT;
    int ret;

    ret = app_vision_uvc_parse_options(argc, argv, &format, &width, &height);
    if (ret < 0)
    {
        return ret;
    }

    return app_vision_uvc_start_stream(format, width, height);
}
MSH_CMD_EXPORT(vision_uvc_start, Start UVC diagnostic stream);

static int vision_uvc_stop(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    return app_vision_uvc_stop_stream();
}
MSH_CMD_EXPORT(vision_uvc_stop, Stop UVC diagnostic stream);

static int vision_uvc_stats(int argc, char **argv)
{
    app_vision_uvc_stats_t stats;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
    }

    stats = g_uvc_stats;

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_release(g_uvc_lock);
    }

    app_vision_uvc_print_stats(&stats);
    return 0;
}
MSH_CMD_EXPORT(vision_uvc_stats, Print latest UVC frame stats);

rt_err_t app_vision_uvc_init(void)
{
#if defined(RT_USING_CHERRYUSB) && defined(RT_CHERRYUSB_HOST) && defined(RT_CHERRYUSB_HOST_VIDEO)
    if (g_uvc_lock == RT_NULL)
    {
        g_uvc_lock = rt_mutex_create("uvclock", RT_IPC_FLAG_PRIO);
        if (g_uvc_lock == RT_NULL)
        {
            LOG_E("create UVC lock failed");
            return -RT_ENOMEM;
        }
    }

    usbh_initialize(0, USBHS_BASE, RT_NULL);
    LOG_I("USB UVC host init ok");
    return RT_EOK;
#else
    LOG_W("USB UVC host disabled");
    return -RT_ERROR;
#endif
}
