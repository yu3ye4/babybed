#ifndef SENSOR_TEMP_HUMI_H
#define SENSOR_TEMP_HUMI_H

int sensor_temp_humi_init(void);
int sensor_temp_humi_read(float *temp, float *humi);

#endif
