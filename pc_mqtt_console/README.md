# BabyBed MQTT Web Console

This is the PC-side web console for the BabyBed project. It subscribes to MQTT telemetry from the device, shows live status in a browser, stores history, sends threshold commands, and calls an LLM such as DeepSeek for infant-safety analysis.

Repository:

```text
https://github.com/yu3ye4/babybed.git
branch: mix
```

## What It Does

- Subscribes to device telemetry from MQTT.
- Displays temperature, humidity, risk level, score, vision status, and face status.
- Saves telemetry history to `data/telemetry.jsonl`.
- Sends threshold commands back to the device through MQTT.
- Calls an LLM for structured infant-safety analysis.
- Supports Chinese and English UI, and Chinese/English LLM output.

## Local Run

Clone the repository and switch to the `mix` branch:

```powershell
git clone https://github.com/yu3ye4/babybed.git
cd babybed
git checkout mix
cd pc_mqtt_console
```

Create a virtual environment and install dependencies:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

Create local configuration:

```powershell
Copy-Item .env.example .env
```

Start the web console:

```powershell
python -m uvicorn app:app --host 0.0.0.0 --port 8000
```

Open on the same PC:

```text
http://127.0.0.1:8000
```

Open from a phone or another computer on the same LAN:

```text
http://<PC_IP>:8000
```

Example:

```text
http://192.168.43.9:8000
```

If another device cannot open the page, allow TCP port `8000` through Windows Firewall:

```powershell
New-NetFirewallRule -DisplayName "BabyBed Web Console 8000" -Direction Inbound -Protocol TCP -LocalPort 8000 -Action Allow
```

## MQTT Configuration

Edit `.env`:

```text
MQTT_HOST=192.168.43.9
MQTT_PORT=1883
MQTT_CLIENT_ID=pc_babybed_console
MQTT_TELEMETRY_TOPIC=babybed/babybed_01/telemetry
MQTT_COMMAND_TOPIC=babybed/babybed_01/command
MQTT_AI_ANALYSIS_TOPIC=babybed/babybed_01/ai/analysis
MQTT_AI_ALERT_TOPIC=babybed/babybed_01/ai/alert
```

Default topics:

```text
telemetry: babybed/babybed_01/telemetry
command:   babybed/babybed_01/command
analysis:  babybed/babybed_01/ai/analysis
alert:     babybed/babybed_01/ai/alert
```

## How Other People Can Use It

If another person pulls this repository, they can run the same web console locally on their own computer.

They can open:

```text
http://127.0.0.1:8000
```

However, seeing live device data depends on which MQTT broker they connect to.

### Option 1: Use Their Own Device and Broker

They run their own MQTT broker and device, then edit `.env`:

```text
MQTT_HOST=<their_broker_ip>
MQTT_PORT=1883
```

The web console will show their own device data.

### Option 2: Use a Shared Public or Cloud MQTT Broker

Use the same public broker address in both the device firmware and the PC web console:

```text
MQTT_HOST=<public_broker_domain_or_ip>
MQTT_PORT=1883
```

In this mode, your device publishes telemetry to the shared broker, and other users' web consoles can subscribe to the same topics.

This is the recommended collaboration mode if users are not on the same LAN.

### Option 3: Deploy This Web Console to a Server

Deploy `pc_mqtt_console` to a cloud server. Everyone opens the same server URL.

Example architecture:

```text
Device M55 -> MQTT Broker -> Cloud Web Console -> Browser / Phone
```

This is closest to a real product deployment.

## LLM Configuration

If `LLM_API_KEY` is empty, the console uses local rule-based analysis.

### DeepSeek

Edit `.env`:

```text
LLM_API_KEY=<your_deepseek_api_key>
LLM_BASE_URL=https://api.deepseek.com
LLM_MODEL=deepseek-chat
```

### OpenAI-Compatible Providers

Any OpenAI-compatible provider can be used by changing:

```text
LLM_API_KEY=<your_api_key>
LLM_BASE_URL=<provider_base_url>
LLM_MODEL=<model_name>
```

For OpenAI itself, `LLM_BASE_URL` can be left empty:

```text
LLM_API_KEY=<your_openai_api_key>
LLM_BASE_URL=
LLM_MODEL=gpt-4.1-mini
```

## Security Notes

Never commit real API keys.

Commit this file:

```text
.env.example
```

Do not commit this file:

```text
.env
```

If an API key was accidentally pushed to GitHub, revoke it immediately and create a new one.

For public or cloud deployment, do not expose an unauthenticated MQTT broker or web console directly to the Internet.

## Device Payload Format

The current firmware publishes telemetry like this:

```text
ts=123456,temp=26.31C,humi=58.20%,smoke=0ppm,risk=1,score=25,reason=face_stable_detected,vision=on,face=1,face_stable=1,cx=80,cy=60,w=30,h=40
```

The web console parses this format automatically.

Recommended future improvement: change the firmware payload to JSON, which will be easier for the web console, mobile apps, and LLM analysis.

## Threshold Commands

The web console publishes threshold commands to:

```text
babybed/babybed_01/command
```

Command format:

```text
SET_THRESH temp_max=30.00
SET_THRESH temp_min=20.00
SET_THRESH humi_max=80.00
SET_THRESH humi_min=40.00
```

The M55 firmware receives the MQTT command and writes it into shared memory. The M33 firmware reads the command and updates local risk thresholds.
