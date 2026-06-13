# Bring-Up and Debug Notes

## Known-Good Sequence

Bring up the system one boundary at a time:

```text
CM33_NS sensors/tasks -> shared memory 0x261C0000 -> CM55 WiFi/MQTT -> Mosquitto broker
```

Recommended order:

1. Debug CM33 AHT20 and uplink with CM55 disabled.
2. Debug CM55 WiFi firmware download and WLAN init.
3. Join WiFi and verify IP address.
4. Connect MQTT only after WiFi got IP.
5. Enable both cores and verify telemetry publishing.

## CM33 AHT20 Pipeline

Expected logs:

```text
[aht20] package init ok on i2c1 addr=0x38
[aht20] humidity read ok
[aht20] temperature read ok
[env] sample temp=... humi=...%
[fusion] recv env temp=... humi=...%
[uplink] ts=...,temp=...,humi=...
```

AHT20 is physically present if:

```text
i2c_scan
[i2c] found addr 0x38
```

The application uses the RT-Thread AHT10 package API:

```c
aht10_init()
aht10_read_humidity()
aht10_read_temperature()
```

The package is stored in:

```text
cm33-app/packages/aht10-latest
```

## RT-Thread Message Queue Trap

`rt_mq_send()` returns `RT_EOK` on success.

`rt_mq_recv()` returns the received byte count on success, not `RT_EOK`.

Correct pattern:

```c
rt_ssize_t recv_len = rt_mq_recv(mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
if (recv_len < 0)
{
    continue;
}
```

Wrong pattern:

```c
if (rt_mq_recv(...) != RT_EOK)
{
    continue;
}
```

The wrong pattern discards successful receives.

## CM55 WiFi Firmware

Expected success logs:

```text
WLAN FW download size: 233524 bytes
WLAN MAC Address : ...
WLAN Firmware    : ...
WLAN CLM         : ...
[I/WLAN.dev] wlan init success
```

If chip ID is read but firmware download fails:

```text
chip ID: 55500
Failed to write firmware image
Failed to load wifi firmware
```

Then the WiFi chip is detectable. The likely issue is SDIO host data transfer, not missing firmware.

## SDIO XFER_COMPLETE Issue

Observed diagnostic:

```text
wait transfer complete fail
NormIntSt=0x0000 ErrIntSt=0x0000
PresentState=... BlkCnt=0x0000 ADMA_Err=0x00
```

Interpretation:

- `BlkCnt=0` means the controller consumed all blocks.
- `ErrIntSt=0` and `ADMA_Err=0` mean no hardware error was reported.
- The transfer likely completed, but XFER_COMPLETE was not delivered or was mishandled.

Fix direction:

- In the SDHC wait-complete path, if the semaphore/status wait times out, perform a fallback hardware-state check.
- Treat transfer as complete only if block count is zero and there are no error flags.
- Do not use fallback if CRC, ADMA, or remaining-block errors are present.

## MQTT Broker

Windows Mosquitto should listen on all interfaces:

```conf
listener 1883 0.0.0.0
allow_anonymous true
```

Restart:

```powershell
net stop mosquitto
net start mosquitto
```

Check:

```powershell
netstat -ano | findstr :1883
```

Good:

```text
0.0.0.0:1883 LISTENING
```

Bad for board access:

```text
127.0.0.1:1883 LISTENING
```

## Network Notes

Phone hotspot is the most reliable test network:

```text
PC:    192.168.43.9
Board: 192.168.43.11
```

Campus WiFi may isolate clients even if IPs are in the same larger subnet.

`errno:113` usually means network unreachable, wrong broker IP, AP isolation, or firewall.

## MQTT Numeric IP

If the log shows:

```text
getaddrinfo err: 203 '192.168.43.9'
resolve uri err
```

The MQTT stack is treating a numeric IP as a DNS name. The Paho RT pipe should handle numeric IPv4 hosts with numeric-host logic such as `AI_NUMERICHOST`.

## UART

Both cores use `uart2` by default:

```c
#define RT_CONSOLE_DEVICE_NAME "uart2"
```

If CM33 and CM55 run together, logs can interleave. Disable one core or use separate UARTs when debugging low-level issues.
