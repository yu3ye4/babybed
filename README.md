# Babybed Edge

Dual-core RT-Thread project for a babybed safety node on PSOC Edge MCU.

The system is split across CM33 and CM55:

- `cm33-secure/`: CM33 Secure boot and handoff project.
- `cm33-app/`: CM33 Non-Secure application. It reads AHT20, evaluates risk, and writes telemetry to shared memory.
- `cm55-wifi-mqtt/`: CM55 WiFi/MQTT application. It initializes WHD WiFi, reads CM33 telemetry from shared memory, and publishes MQTT messages.

## Data Flow

```text
CM33_NS sensors/tasks -> shared memory 0x261C0000 -> CM55 WiFi/MQTT -> Mosquitto broker
```

Shared memory address:

```c
0x261C0000
```

MQTT topics:

```text
babybed/babybed_01/telemetry
babybed/babybed_01/command
```

## AHT20

The AHT20 API used by the CM33 application comes from:

```text
cm33-app/packages/aht10-latest
```

The CM33 application wraps the package API in `applications/app_aht20.c`.

## AI Model

The YOLOv5n 320x320 int8 TFLite model is included in:

```text
cm33-app/models/baby_yolov5n_int8.tflite
```

For firmware builds, the same model is embedded as a C array in:

```text
cm33-app/applications/ai_model.c
cm33-app/applications/ai_model.h
```

At CM33 startup, the app logs the linked model size and expected input shape. The current firmware exposes the model bytes to application code; a TFLite Micro or other embedded inference runtime still needs to be added before the MCU can run YOLO inference locally.

## Broker Configuration

The MQTT broker URI is configured in the CM55 project. Change the broker IP to the active WLAN IPv4 address of the PC running Mosquitto.

For Mosquitto on Windows, make sure it listens on all interfaces:

```conf
listener 1883 0.0.0.0
allow_anonymous true
```

Check listener:

```powershell
netstat -ano | findstr :1883
```

Expected:

```text
0.0.0.0:1883 LISTENING
```

## Bring-Up Order

1. Build and flash `cm33-secure`.
2. Build and flash `cm33-app`.
3. Build and flash `cm55-wifi-mqtt`.
4. Verify CM33 telemetry first:

```text
[aht20] package init ok on i2c1 addr=0x38
[env] sample temp=... humi=...%
[fusion] recv env ...
[uplink] ts=...,temp=...,humi=...
```

5. Verify CM55 WiFi/MQTT:

```text
WLAN Firmware
WLAN CLM
[I/WLAN.dev] wlan init success
[I/mqtt] MQTT server connect success.
[mqtt] online, subscriptions ready
```

6. Subscribe on the PC:

```powershell
mosquitto_sub -h 127.0.0.1 -p 1883 -t "babybed/babybed_01/telemetry" -v
```

## UART Note

The CM33 and CM55 projects both use `uart2` by default. When both cores run, logs can interleave or appear corrupted. For debugging, run one core at a time or route consoles to separate UARTs.

## Debug Notes

Detailed WiFi, SDIO, MQTT, IPC, and AHT20 bring-up notes are in:

```text
docs/bringup.md
```

## License

Apache-2.0. See `LICENSE`.
