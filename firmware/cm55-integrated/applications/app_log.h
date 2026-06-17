#ifndef APP_LOG_H__
#define APP_LOG_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_LOG_LINE_MAX 256

rt_err_t app_log_init(void);
void app_log_printf(const char *tag, const char *fmt, ...);

#define APP_LOG(tag, fmt, ...) app_log_printf((tag), (fmt), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* APP_LOG_H__ */
