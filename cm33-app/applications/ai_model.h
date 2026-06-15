#ifndef AI_MODEL_H__
#define AI_MODEL_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AI_MODEL_INPUT_WIDTH      320U
#define AI_MODEL_INPUT_HEIGHT     320U
#define AI_MODEL_INPUT_CHANNELS   3U

extern const rt_uint8_t g_ai_model_data[];
extern const rt_size_t g_ai_model_size;

const rt_uint8_t *ai_model_get_data(void);
rt_size_t ai_model_get_size(void);

#ifdef __cplusplus
}
#endif

#endif /* AI_MODEL_H__ */

