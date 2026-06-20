#!/usr/bin/env python3
"""Run BabyBed YOLO posture inference on frames from a PC camera."""

from __future__ import annotations

import argparse
import shutil
import sys
import time

import numpy as np

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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run YOLO posture inference on PC camera frames."
    )
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Path to .tflite model")
    parser.add_argument("--classes", default=DEFAULT_CLASSES, help="Path to classes.txt")
    parser.add_argument("--camera", type=int, default=0, help="OpenCV camera index")
    parser.add_argument("--interval", type=float, default=1.0, help="Seconds between inferences")
    parser.add_argument("--count", type=int, default=10, help="Number of inferences; 0 runs forever")
    parser.add_argument("--warmup", type=int, default=30, help="Frames to read before inference")
    parser.add_argument("--conf", type=float, default=0.25, help="Confidence threshold")
    parser.add_argument("--iou", type=float, default=0.45, help="NMS IoU threshold")
    parser.add_argument("--top-k", type=int, default=5, help="Max detections to keep")
    return parser.parse_args()


def import_cv2():
    try:
        import cv2  # type: ignore

        return cv2
    except Exception as exc:
        raise RuntimeError(
            "OpenCV is unavailable. Install it with: pip install opencv-python numpy"
        ) from exc


def open_camera(camera_index: int):
    cv2 = import_cv2()
    cap = cv2.VideoCapture(camera_index, cv2.CAP_DSHOW)
    if not cap.isOpened():
        cap.release()
        cap = cv2.VideoCapture(camera_index)
    if not cap.isOpened():
        raise RuntimeError(f"failed to open camera index={camera_index}")
    print(f"[camera] opened index={camera_index}")
    return cap


def read_frame(cap, attempts: int = 5):
    frame = None
    for _ in range(max(1, attempts)):
        ok, current = cap.read()
        if ok and current is not None and current.size > 0:
            frame = current
            break
    if frame is None:
        raise RuntimeError("failed to read a valid camera frame")
    if int(np.count_nonzero(frame)) == 0:
        raise RuntimeError("captured camera frame is all zeros")
    return frame


def warmup_camera(cap, frames: int) -> None:
    if frames <= 0:
        return
    valid = 0
    for _ in range(frames):
        ok, current = cap.read()
        if ok and current is not None and current.size > 0:
            valid += 1
    print(f"[camera] warmup valid_reads={valid}/{frames}")


def frame_to_rgb(frame_bgr: np.ndarray) -> np.ndarray:
    cv2 = import_cv2()
    return cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)


def print_top_result(idx: int, detections: list[dict]) -> None:
    if not detections:
        print("[vision] idx=%d posture=unknown confidence=0.00 risk=2 bbox=none" % idx)
        return

    det = detections[0]
    x1, y1, x2, y2 = det["bbox"]
    print(
        "[vision] "
        f"idx={idx} posture={det['class']} confidence={det['score']:.2f} "
        f"risk={det['risk']} bbox=({x1},{y1},{x2},{y2})"
    )


def sleep_until_next(start_time: float, interval: float) -> None:
    if interval <= 0:
        return
    elapsed = time.monotonic() - start_time
    remaining = interval - elapsed
    if remaining > 0:
        time.sleep(remaining)


def main() -> int:
    args = parse_args()
    if args.count < 0:
        print("[error] --count must be >= 0", file=sys.stderr)
        return 2
    if args.warmup < 0:
        print("[error] --warmup must be >= 0", file=sys.stderr)
        return 2

    classes = load_classes(args.classes)
    tmp_dir = None
    cap = None
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

        input_detail = input_details[0]
        cap = open_camera(args.camera)
        warmup_camera(cap, args.warmup)

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
            print_top_result(idx, detections)
            sleep_until_next(started, args.interval)

        return 0
    finally:
        if cap is not None:
            cap.release()
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
