import asyncio
import json
import os
import time
from collections import deque
from pathlib import Path
from threading import Lock
from typing import Any

import paho.mqtt.client as mqtt
from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from openai import OpenAI
from pydantic import BaseModel


BASE_DIR = Path(__file__).resolve().parent
STATIC_DIR = BASE_DIR / "static"
DATA_DIR = BASE_DIR / "data"
HISTORY_FILE = DATA_DIR / "telemetry.jsonl"

load_dotenv(BASE_DIR / ".env")

MQTT_HOST = os.getenv("MQTT_HOST", "192.168.43.9")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_CLIENT_ID = os.getenv("MQTT_CLIENT_ID", "pc_babybed_console")
MQTT_TELEMETRY_TOPIC = os.getenv("MQTT_TELEMETRY_TOPIC", "babybed/babybed_01/telemetry")
MQTT_COMMAND_TOPIC = os.getenv("MQTT_COMMAND_TOPIC", "babybed/babybed_01/command")
MQTT_AI_ANALYSIS_TOPIC = os.getenv("MQTT_AI_ANALYSIS_TOPIC", "babybed/babybed_01/ai/analysis")
MQTT_AI_ALERT_TOPIC = os.getenv("MQTT_AI_ALERT_TOPIC", "babybed/babybed_01/ai/alert")
LLM_API_KEY = os.getenv("LLM_API_KEY") or os.getenv("OPENAI_API_KEY", "")
LLM_BASE_URL = os.getenv("LLM_BASE_URL") or os.getenv("OPENAI_BASE_URL", "")
LLM_MODEL = os.getenv("LLM_MODEL") or os.getenv("OPENAI_MODEL", "gpt-4.1-mini")

DATA_DIR.mkdir(exist_ok=True)

app = FastAPI(title="BabyBed MQTT Web Console")
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")

history: deque[dict[str, Any]] = deque(maxlen=500)
state_lock = Lock()
mqtt_connected = False
mqtt_client: mqtt.Client | None = None
analysis_cache: dict[str, Any] = {
    "mode": "none",
    "text": "暂无分析结果",
    "structured": None,
    "created_at": None,
}


class ThresholdCommand(BaseModel):
    key: str
    value: float


def parse_metric_value(value: str) -> float | int | str:
    raw = value.strip()
    lowered = raw.lower()

    for suffix in ("ppm", "c", "%"):
        if lowered.endswith(suffix):
            raw = raw[: -len(suffix)]
            break

    try:
        num = float(raw)
    except ValueError:
        return value.strip()

    if num.is_integer():
        return int(num)
    return num


def parse_payload(text: str) -> dict[str, Any]:
    text = text.strip()
    if not text:
        return {}

    if text.startswith("{"):
        try:
            parsed = json.loads(text)
            return parsed if isinstance(parsed, dict) else {"value": parsed}
        except json.JSONDecodeError:
            pass

    parsed: dict[str, Any] = {}
    for item in text.split(","):
        if "=" not in item:
            continue
        key, value = item.split("=", 1)
        parsed[key.strip()] = parse_metric_value(value)
    return parsed


def normalize_record(topic: str, payload: bytes) -> dict[str, Any]:
    raw = payload.decode("utf-8", errors="replace")
    data = parse_payload(raw)
    risk = int(data.get("risk", 0) or 0)
    score = int(data.get("score", 0) or 0)

    return {
        "recv_time": time.time(),
        "topic": topic,
        "raw": raw,
        "data": data,
        "risk": risk,
        "score": score,
        "alert": risk >= 2 or score >= 55,
    }


def append_history(record: dict[str, Any]) -> None:
    line = json.dumps(record, ensure_ascii=False)
    with state_lock:
        history.append(record)
    with HISTORY_FILE.open("a", encoding="utf-8") as f:
        f.write(line + "\n")


def load_history() -> None:
    if not HISTORY_FILE.exists():
        return

    lines = HISTORY_FILE.read_text(encoding="utf-8").splitlines()[-500:]
    with state_lock:
        history.clear()
        for line in lines:
            try:
                history.append(json.loads(line))
            except json.JSONDecodeError:
                continue


