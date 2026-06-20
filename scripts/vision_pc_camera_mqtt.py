#!/usr/bin/env python3
"""Publish BabyBed PC camera YOLO posture inference results to MQTT."""

from __future__ import annotations

import argparse
import shutil
import sys
import threading
import time

from vision_pc_camera_yolo import (
    frame_to_rgb,
    open_camera,
    read_frame,
    sleep_until_next,
    warmup_camera,
)
from vision_verify_yolo_tflite import (
    decode_yolo,
    load_classes,
    load_interpreter,
    prepare_image_input,
    print_tensor_details,
    run_inference,
)


DEFAULT_MODEL = "firmware/vision/models/baby_yolov5n_int8.tflite"
DEFAULT_CLASSES = "firmware/vision/models/classes.txt"
DEFAULT_BROKER = "127.0.0.1"
DEFAULT_TOPIC = "babybed/babybed_01/telemetry"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run PC camera YOLO inference and publish BabyBed vision telemetry."
    )
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Path to .tflite model")
    parser.add_argument("--classes", default=DEFAULT_CLASSES, help="Path to classes.txt")
    parser.add_argument("--camera", type=int, default=0, help="OpenCV camera index")
    parser.add_argument("--interval", type=float, default=1.0, help="Seconds between inferences")
    parser.add_argument("--count", type=int, default=0, help="Number of publishes; 0 runs forever")
    parser.add_argument("--warmup", type=int, default=30, help="Frames to read before inference")
    parser.add_argument("--conf", type=float, default=0.25, help="Confidence threshold")
    parser.add_argument("--iou", type=float, default=0.45, help="NMS IoU threshold")
    parser.add_argument("--top-k", type=int, default=5, help="Max detections to keep")
    parser.add_argument("--broker", default=DEFAULT_BROKER, help="MQTT broker host")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--topic", default=DEFAULT_TOPIC, help="MQTT telemetry topic")
    parser.add_argument("--client-id", default="pc_camera_yolo", help="MQTT client id")
    return parser.parse_args()


def import_mqtt():
    try:
        import paho.mqtt.client as mqtt  # type: ignore

        return mqtt
    except Exception as exc:
        raise RuntimeError(
            "paho-mqtt is unavailable. Install it with: pip install paho-mqtt"
        ) from exc


def make_mqtt_client(client_id: str, on_connect, on_disconnect):
    mqtt = import_mqtt()
    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    except AttributeError:
        client = mqtt.Client(client_id=client_id)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    return client


def mqtt_reason_success(reason_code) -> bool:
    if reason_code == 0:
        return True
    value = getattr(reason_code, "value", None)
    if value == 0:
        return True
    return str(reason_code).lower() in {"0", "success"}


def connect_mqtt(host: str, port: int, client_id: str):
    connected = threading.Event()
    failed: list[str] = []

    def on_connect(client, userdata, flags, reason_code, properties=None):
        if mqtt_reason_success(reason_code):
            connected.set()
        else:
            failed.append(f"MQTT connect failed: {reason_code}")
            connected.set()

    def on_disconnect(client, userdata, flags=None, reason_code=None, properties=None):
        if reason_code not in (None, 0):
            print(f"[mqtt] disconnected reason={reason_code}", file=sys.stderr)

    client = make_mqtt_client(client_id, on_connect, on_disconnect)
    client.connect(host, port, keepalive=60)
    client.loop_start()
    if not connected.wait(timeout=5.0):
        client.loop_stop()
        client.disconnect()
        raise RuntimeError(f"MQTT connect timeout: {host}:{port}")
    if failed:
        client.loop_stop()
        client.disconnect()
        raise RuntimeError(failed[0])
    print(f"[mqtt] connected broker={host}:{port} client_id={client_id}")
    return client


def build_payload(detections: list[dict]) -> tuple[str, dict]:
    if not detections:
        result = {
            "posture": "unknown",
            "confidence": 0,
            "risk": 2,
            "face": 0,
            "face_stable": 0,
            "reason": "pc_camera_yolo_no_detection",
            "bbox": None,
        }
    else:
        det = detections[0]
        result = {
            "posture": det["class"],
            "confidence": max(0, min(100, int(round(float(det["score"]) * 100.0)))),
            "risk": int(det["risk"]),
            "face": 1,
            "face_stable": 1,
            "reason": "pc_camera_yolo",
            "bbox": det["bbox"],
        }

    payload = (
        "vision=on,"
        f"vision_posture={result['posture']},"
        f"vision_confidence={result['confidence']},"
        f"vision_risk={result['risk']},"
        f"face={result['face']},"
        f"face_stable={result['face_stable']},"
        f"reason={result['reason']}"
    )
    return payload, result


def publish_payload(client, topic: str, payload: str) -> None:
    mqtt = import_mqtt()
    info = client.publish(topic, payload, qos=1)
    info.wait_for_publish(timeout=5.0)
    if info.rc != mqtt.MQTT_ERR_SUCCESS:
        raise RuntimeError(f"MQTT publish failed: rc={info.rc}")


def print_result(idx: int, result: dict, topic: str, payload: str) -> None:
    bbox = "none" if result["bbox"] is None else tuple(result["bbox"])
    print(
        "[vision] "
        f"idx={idx} posture={result['posture']} "
        f"confidence={result['confidence']} risk={result['risk']} bbox={bbox}"
    )
    print(f"[mqtt] published topic={topic} payload={payload}")


def main() -> int:
    args = parse_args()
    if args.count < 0:
        print("[error] --count must be >= 0", file=sys.stderr)
        return 2
    if args.warmup < 0:
        print("[error] --warmup must be >= 0", file=sys.stderr)
        return 2
    if args.interval < 0:
        print("[error] --interval must be >= 0", file=sys.stderr)
        return 2

    classes = load_classes(args.classes)
    tmp_dir = None
    cap = None
    mqtt_client = None
    try:
        interpreter, tmp_dir = load_interpreter(args.model)
        interpreter.allocate_tensors()

        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
        print_tensor_details("inputs", input_details)
        print_tensor_details("outputs", output_details)

        if not input_details or not output_details:
            raise RuntimeError("model has no input or output tensors")
        if len(output_details) != 1:
            print("[warn] this script decodes only the first output tensor")

        mqtt_client = connect_mqtt(args.broker, args.port, args.client_id)
        cap = open_camera(args.camera)
        warmup_camera(cap, args.warmup)

        input_detail = input_details[0]
        idx = 0
        while args.count == 0 or idx < args.count:
            started = time.monotonic()
            idx += 1
            frame_bgr = read_frame(cap)
            image_rgb = frame_to_rgb(frame_bgr)
            input_tensor = prepare_image_input(image_rgb, input_detail)
            output, output_detail = run_inference(interpreter, input_tensor)
            detections = decode_yolo(
                output=output,
                output_detail=output_detail,
                classes=classes,
                image_w=image_rgb.shape[1],
                image_h=image_rgb.shape[0],
                conf_thresh=args.conf,
                iou_thresh=args.iou,
                top_k=args.top_k,
            )
            payload, result = build_payload(detections)
            publish_payload(mqtt_client, args.topic, payload)
            print_result(idx, result, args.topic, payload)
            sleep_until_next(started, args.interval)

        return 0
    finally:
        if cap is not None:
            cap.release()
        if mqtt_client is not None:
            mqtt_client.loop_stop()
            mqtt_client.disconnect()
        if tmp_dir:
            shutil.rmtree(tmp_dir, ignore_errors=True)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
    except Exception as exc:
        print(f"[error] {exc}", file=sys.stderr)
        raise SystemExit(1)
