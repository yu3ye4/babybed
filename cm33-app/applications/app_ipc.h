#ifndef APP_IPC_H__
#define APP_IPC_H__

#include <rtthread.h>
#include <stdint.h>

rt_err_t app_ipc_init(void);
rt_err_t app_ipc_put_uplink(const char *text, rt_size_t len);
void     app_ipc_debug_mark(const char *text, uint32_t code);
void     app_ipc_get_commands(void);

#endif /* APP_IPC_H__ */