def build_rule_analysis(records: list[dict[str, Any]], lang: str = "zh") -> str:
    if not records:
        return "No telemetry data is available." if lang == "en" else "暂无 telemetry 数据。"

    latest = records[-1]
    data = latest.get("data", {})
    risk = latest.get("risk", 0)
    score = latest.get("score", 0)
    temp = data.get("temp", "未知")
    humi = data.get("humi", "未知")
    vision = data.get("vision", "未知")
    face = data.get("face", "未知")
    reason = data.get("reason", "未知")

    high_risk_count = sum(1 for item in records if item.get("risk", 0) >= 2)
    face_seen_count = sum(1 for item in records if item.get("data", {}).get("face") == 1)

    level_text = "正常"
    if risk >= 3:
        level_text = "紧急"
    elif risk >= 2:
        level_text = "警告"
    elif risk >= 1:
        level_text = "关注"

    if lang == "en":
        level_text_en = {
            "正常": "normal",
            "紧急": "emergency",
            "警告": "warning",
            "关注": "attention",
        }.get(level_text, "unknown")
        return (
            f"Current status is {level_text_en}, risk={risk}, score={score}. "
            f"Latest temperature is {temp}, humidity is {humi}, vision link is {vision}, "
            f"face detection is {face}, reason is {reason}. "
            f"In the latest {len(records)} records, high-risk samples: {high_risk_count}, "
            f"face detected samples: {face_seen_count}. "
            "Check the environment, verify the vision module, and keep local risk detection as the source of real-time alerts."
        )

    return (
        f"当前状态为{level_text}，risk={risk}，score={score}。"
        f"最新温度 {temp}，湿度 {humi}，视觉链路 {vision}，人脸检测 {face}，原因 {reason}。"
        f"最近 {len(records)} 条数据中，高风险样本 {high_risk_count} 条，人脸出现 {face_seen_count} 条。"
        "建议优先确认环境温湿度是否异常、视觉模块是否在线，并保留本地风险判断作为告警依据。"
    )


def build_rule_structured_analysis(records: list[dict[str, Any]], lang: str = "zh") -> dict[str, Any]:
    if not records:
        if lang == "en":
            return {
                "risk_summary": "No telemetry data is available, so the current status cannot be assessed.",
                "likely_causes": ["Insufficient data"],
                "parent_actions": ["Confirm the device is powered on", "Confirm MQTT is connected", "Wait for new telemetry data"],
                "urgency": "unknown",
                "disclaimer": "This suggestion does not replace on-site caregiver judgment or medical advice.",
            }
        return {
            "risk_summary": "暂无 telemetry 数据，无法判断当前状态。",
            "likely_causes": ["数据不足"],
            "parent_actions": ["确认设备已开机", "确认 MQTT 连接正常", "等待新的监测数据"],
            "urgency": "unknown",
            "disclaimer": "本建议不能替代监护人现场判断或医疗建议。",
        }

    latest = records[-1]
    data = latest.get("data", {})
    risk = latest.get("risk", 0)
    score = latest.get("score", 0)
    reason = str(data.get("reason", "unknown"))
    vision = data.get("vision", "unknown")

    urgency = "normal"
    if risk >= 3:
        urgency = "emergency"
    elif risk >= 2:
        urgency = "warning"
    elif risk >= 1:
        urgency = "attention"

    causes = [reason]
    if vision == "off":
        causes.append("Vision module is offline, so face and posture status cannot be confirmed" if lang == "en" else "视觉模块离线，无法确认人脸和姿态状态")

    actions = ["Check the baby in person", "Check temperature, humidity, and the surrounding environment"] if lang == "en" else ["现场确认婴儿状态", "检查温湿度和周边环境"]
    if vision == "off":
        actions.extend(["Check camera power and connection", "Restart the vision module"] if lang == "en" else ["检查摄像头供电和连接", "重启视觉模块"])

    if lang == "en":
        return {
            "risk_summary": f"Current urgency is {urgency}, risk={risk}, score={score}. The main reason is {reason}.",
            "likely_causes": causes,
            "parent_actions": actions,
            "urgency": urgency,
            "disclaimer": "This suggestion does not replace on-site caregiver judgment or medical advice. Handle urgent cases immediately in person.",
        }

    return {
        "risk_summary": f"当前 urgency={urgency}，risk={risk}，score={score}，主要原因是 {reason}。",
        "likely_causes": causes,
        "parent_actions": actions,
        "urgency": urgency,
        "disclaimer": "本建议不能替代监护人现场判断或医疗建议，紧急情况请立即人工处理。",
    }


