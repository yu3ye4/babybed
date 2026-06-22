#include "thing.h"

extern "C" {
#include "../../app_sensor_aht20.h"
}

#define TAG "Environment"

namespace iot
{

static int centi_to_int(rt_int32_t value)
{
    return (int)((value >= 0) ? ((value + 50) / 100) : ((value - 50) / 100));
}

static rt_int32_t abs_i32(rt_int32_t value)
{
    return (value < 0) ? -value : value;
}

class Environment : public Thing
{
public:
    Environment() : Thing("Environment", "temperature and humidity sensor")
    {
        properties_.AddNumberProperty("temperature", "current temperature in Celsius", [this]() -> int {
            rt_int32_t temp_centi;
            rt_int32_t humi_centi;

            if (app_sensor_aht20_read_centi(&temp_centi, &humi_centi) != RT_EOK)
            {
                return 0;
            }

            return centi_to_int(temp_centi);
        });

        properties_.AddNumberProperty("humidity", "current relative humidity percent", [this]() -> int {
            rt_int32_t temp_centi;
            rt_int32_t humi_centi;

            if (app_sensor_aht20_read_centi(&temp_centi, &humi_centi) != RT_EOK)
            {
                return 0;
            }

            return centi_to_int(humi_centi);
        });

        methods_.AddMethod("Refresh", "refresh current temperature and humidity", ParameterList(),
                           [this](const ParameterList &) {
            rt_int32_t temp_centi;
            rt_int32_t humi_centi;

            if (app_sensor_aht20_read_centi(&temp_centi, &humi_centi) == RT_EOK)
            {
                rt_kprintf("[%s] temp=%d.%02dC humi=%d.%02d%%\n",
                           TAG,
                           temp_centi / 100,
                           abs_i32(temp_centi % 100),
                           humi_centi / 100,
                           abs_i32(humi_centi % 100));
            }
            else
            {
                rt_kprintf("[%s] read failed\n", TAG);
            }
        });
    }
};

} // namespace iot

DECLARE_THING(Environment);
