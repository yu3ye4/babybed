# BabyBed MQTT Web Console

电脑端 Web 控制台，用于接收设备 MQTT telemetry、保存历史、显示实时状态、发送阈值命令，并可选调用 LLM 做分析。

## 运行

```powershell
cd D:\RT-ThreadStudio\workspace\Edgi_Talk_M55_WIFI\pc_mqtt_console
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
Copy-Item .env.example .env
python -m uvicorn app:app --host 0.0.0.0 --port 8000
```

打开浏览器：

```text
http://127.0.0.1:8000
```

## MQTT Topic

默认配置：

```text
telemetry: babybed/babybed_01/telemetry
command:   babybed/babybed_01/command
analysis:  babybed/babybed_01/ai/analysis
alert:     babybed/babybed_01/ai/alert
```

如果 broker IP 不是 `192.168.43.9`，修改 `.env` 里的 `MQTT_HOST`。

## LLM

不配置 `LLM_API_KEY` 时，控制台会使用本地规则生成分析结果。

接 OpenAI：

```powershell
$env:LLM_API_KEY="你的 API key"
$env:LLM_MODEL="gpt-4.1-mini"
```

接 DeepSeek，写入 `.env`：

```text
LLM_API_KEY=你的 DeepSeek API key
LLM_BASE_URL=https://api.deepseek.com
LLM_MODEL=deepseek-chat
```

## 设备数据格式

当前兼容这种上行格式：

```text
ts=123456,temp=26.31C,humi=58.20%,smoke=0ppm,risk=1,score=25,reason=face_stable_detected,vision=on,face=1,face_stable=1,cx=80,cy=60,w=30,h=40
```

后续建议把设备侧 payload 改为 JSON，电脑端和手机端会更容易解析。