def parse_llm_json(text: str) -> dict[str, Any] | None:
    cleaned = text.strip()
    if cleaned.startswith("```"):
        cleaned = cleaned.strip("`")
        if cleaned.lower().startswith("json"):
            cleaned = cleaned[4:].strip()

    start = cleaned.find("{")
    end = cleaned.rfind("}")
    if start < 0 or end <= start:
        return None

    try:
        parsed = json.loads(cleaned[start : end + 1])
    except json.JSONDecodeError:
        return None

    if not isinstance(parsed, dict):
        return None

    required = {"risk_summary", "likely_causes", "parent_actions", "urgency", "disclaimer"}
    if not required.issubset(parsed.keys()):
        return None

    return parsed


async def run_llm_analysis(records: list[dict[str, Any]], lang: str = "zh") -> dict[str, Any]:
    lang = "en" if lang == "en" else "zh"
    if not records:
        structured = build_rule_structured_analysis(records, lang)
        return {"mode": "rule", "text": structured["risk_summary"], "structured": structured, "created_at": time.time()}

    if not LLM_API_KEY:
        structured = build_rule_structured_analysis(records, lang)
        return {"mode": "rule", "text": build_rule_analysis(records, lang), "structured": structured, "created_at": time.time()}

    client_kwargs = {"api_key": LLM_API_KEY}
    if LLM_BASE_URL:
        client_kwargs["base_url"] = LLM_BASE_URL
    client = OpenAI(**client_kwargs)
    compact = [
        {
            "time": item.get("recv_time"),
            "risk": item.get("risk"),
            "score": item.get("score"),
            "data": item.get("data"),
        }
        for item in records[-30:]
    ]

    if lang == "en":
        prompt = (
            "You are an infant safety monitoring assistant. Analyze baby-bed telemetry cautiously. "
            "Only use sensor data, risk level, vision status, and common infant safety knowledge. "
            "Do not diagnose illness and do not replace doctors, caregivers, or local real-time device alarms. "
            "If data is insufficient, explicitly say so. "
            "Output only one JSON object. Do not output Markdown or extra explanations. "
            "All JSON field values must be written in English. "
            "The JSON fields are fixed: "
            "risk_summary: string, summarize the current risk; "
            "likely_causes: string[], list likely causes; "
            "parent_actions: string[], list immediate checks or actions for caregivers; "
            "urgency: string, one of normal, attention, warning, emergency, unknown; "
            "disclaimer: string, state that the suggestion does not replace on-site caregiving or medical judgment."
            f"\n\ntelemetry:\n{json.dumps(compact, ensure_ascii=False)}"
        )
    else:
        prompt = (
            "你是婴儿安全监测助手，负责根据婴儿床 telemetry 做谨慎的安全辅助分析。"
            "你只能根据传感器数据、风险等级、视觉状态和常见婴儿护理安全常识给建议。"
            "不要诊断疾病，不要替代医生、监护人或设备本地实时报警。"
            "如果数据不足，要明确说明数据不足。"
            "必须只输出一个 JSON 对象，不要输出 Markdown，不要输出额外解释。"
            "所有 JSON 字段内容必须使用中文。"
            "JSON 字段固定为："
            "risk_summary: string，概括当前风险；"
            "likely_causes: string[]，列出可能原因；"
            "parent_actions: string[]，列出家长可立即执行的检查或操作；"
            "urgency: string，只能是 normal、attention、warning、emergency、unknown 之一；"
            "disclaimer: string，说明建议不能替代现场监护或医疗判断。"
            f"\n\ntelemetry:\n{json.dumps(compact, ensure_ascii=False)}"
        )

    def call_openai() -> str:
        response = client.chat.completions.create(
            model=LLM_MODEL,
            messages=[
                {"role": "system", "content": "你只输出合法 JSON，不输出 Markdown 或解释文字。"},
                {"role": "user", "content": prompt},
            ],
            temperature=0.2,
        )
        return response.choices[0].message.content or ""

    text = await asyncio.to_thread(call_openai)
    structured = parse_llm_json(text)
    if structured is None:
        structured = build_rule_structured_analysis(records, lang)
        if lang == "en":
            structured["risk_summary"] = f"The LLM output could not be parsed as the fixed JSON format. Rule-based fallback is used. Raw output: {text[:300]}"
        else:
            structured["risk_summary"] = f"LLM 输出未能解析为固定 JSON，已使用规则兜底。原始输出：{text[:300]}"
    return {"mode": "llm", "text": structured["risk_summary"], "structured": structured, "created_at": time.time()}


