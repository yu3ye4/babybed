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
#define APP_UVC_BYTES_PER_PIXEL  2U
#define APP_UVC_FRAME_BUF_SIZE   (APP_UVC_SNAP_WIDTH * APP_UVC_SNAP_HEIGHT * APP_UVC_BYTES_PER_PIXEL)
#define APP_UVC_FRAME_BUF_COUNT  2U
#define APP_UVC_SNAP_TIMEOUT_MS  3000U

static struct usbh_video *g_uvc_video;
static rt_mutex_t g_uvc_lock;
static rt_bool_t g_uvc_started;
static rt_bool_t g_uvc_stream_created;

static rt_uint8_t g_uvc_frame_buffer[APP_UVC_FRAME_BUF_COUNT][APP_UVC_FRAME_BUF_SIZE]
    __attribute__((aligned(32), section(".m33_m55_shared_hyperram")));
static struct usbh_videoframe g_uvc_frame_pool[APP_UVC_FRAME_BUF_COUNT];

void usbh_video_run(struct usbh_video *video_class)
{
    LOG_I("UVC device connected");

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
    }

    g_uvc_video = video_class;
    g_uvc_stream_created = RT_FALSE;

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
        g_uvc_frame_pool[i].frame_buf = g_uvc_frame_buffer[i];
        g_uvc_frame_pool[i].frame_bufsize = APP_UVC_FRAME_BUF_SIZE;
        g_uvc_frame_pool[i].frame_format = USBH_VIDEO_FORMAT_UNCOMPRESSED;
        g_uvc_frame_pool[i].frame_size = 0;
    }
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

static void app_vision_uvc_print_stats(struct usbh_videoframe *frame)
{
    rt_uint8_t min_v = 255;
    rt_uint8_t max_v = 0;
    rt_uint32_t nonzero = 0;
    rt_uint64_t sum = 0;
    rt_uint32_t mean = 0;
    rt_uint32_t frame_size = frame->frame_size;
    const rt_uint8_t *buf = frame->frame_buf;
    const char *format = frame->frame_format == USBH_VIDEO_FORMAT_MJPEG ? "mjpeg" : "uncompressed";

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

    rt_kprintf("[app][vision_uvc] width=%u height=%u format=%s frame_len=%u min=%u max=%u mean=%u nonzero=%u\r\n",
               APP_UVC_SNAP_WIDTH,
               APP_UVC_SNAP_HEIGHT,
               format,
               frame_size,
               min_v,
               max_v,
               mean,
               nonzero);
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
    struct usbh_videoframe *frame = RT_NULL;
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

    g_uvc_started = RT_TRUE;
    rt_kprintf("[app][vision_uvc] snap start width=%u height=%u format=uncompressed\r\n",
               APP_UVC_SNAP_WIDTH,
               APP_UVC_SNAP_HEIGHT);

    usbh_video_stream_start(APP_UVC_SNAP_WIDTH, APP_UVC_SNAP_HEIGHT, USBH_VIDEO_FORMAT_UNCOMPRESSED);

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_release(g_uvc_lock);
    }

    ret = usbh_video_stream_dequeue(&frame, rt_tick_from_millisecond(APP_UVC_SNAP_TIMEOUT_MS));

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_take(g_uvc_lock, RT_WAITING_FOREVER);
    }

    usbh_video_stream_stop();
    g_uvc_started = RT_FALSE;

    if (ret < 0 || frame == RT_NULL)
    {
        rt_kprintf("[app][vision_uvc] snap timeout or failed: %d\r\n", ret);
        if (g_uvc_lock != RT_NULL)
        {
            rt_mutex_release(g_uvc_lock);
        }
        return ret < 0 ? ret : -RT_ETIMEOUT;
    }

    app_vision_uvc_print_stats(frame);
    usbh_video_stream_enqueue(frame);

    if (g_uvc_lock != RT_NULL)
    {
        rt_mutex_release(g_uvc_lock);
    }

    return 0;
}
MSH_CMD_EXPORT(vision_uvc_snap, Capture one 320x240 uncompressed UVC frame and print stats);

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
