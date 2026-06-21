#!/usr/bin/env python3
"""Run BabyBed YOLO posture inference on a video and annotate the frames."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys
import time

from vision_pc_camera_yolo import frame_to_rgb, import_cv2
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
        description="Run YOLO posture inference on a video and draw annotations."
    )
    parser.add_argument("--video", required=True, help="Input video path")
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Path to .tflite model")
    parser.add_argument("--classes", default=DEFAULT_CLASSES, help="Path to classes.txt")
    parser.add_argument("--output", help="Optional annotated output video path")
    parser.add_argument("--display", action="store_true", help="Show annotated frames live")
    parser.add_argument(
        "--realtime",
        action="store_true",
        help="When displaying, delay frames to roughly match the source FPS",
    )
    parser.add_argument("--conf", type=float, default=0.45, help="Confidence threshold")
    parser.add_argument("--iou", type=float, default=0.45, help="NMS IoU threshold")
    parser.add_argument("--top-k", type=int, default=5, help="Max detections to draw")
    parser.add_argument(
        "--stride",
        type=int,
        default=1,
        help="Run inference every N frames; skipped frames reuse the last result",
    )
    parser.add_argument("--limit", type=int, default=0, help="Max frames to process; 0 means all")
    parser.add_argument("--print-every", type=int, default=30, help="Print progress every N frames")
    return parser.parse_args()


def open_video(path: str):
    cv2 = import_cv2()
    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        raise RuntimeError(f"failed to open video: {path}")
    return cap


def make_writer(output_path: str, fps: float, width: int, height: int):
    cv2 = import_cv2()
    Path(output_path).parent.mkdir(parents=True, exist_ok=True)
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(output_path, fourcc, fps if fps > 0 else 25.0, (width, height))
    if not writer.isOpened():
        raise RuntimeError(f"failed to open output video: {output_path}")
    return writer


def draw_annotations(frame_bgr, detections: list[dict], frame_idx: int):
    cv2 = import_cv2()
    annotated = frame_bgr.copy()
    if detections:
        for det in detections:
            x1, y1, x2, y2 = [int(v) for v in det["bbox"]]
            risk = int(det["risk"])
            color = (32, 160, 32) if risk <= 1 else (0, 165, 255) if risk == 2 else (0, 0, 220)
            cv2.rectangle(annotated, (x1, y1), (x2, y2), color, 2)
            label = f"{det['class']} {det['score']:.2f} risk {risk}"
            y_text = max(22, y1 - 8)
            cv2.rectangle(
                annotated,
                (x1, y_text - 20),
                (x1 + max(210, len(label) * 11), y_text + 6),
                color,
                -1,
            )
            cv2.putText(
                annotated,
                label,
                (x1 + 4, y_text),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.58,
                (255, 255, 255),
                2,
                cv2.LINE_AA,
            )
    else:
        cv2.rectangle(annotated, (8, 8), (270, 42), (0, 0, 0), -1)
        cv2.putText(
            annotated,
            "unknown 0.00 risk 2",
            (18, 32),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.65,
            (255, 255, 255),
            2,
            cv2.LINE_AA,
        )

    cv2.putText(
        annotated,
        f"frame {frame_idx}",
        (10, annotated.shape[0] - 12),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (255, 255, 255),
        2,
        cv2.LINE_AA,
    )
    return annotated


def print_result(frame_idx: int, detections: list[dict]) -> None:
    if not detections:
        print(f"[vision] frame={frame_idx} posture=unknown confidence=0.00 risk=2 bbox=none")
        return
    det = detections[0]
    x1, y1, x2, y2 = det["bbox"]
    print(
        "[vision] "
        f"frame={frame_idx} posture={det['class']} confidence={det['score']:.2f} "
        f"risk={det['risk']} bbox=({x1},{y1},{x2},{y2})"
    )


def main() -> int:
    args = parse_args()
    if args.stride <= 0:
        print("[error] --stride must be >= 1", file=sys.stderr)
        return 2
    if args.limit < 0:
        print("[error] --limit must be >= 0", file=sys.stderr)
        return 2

    classes = load_classes(args.classes)
    tmp_dir = None
    cap = None
    writer = None
    try:
        interpreter, tmp_dir = load_interpreter(args.model)
        interpreter.allocate_tensors()
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
        print_tensor_details("inputs", input_details)
        print_tensor_details("outputs", output_details)
        if not input_details or not output_details:
            raise RuntimeError("model has no input or output tensors")
        input_detail = input_details[0]

        cap = open_video(args.video)
        cv2 = import_cv2()
        width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        fps = float(cap.get(cv2.CAP_PROP_FPS) or 0.0)
        total = int(cap.get(cv2.CAP_PROP_FRAME_COUNT) or 0)
        print(f"[video] path={args.video}")
        print(f"[video] width={width} height={height} fps={fps:.2f} frames={total}")

        if args.output:
            writer = make_writer(args.output, fps, width, height)
            print(f"[video] output={args.output}")

        last_detections: list[dict] = []
        frame_idx = 0
        processed = 0
        started = time.monotonic()
        while True:
            ok, frame_bgr = cap.read()
            if not ok or frame_bgr is None:
                break
            frame_idx += 1
            if args.limit and frame_idx > args.limit:
                break

            if (frame_idx - 1) % args.stride == 0:
                image_rgb = frame_to_rgb(frame_bgr)
                input_tensor = prepare_image_input(image_rgb, input_detail)
                output, output_detail = run_inference(interpreter, input_tensor)
                last_detections = decode_yolo(
                    output=output,
                    output_detail=output_detail,
                    classes=classes,
                    image_w=image_rgb.shape[1],
                    image_h=image_rgb.shape[0],
                    conf_thresh=args.conf,
                    iou_thresh=args.iou,
                    top_k=args.top_k,
                )

            annotated = draw_annotations(frame_bgr, last_detections, frame_idx)
            if writer is not None:
                writer.write(annotated)
            if args.display:
                cv2.imshow("BabyBed YOLO Video", annotated)
                delay_ms = max(1, int(1000.0 / fps)) if args.realtime and fps > 0 else 1
                if cv2.waitKey(delay_ms) & 0xFF in (27, ord("q")):
                    break

            processed += 1
            if args.print_every > 0 and (frame_idx == 1 or frame_idx % args.print_every == 0):
                print_result(frame_idx, last_detections)

        elapsed = time.monotonic() - started
        speed = processed / elapsed if elapsed > 0 else 0.0
        print(f"[done] processed={processed} elapsed={elapsed:.2f}s speed={speed:.2f}fps")
        return 0
    finally:
        if writer is not None:
            writer.release()
        if cap is not None:
            cap.release()
        if args.display:
            import_cv2().destroyAllWindows()
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