def publish(topic: str, payload: str, qos: int = 1) -> None:
    if mqtt_client is None:
        raise RuntimeError("MQTT client is not initialized")
    result = mqtt_client.publish(topic, payload, qos=qos)
    if result.rc != mqtt.MQTT_ERR_SUCCESS:
        raise RuntimeError(f"MQTT publish failed: {result.rc}")


def on_connect(client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any = None) -> None:
    global mqtt_connected
    mqtt_connected = True
    client.subscribe(MQTT_TELEMETRY_TOPIC, qos=1)


def on_disconnect(client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any = None) -> None:
    global mqtt_connected
    mqtt_connected = False


def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
    record = normalize_record(msg.topic, msg.payload)
    append_history(record)


def start_mqtt() -> None:
    global mqtt_client
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=MQTT_CLIENT_ID)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    client.connect_async(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()
    mqtt_client = client


@app.on_event("startup")
async def startup() -> None:
    load_history()
    start_mqtt()


@app.on_event("shutdown")
async def shutdown() -> None:
    if mqtt_client is not None:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()


@app.get("/")
async def index() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


@app.get("/api/status")
async def status() -> dict[str, Any]:
    with state_lock:
        latest = history[-1] if history else None
        count = len(history)
    return {
        "mqtt_connected": mqtt_connected,
        "mqtt_host": MQTT_HOST,
        "mqtt_port": MQTT_PORT,
        "telemetry_topic": MQTT_TELEMETRY_TOPIC,
        "command_topic": MQTT_COMMAND_TOPIC,
        "history_count": count,
        "latest": latest,
        "analysis": analysis_cache,
    }


@app.get("/api/history")
async def get_history(limit: int = 100) -> dict[str, Any]:
    limit = max(1, min(limit, 500))
    with state_lock:
        records = list(history)[-limit:]
    return {"records": records}


@app.post("/api/command/threshold")
async def set_threshold(command: ThresholdCommand) -> dict[str, Any]:
    allowed = {"temp_min", "temp_max", "humi_min", "humi_max"}
    if command.key not in allowed:
        raise HTTPException(status_code=400, detail=f"key must be one of {sorted(allowed)}")

    payload = f"SET_THRESH {command.key}={command.value:.2f}"
    try:
        publish(MQTT_COMMAND_TOPIC, payload)
    except RuntimeError as exc:
        raise HTTPException(status_code=503, detail=str(exc)) from exc
    return {"topic": MQTT_COMMAND_TOPIC, "payload": payload}


@app.post("/api/analyze")
async def analyze(lang: str = "zh") -> dict[str, Any]:
    global analysis_cache
    with state_lock:
        records = list(history)[-60:]

    analysis_cache = await run_llm_analysis(records, lang)
    publish(MQTT_AI_ANALYSIS_TOPIC, json.dumps(analysis_cache, ensure_ascii=False))

    latest = records[-1] if records else None
    if latest and latest.get("alert"):
        publish(MQTT_AI_ALERT_TOPIC, json.dumps(analysis_cache, ensure_ascii=False))

    return analysis_cache
