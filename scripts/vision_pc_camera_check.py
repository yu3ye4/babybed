#!/usr/bin/env python3
"""Check that a PC camera can capture usable frames for BabyBed vision."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open a PC camera, capture frames, and print image statistics."
    )
    parser.add_argument("--camera", type=int, default=0, help="OpenCV camera index")
    parser.add_argument(
        "--frames",
        type=int,
        default=30,
        help="Number of frames to read before reporting the last frame",
    )
    parser.add_argument("--save", help="Optional path to save the last captured frame")
    return parser.parse_args()


def import_cv2():
    try:
        import cv2  # type: ignore

        return cv2
    except Exception as exc:
        raise RuntimeError(
            "OpenCV is unavailable. Install it with: pip install opencv-python numpy"
        ) from exc


def save_frame(path_text: str, frame_bgr: np.ndarray) -> None:
    cv2 = import_cv2()
    path = Path(path_text)
    if path.parent:
        path.parent.mkdir(parents=True, exist_ok=True)

    suffix = path.suffix.lower() or ".jpg"
    ok, encoded = cv2.imencode(suffix, frame_bgr)
    if not ok:
        raise RuntimeError(f"failed to encode frame as {suffix}")
    encoded.tofile(str(path))
    print(f"[save] {path}")


def main() -> int:
    args = parse_args()
    if args.frames <= 0:
        print("[error] --frames must be positive", file=sys.stderr)
        return 2

    try:
        cv2 = import_cv2()
    except RuntimeError as exc:
        print(f"[error] {exc}", file=sys.stderr)
        return 2

    cap = cv2.VideoCapture(args.camera, cv2.CAP_DSHOW)
    if not cap.isOpened():
        cap.release()
        cap = cv2.VideoCapture(args.camera)

    if not cap.isOpened():
        print(f"[error] failed to open camera index={args.camera}", file=sys.stderr)
        return 1

    print(f"[camera] opened index={args.camera}")

    frame = None
    read_count = 0
    for _ in range(args.frames):
        ok, current = cap.read()
        if ok and current is not None and current.size > 0:
            frame = current
            read_count += 1

    fps = float(cap.get(cv2.CAP_PROP_FPS) or 0.0)
    cap.release()

    if frame is None:
        print(
            f"[error] camera opened but no valid frame was read "
            f"from {args.frames} attempts",
            file=sys.stderr,
        )
        return 1

    height, width = frame.shape[:2]
    min_v = int(frame.min())
    max_v = int(frame.max())
    mean_v = float(frame.mean())
    nonzero = int(np.count_nonzero(frame))

    if nonzero == 0:
        print("[error] last frame is all zeros", file=sys.stderr)
        return 1

    print(
        "[frame] "
        f"width={width} height={height} fps={fps:.2f} "
        f"mean={mean_v:.2f} min={min_v} max={max_v} nonzero={nonzero} "
        f"valid_reads={read_count}/{args.frames}"
    )

    if args.save:
        try:
            save_frame(args.save, frame)
        except Exception as exc:
            print(f"[error] failed to save frame: {exc}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
