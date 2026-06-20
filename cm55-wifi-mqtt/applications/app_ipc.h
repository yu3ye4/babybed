#ifndef APP_IPC_H__
#define APP_IPC_H__

#include <rtthread.h>

rt_err_t app_ipc_init(void);
rt_err_t app_ipc_read_uplink(char *buf, rt_size_t size);
void     app_ipc_put_command(const char *key, rt_int32_t val_centi);
void     app_ipc_put_ai_mode(rt_int32_t mode);

#endif /* APP_IPC_H__ */
