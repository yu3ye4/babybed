#include "app_ai_mode.h"

static app_ai_mode_t g_ai_mode = APP_AI_MODE_EXTERNAL_VISION;

void app_ai_mode_init(void)
{
    g_ai_mode = APP_AI_MODE_EXTERNAL_VISION;
    rt_kprintf("[ai] mode=%s\r\n", app_ai_mode_name(g_ai_mode));
}

void app_ai_mode_set(app_ai_mode_t mode)
{
    if (mode > APP_AI_MODE_LOCAL_MODEL)
    {
        rt_kprintf("[ai] invalid mode id=%d\r\n", (int)mode);
        return;
    }

    if (g_ai_mode != mode)
    {
        g_ai_mode = mode;
        rt_kprintf("[ai] mode=%s\r\n", app_ai_mode_name(g_ai_mode));
    }
}

app_ai_mode_t app_ai_mode_get(void)
{
    return g_ai_mode;
}

const char *app_ai_mode_name(app_ai_mode_t mode)
{
    switch (mode)
    {
    case APP_AI_MODE_OFF:
        return "off";
    case APP_AI_MODE_EXTERNAL_VISION:
        return "external";
    case APP_AI_MODE_LOCAL_MODEL:
        return "local";
    default:
        return "unknown";
    }
}

#ifdef RT_USING_MSH
static void ai_mode(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("[ai] mode=%s\r\n", app_ai_mode_name(app_ai_mode_get()));
        rt_kprintf("[ai] usage: ai_mode off|external|local\r\n");
        return;
    }

    if (rt_strcmp(argv[1], "off") == 0)
    {
        app_ai_mode_set(APP_AI_MODE_OFF);
    }
    else if (rt_strcmp(argv[1], "external") == 0 || rt_strcmp(argv[1], "vision") == 0)
    {
        app_ai_mode_set(APP_AI_MODE_EXTERNAL_VISION);
    }
    else if (rt_strcmp(argv[1], "local") == 0 || rt_strcmp(argv[1], "ai") == 0)
    {
        app_ai_mode_set(APP_AI_MODE_LOCAL_MODEL);
    }
    else
    {
        rt_kprintf("[ai] unknown mode: %s\r\n", argv[1]);
    }
}
MSH_CMD_EXPORT(ai_mode, set babybed AI mode);
#endif
