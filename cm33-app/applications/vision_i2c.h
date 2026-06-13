#ifndef VISION_I2C_H__
#define VISION_I2C_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VISION_I2C_DEFAULT_BUS_NAME    "i2c1"
#define VISION_I2C_DEFAULT_ADDR        0x52

typedef struct
{
    rt_uint8_t center_x;
    rt_uint8_t center_y;
    rt_uint8_t width;
    rt_uint8_t height;
    rt_bool_t  found;
    rt_uint32_t ts_ms;
} vision_frame_t;

typedef enum
{
    VISION_LINK_OFFLINE = 0,
    VISION_LINK_ONLINE,
} vision_link_state_t;

typedef struct
{
    vision_link_state_t link;
    rt_bool_t stable_found;
    rt_uint8_t stable_count;
    vision_frame_t last_frame;
} vision_status_t;

rt_err_t vision_i2c_start(void);
rt_err_t vision_i2c_get_status(vision_status_t *out_status);

#ifdef __cplusplus
}
#endif

#endif /* VISION_I2C_H__ */
