#ifndef APP_AI_MODE_H__
#define APP_AI_MODE_H__

#include <rtthread.h>

typedef enum
{
    APP_AI_MODE_OFF = 0,
    APP_AI_MODE_EXTERNAL_VISION = 1,
    APP_AI_MODE_LOCAL_MODEL = 2
} app_ai_mode_t;

void app_ai_mode_init(void);
void app_ai_mode_set(app_ai_mode_t mode);
app_ai_mode_t app_ai_mode_get(void);
const char *app_ai_mode_name(app_ai_mode_t mode);

#endif /* APP_AI_MODE_H__ */
