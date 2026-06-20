#!/usr/bin/env python3
"""Verify the BabyBed YOLO TFLite model on a PC.

This script is intentionally PC-only. It prints TensorFlow Lite input/output
metadata and can run a simple YOLO detection smoke test on one image.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path
from typing import Iterable

import numpy as np


EXPECTED_CLASSES = [
    "safe_sleep",
    "face_down",
    "blanket_cover",
    "near_edge",
    "side_sleep",
    "bad",
]

RISK_BY_CLASS = {
    "safe_sleep": 0,
    "side_sleep": 1,
    "near_edge": 2,
    "blanket_cover": 3,
    "face_down": 3,
    "bad": 3,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Inspect and smoke-test a YOLO int8 TFLite model."
    )
    parser.add_argument("--model", required=True, help="Path to .tflite model")
    parser.add_argument("--classes", help="Path to classes.txt")
    parser.add_argument("--image", help="Optional test image path")
    parser.add_argument("--conf", type=float, default=0.25, help="Confidence threshold")
    parser.add_argument("--iou", type=float, default=0.45, help="NMS IoU threshold")
    parser.add_argument("--top-k", type=int, default=10, help="Max detections to print")
    return parser.parse_args()


def load_classes(path: str | None) -> list[str]:
    if not path:
        return EXPECTED_CLASSES.copy()

    class_path = Path(path)
    lines = class_path.read_text(encoding="utf-8").splitlines()
    names: list[str] = []
    for line in lines:
        text = line.strip()
        if not text:
            continue
        match = re.match(r"^\s*\d+\s*[:,-]?\s*(\S+)\s*$", text)
        names.append(match.group(1) if match else text.split()[0])

    print(f"[classes] {class_path}")
    for idx, name in enumerate(names):
        print(f"  {idx}: {name}")

    if names != EXPECTED_CLASSES:
        print(
            "[warn] classes.txt order differs from expected: "
            + ", ".join(EXPECTED_CLASSES)
        )
    return names


def import_tflite_interpreter():
    try:
        import tensorflow as tf  # type: ignore

        return tf.lite.Interpreter, f"tensorflow {tf.__version__}"
    except Exception as tf_exc:
        try:
            from tflite_runtime.interpreter import Interpreter  # type: ignore

            return Interpreter, "tflite_runtime"
        except Exception as rt_exc:
            raise RuntimeError(
                "TensorFlow Lite interpreter is unavailable. Install tensorflow "
                "or tflite_runtime first."
            ) from rt_exc if rt_exc else tf_exc


def load_interpreter(model_path: str):
    interpreter_cls, backend = import_tflite_interpreter()
    print(f"[backend] {backend}")

    try:
        interpreter = interpreter_cls(model_path=model_path)
        return interpreter, None
    except Exception as direct_exc:
        tmp_dir = tempfile.mkdtemp(prefix="babybed_tflite_")
        tmp_model = os.path.join(tmp_dir, "model.tflite")
        shutil.copyfile(model_path, tmp_model)
        print(
            "[warn] direct model load failed; copied model to ASCII temp path: "
            f"{tmp_model}"
        )
        try:
            interpreter = interpreter_cls(model_path=tmp_model)
            return interpreter, tmp_dir
        except Exception as copy_exc:
            shutil.rmtree(tmp_dir, ignore_errors=True)
            raise RuntimeError(
                f"failed to load model directly ({direct_exc}) or from temp copy "
                f"({copy_exc})"
            ) from copy_exc


def quant_tuple(detail: dict) -> tuple[float, int]:
    scale, zero_point = detail.get("quantization", (0.0, 0))
    return float(scale or 0.0), int(zero_point or 0)


def print_tensor_details(label: str, details: list[dict]) -> None:
    print(f"[{label}] count={len(details)}")
    for idx, detail in enumerate(details):
        shape = detail.get("shape")
        shape_text = shape.tolist() if hasattr(shape, "tolist") else shape
        dtype = detail.get("dtype")
        dtype_name = getattr(dtype, "__name__", str(dtype))
        scale, zero_point = quant_tuple(detail)
        print(
            f"  {idx}: name={detail.get('name')} shape={shape_text} "
            f"dtype={dtype_name} scale={scale} zero_point={zero_point}"
        )


def dtype_range(dtype: np.dtype) -> tuple[int, int]:
    info = np.iinfo(dtype)
    return int(info.min), int(info.max)


def make_empty_input(input_detail: dict) -> np.ndarray:
    shape = tuple(int(v) for v in input_detail["shape"])
    dtype = np.dtype(input_detail["dtype"])
    scale, zero_point = quant_tuple(input_detail)
    if np.issubdtype(dtype, np.integer):
        low, high = dtype_range(dtype)
        value = min(max(zero_point, low), high) if scale else 0
        return np.full(shape, value, dtype=dtype)
    return np.zeros(shape, dtype=dtype)


def dequantize(tensor: np.ndarray, detail: dict) -> np.ndarray:
    scale, zero_point = quant_tuple(detail)
    if scale and np.issubdtype(tensor.dtype, np.integer):
        return (tensor.astype(np.float32) - zero_point) * scale
    return tensor.astype(np.float32)


def summarize_output(output: np.ndarray, output_detail: dict) -> None:
    values = dequantize(output, output_detail)
    print(
        "[output-summary] "
        f"shape={list(output.shape)} raw_dtype={output.dtype} "
        f"dequant_min={values.min():.6f} dequant_max={values.max():.6f} "
        f"dequant_mean={values.mean():.6f}"
    )


def read_image_rgb(path: str) -> np.ndarray:
    import cv2  # type: ignore

    data = np.fromfile(path, dtype=np.uint8)
    image_bgr = cv2.imdecode(data, cv2.IMREAD_COLOR)
    if image_bgr is None:
        raise ValueError(f"failed to read image: {path}")
    return cv2.cvtColor(image_bgr, cv2.COLOR_BGR2RGB)


def prepare_image_input(image_rgb: np.ndarray, input_detail: dict) -> np.ndarray:
    import cv2  # type: ignore

    shape = tuple(int(v) for v in input_detail["shape"])
    if len(shape) != 4 or shape[0] != 1 or shape[-1] != 3:
        raise ValueError(f"unsupported input shape for image mode: {shape}")

    height, width = shape[1], shape[2]
    resized = cv2.resize(image_rgb, (width, height), interpolation=cv2.INTER_LINEAR)
    dtype = np.dtype(input_detail["dtype"])
    scale, zero_point = quant_tuple(input_detail)

    if np.issubdtype(dtype, np.integer):
        if not scale:
            raise ValueError("integer input has no quantization scale")
        values = np.rint((resized.astype(np.float32) / 255.0) / scale + zero_point)
        low, high = dtype_range(dtype)
        return np.clip(values, low, high).astype(dtype)[None, ...]

    return (resized.astype(dtype) / np.array(255.0, dtype=dtype))[None, ...]


def sigmoid(values: np.ndarray) -> np.ndarray:
    return 1.0 / (1.0 + np.exp(-values))


def probability_values(values: np.ndarray) -> np.ndarray:
    if values.size and float(values.min()) >= 0.0 and float(values.max()) <= 1.0:
        return values
    return sigmoid(values)


def xywh_to_xyxy(boxes: np.ndarray, image_w: int, image_h: int) -> np.ndarray:
    coords = boxes.astype(np.float32).copy()
    max_coord = float(np.nanmax(np.abs(coords[:, :4]))) if coords.size else 0.0
    if max_coord <= 2.0:
        coords[:, [0, 2]] *= image_w
        coords[:, [1, 3]] *= image_h

    cx, cy, w, h = coords[:, 0], coords[:, 1], coords[:, 2], coords[:, 3]
    xyxy = np.stack(
        [cx - w / 2.0, cy - h / 2.0, cx + w / 2.0, cy + h / 2.0], axis=1
    )
    xyxy[:, [0, 2]] = np.clip(xyxy[:, [0, 2]], 0, image_w - 1)
    xyxy[:, [1, 3]] = np.clip(xyxy[:, [1, 3]], 0, image_h - 1)
    return xyxy


def box_iou(box: np.ndarray, boxes: np.ndarray) -> np.ndarray:
    x1 = np.maximum(box[0], boxes[:, 0])
    y1 = np.maximum(box[1], boxes[:, 1])
    x2 = np.minimum(box[2], boxes[:, 2])
    y2 = np.minimum(box[3], boxes[:, 3])
    inter = np.maximum(0.0, x2 - x1) * np.maximum(0.0, y2 - y1)
    box_area = max(0.0, float(box[2] - box[0])) * max(0.0, float(box[3] - box[1]))
    boxes_area = np.maximum(0.0, boxes[:, 2] - boxes[:, 0]) * np.maximum(
        0.0, boxes[:, 3] - boxes[:, 1]
    )
    union = box_area + boxes_area - inter
    return np.divide(inter, union, out=np.zeros_like(inter), where=union > 0)


def nms(boxes: np.ndarray, scores: np.ndarray, iou_thresh: float) -> list[int]:
    order = np.argsort(-scores)
    keep: list[int] = []
    while order.size:
        current = int(order[0])
        keep.append(current)
        if order.size == 1:
            break
        rest = order[1:]
        ious = box_iou(boxes[current], boxes[rest])
        order = rest[ious <= iou_thresh]
    return keep


def decode_yolo(
    output: np.ndarray,
    output_detail: dict,
    classes: list[str],
    image_w: int,
    image_h: int,
    conf_thresh: float,
    iou_thresh: float,
    top_k: int,
) -> list[dict]:
    values = dequantize(output, output_detail)
    if values.ndim == 3 and values.shape[0] == 1:
        values = values[0]
    if values.ndim != 2 or values.shape[1] < 5 + len(classes):
        raise ValueError(
            f"expected YOLO output [1, N, {5 + len(classes)}], got {list(output.shape)}"
        )

    boxes_xywh = values[:, :4]
    obj = probability_values(values[:, 4])
    cls_scores = probability_values(values[:, 5 : 5 + len(classes)])
    cls_ids = np.argmax(cls_scores, axis=1)
    cls_conf = cls_scores[np.arange(cls_scores.shape[0]), cls_ids]
    scores = obj * cls_conf

    mask = scores >= conf_thresh
    if not np.any(mask):
        return []

    boxes = xywh_to_xyxy(boxes_xywh[mask], image_w, image_h)
    scores = scores[mask]
    cls_ids = cls_ids[mask]
    kept = nms(boxes, scores, iou_thresh)[:top_k]

    detections: list[dict] = []
    for idx in kept:
        class_name = classes[int(cls_ids[idx])]
        bbox = boxes[idx]
        detections.append(
            {
                "class": class_name,
                "score": float(scores[idx]),
                "risk": RISK_BY_CLASS.get(class_name, 3),
                "bbox": [int(round(v)) for v in bbox.tolist()],
            }
        )
    return detections


def run_inference(interpreter, input_tensor: np.ndarray) -> tuple[np.ndarray, dict]:
    input_detail = interpreter.get_input_details()[0]
    output_detail = interpreter.get_output_details()[0]
    interpreter.set_tensor(input_detail["index"], input_tensor)
    interpreter.invoke()
    return interpreter.get_tensor(output_detail["index"]), output_detail


def print_detections(detections: Iterable[dict]) -> None:
    print("[detections]")
    count = 0
    for count, det in enumerate(detections, start=1):
        x1, y1, x2, y2 = det["bbox"]
        print(
            f"  {count}: class={det['class']} score={det['score']:.4f} "
            f"risk={det['risk']} bbox_xyxy=({x1},{y1},{x2},{y2})"
        )
    if count == 0:
        print("  none above threshold")


def main() -> int:
    args = parse_args()
    classes = load_classes(args.classes)

    tmp_dir = None
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
        if args.image:
            image_rgb = read_image_rgb(args.image)
            input_tensor = prepare_image_input(image_rgb, input_detail)
            output, output_detail = run_inference(interpreter, input_tensor)
            summarize_output(output, output_detail)
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
            print_detections(detections)
        else:
            input_tensor = make_empty_input(input_detail)
            output, output_detail = run_inference(interpreter, input_tensor)
            summarize_output(output, output_detail)
            print("[info] no --image provided; skipped YOLO post-processing")

        return 0
    finally:
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
