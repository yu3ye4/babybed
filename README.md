# BabyBed PSOC Edge Demo

This repository contains the currently working BabyBed demo for PSOC Edge:

- `firmware/Edgi_Talk_M33_Blink_LED`: CM33 project used to start/release the CM55 core.
- `firmware/cm55-integrated`: CM55 project running XiaoZhi, WiFi, WebSocket, MQTT, AHT20 telemetry, baby-cry detection, green LED alert, and soothing music play/pause.
- `pc_mqtt_console`: PC web console that connects to Mosquitto and displays telemetry/alerts.

The current validated flow is:

```text
BlinkLED CM33 starts CM55
CM55 connects WiFi and XiaoZhi WebSocket
CM55 reads AHT20 on i2c1
CM55 publishes MQTT telemetry to the PC broker
CM55 detects baby crying, blinks green LED, publishes alert, and plays/pauses music
PC web console displays telemetry and alerts
```

## 1. Prerequisites

- RT-Thread Studio with the PSOC Edge toolchain installed.
- Mosquitto MQTT broker installed on the PC.
- Python 3.10+ for the web console.
- PSOC Edge board connected to the same WiFi network as the PC.
- The current default WiFi/broker test setup uses the phone hotspot `HiwonderESP` and PC broker IP `192.168.43.9`.

## 2. Start Mosquitto

Make sure Mosquitto listens on all network interfaces, not only `127.0.0.1`.

Example `mosquitto.conf`:

```conf
listener 1883 0.0.0.0
allow_anonymous true
```

Restart Mosquitto as Administrator:

```powershell
net stop mosquitto
net start mosquitto
```

Verify the listener:

```powershell
netstat -ano | findstr :1883
```

Expected:

```text
TCP    0.0.0.0:1883    0.0.0.0:0    LISTENING
```

## 3. Configure Broker IP

Check the PC WLAN IPv4 address:

```powershell
ipconfig
```

If the PC IP is not `192.168.43.9`, edit:

```text
firmware/cm55-integrated/applications/app_mqtt.h
```

Update:

```c
#define APP_MQTT_BROKER_URI "tcp://192.168.43.9:1883"
```

The firmware publishes to:

```text
babybed/babybed_01/telemetry
```

The firmware subscribes to:

```text
babybed/babybed_01/command
```

## 4. Start PC Web Console

```powershell
cd pc_mqtt_console
python -m pip install -r requirements.txt
copy .env.example .env
python -m uvicorn app:app --host 0.0.0.0 --port 8000
```

Open:

```text
http://127.0.0.1:8000
```

The top-right status should show MQTT connected.

For a raw MQTT check:

```powershell
mosquitto_sub -h 127.0.0.1 -p 1883 -t "babybed/babybed_01/#" -v
```

## 5. Build and Flash Firmware

In RT-Thread Studio:

1. Import `firmware/cm55-integrated`.
2. Build `cm55-integrated`.
3. Import `firmware/Edgi_Talk_M33_Blink_LED`.
4. Flash/run `Edgi_Talk_M33_Blink_LED` to start the CM55 firmware.

Use the BlinkLED CM33 project for this demo. Do not use the old `cm33-app` project for the current known-good flow.

## 6. Expected Serial Logs

WiFi and XiaoZhi:

```text
WLAN Firmware
WLAN CLM
[I/WLAN.dev] wlan init success
[I/WLAN.lwip] Got IP address
[I/xz.ws] WebSocket handshake completed
[I/xz.wakeword] Listening for wake words
```

MQTT and AHT20:

```text
[app][aht20] init ok on i2c1 addr=0x38
[app][mqtt] connected
[app][mqtt] publish telemetry ok
```

Baby crying alert:

```text
*** WAKE WORD DETECTED: crying
Baby crying detected
[app][alert] baby crying alert
[app][alert] baby music play/resume
[app][mqtt] publish cry alert ok
[app][alert] baby crying stopped
[app][alert] baby music pause
```

## 7. Web Console Results

Normal telemetry payload example:

```text
temp=28.30,humi=56.79,risk=0,score=0,reason=m55_aht20
```

Baby crying alert payload example:

```text
event=baby_crying,temp=28.30,humi=56.79,risk=3,score=90,reason=baby_crying,confidence=0.92,message=婴儿哭了
```

The web page should update temperature, humidity, risk, and alert history.

## 8. Common Issues

- MQTT not connected on the web page: confirm Mosquitto is running and listening on `0.0.0.0:1883`.
- Firmware cannot connect to MQTT: confirm `APP_MQTT_BROKER_URI` uses the PC WLAN IPv4 address.
- PC can access MQTT but board cannot: check Windows Firewall and make sure PC and board are on the same hotspot/network.
- WiFi works on PC but not on board: check serial logs for `WLAN Firmware`, `WLAN CLM`, and `Got IP address`.
- `getaddrinfo err: 203 '192.168.x.x'`: numeric broker IP handling is required; this project includes the MQTT pipe fix.
- Logs are interleaved or garbled: CM33 and CM55 may both use `uart2`; keep the CM33 BlinkLED project quiet and use CM55 logs for this demo.
- No baby crying alert: confirm wakeword logs show model labels `crying` and `noise`, and keep the microphone listening.

## 9. MQTT Command Test

The board subscribes to:

```text
babybed/babybed_01/command
```

Example command:

```powershell
mosquitto_pub -h 127.0.0.1 -p 1883 -t "babybed/babybed_01/command" -m "SET_THRESH temp_max=37.5"
```

Supported threshold keys:

```text
temp_min
temp_max
humi_min
humi_max
```
