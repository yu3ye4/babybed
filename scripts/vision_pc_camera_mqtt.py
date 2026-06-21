#!/usr/bin/env python3
"""Publish BabyBed PC camera YOLO posture inference results to MQTT."""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import shutil
import sys
import threading
import time

from vision_pc_camera_yolo import (
    frame_to_rgb,
    import_cv2,
    open_camera,
    read_frame,
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
DEFAULT_PREVIEW_HOST = "127.0.0.1"
DEFAULT_PREVIEW_PORT = 8765


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
    parser.add_argument("--preview-host", default=DEFAULT_PREVIEW_HOST, help="MJPEG preview host")
    parser.add_argument(
        "--preview-port",
        type=int,
        default=DEFAULT_PREVIEW_PORT,
        help="MJPEG preview port; 0 disables preview",
    )
    parser.add_argument("--preview-fps", type=float, default=15.0, help="MJPEG preview FPS")
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


def default_result() -> dict:
    return {
        "posture": "unknown",
        "confidence": 0,
        "risk": 2,
        "face": 0,
        "face_stable": 0,
        "reason": "pc_camera_yolo_no_detection",
        "bbox": None,
    }


def draw_overlay(frame_bgr, result: dict):
    cv2 = import_cv2()
    output = frame_bgr.copy()
    bbox = result.get("bbox")
    risk = int(result.get("risk", 2) or 0)
    color = (32, 128, 32) if risk <= 1 else (0, 165, 255) if risk == 2 else (0, 0, 220)

    if isinstance(bbox, list) and len(bbox) == 4:
        x1, y1, x2, y2 = [int(v) for v in bbox]
        cv2.rectangle(output, (x1, y1), (x2, y2), color, 2)

    label = (
        f"{result.get('posture', 'unknown')} "
        f"{result.get('confidence', 0)}% risk {risk}"
    )
    cv2.rectangle(output, (8, 8), (8 + max(260, len(label) * 12), 42), (0, 0, 0), -1)
    cv2.putText(
        output,
        label,
        (18, 32),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.65,
        (255, 255, 255),
        2,
        cv2.LINE_AA,
    )
    return output


class PreviewState:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.jpeg: bytes | None = None

    def update(self, frame_bgr, result: dict) -> None:
        cv2 = import_cv2()
        preview = draw_overlay(frame_bgr, result)
        ok, encoded = cv2.imencode(".jpg", preview, [int(cv2.IMWRITE_JPEG_QUALITY), 82])
        if not ok:
            return
        with self.lock:
            self.jpeg = encoded.tobytes()

    def get(self) -> bytes | None:
        with self.lock:
            return self.jpeg


def make_preview_handler(state: PreviewState, fps: float):
    delay = 1.0 / fps if fps > 0 else 0.1

    class PreviewHandler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args) -> None:
            return

        def do_GET(self) -> None:
            if self.path not in ("/", "/video.mjpg"):
                self.send_error(404)
                return

            self.send_response(200)
            self.send_header("Age", "0")
            self.send_header("Cache-Control", "no-cache, private")
            self.send_header("Pragma", "no-cache")
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
            self.end_headers()

            while True:
                jpeg = state.get()
                if jpeg is None:
                    time.sleep(delay)
                    continue
                try:
                    self.wfile.write(b"--frame\r\n")
                    self.wfile.write(b"Content-Type: image/jpeg\r\n")
                    self.wfile.write(f"Content-Length: {len(jpeg)}\r\n\r\n".encode("ascii"))
                    self.wfile.write(jpeg)
                    self.wfile.write(b"\r\n")
                    time.sleep(delay)
                except (BrokenPipeError, ConnectionResetError, OSError):
                    break

    return PreviewHandler


def start_preview_server(host: str, port: int, fps: float) -> tuple[ThreadingHTTPServer | None, PreviewState | None]:
    if port <= 0:
        return None, None

    state = PreviewState()
    server = ThreadingHTTPServer((host, port), make_preview_handler(state, fps))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    print(f"[preview] http://{host}:{port}/video.mjpg")
    return server, state


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
    if args.preview_port < 0:
        print("[error] --preview-port must be >= 0", file=sys.stderr)
        return 2
    if args.preview_fps <= 0:
        print("[error] --preview-fps must be > 0", file=sys.stderr)
        return 2

    classes = load_classes(args.classes)
    tmp_dir = None
    cap = None
    mqtt_client = None
    preview_server = None
    preview_state = None
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
        preview_server, preview_state = start_preview_server(
            args.preview_host, args.preview_port, args.preview_fps
        )
        cap = open_camera(args.camera)
        warmup_camera(cap, args.warmup)

        input_detail = input_details[0]
        idx = 0
        latest_result = default_result()
        next_infer_at = time.monotonic()
        frame_delay = 1.0 / args.preview_fps
        while args.count == 0 or idx < args.count:
            frame_bgr = read_frame(cap)
            now = time.monotonic()
            if args.interval == 0 or now >= next_infer_at:
                idx += 1
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
                payload, latest_result = build_payload(detections)
                publish_payload(mqtt_client, args.topic, payload)
                print_result(idx, latest_result, args.topic, payload)
                next_infer_at = time.monotonic() + args.interval

            if preview_state is not None:
                preview_state.update(frame_bgr, latest_result)
            time.sleep(frame_delay)

        return 0
    finally:
        if preview_server is not None:
            preview_server.shutdown()
            preview_server.server_close()
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
