#!/usr/bin/env python3
"""Convert a TFLite model file into C source/header files."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


DEFAULT_SYMBOL = "baby_yolov5n_int8_tflite"
BYTES_PER_LINE = 12


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Embed a .tflite model as a C unsigned char array."
    )
    parser.add_argument("--model", required=True, help="Input .tflite file")
    parser.add_argument("--out-dir", required=True, help="Directory for generated files")
    parser.add_argument(
        "--symbol",
        default=DEFAULT_SYMBOL,
        help=f"C array symbol name, default: {DEFAULT_SYMBOL}",
    )
    return parser.parse_args()


def require_c_identifier(name: str) -> str:
    if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name):
        raise ValueError(f"invalid C identifier: {name}")
    return name


def header_guard(symbol: str) -> str:
    return f"{symbol.upper()}_H"


def format_bytes(data: bytes) -> str:
    lines: list[str] = []
    for offset in range(0, len(data), BYTES_PER_LINE):
        chunk = data[offset : offset + BYTES_PER_LINE]
        values = ", ".join(f"0x{value:02x}" for value in chunk)
        lines.append(f"    {values},")
    return "\n".join(lines)


def write_header(path: Path, symbol: str) -> None:
    guard = header_guard(symbol)
    path.write_text(
        "\n".join(
            [
                f"#ifndef {guard}",
                f"#define {guard}",
                "",
                "#ifdef __cplusplus",
                'extern "C" {',
                "#endif",
                "",
                f"extern const unsigned char {symbol}[];",
                f"extern const unsigned int {symbol}_len;",
                "",
                "#ifdef __cplusplus",
                "}",
                "#endif",
                "",
                f"#endif /* {guard} */",
                "",
            ]
        ),
        encoding="utf-8",
        newline="\n",
    )


def write_source(path: Path, header_name: str, symbol: str, data: bytes) -> None:
    body = format_bytes(data)
    path.write_text(
        "\n".join(
            [
                f'#include "{header_name}"',
                "",
                f"const unsigned char {symbol}[] = {{",
                body,
                "};",
                "",
                f"const unsigned int {symbol}_len = {len(data)}u;",
                "",
            ]
        ),
        encoding="utf-8",
        newline="\n",
    )


def main() -> int:
    args = parse_args()
    symbol = require_c_identifier(args.symbol)
    model_path = Path(args.model)
    out_dir = Path(args.out_dir)

    data = model_path.read_bytes()
    out_dir.mkdir(parents=True, exist_ok=True)

    header_path = out_dir / f"{symbol}.h"
    source_path = out_dir / f"{symbol}.c"
    write_header(header_path, symbol)
    write_source(source_path, header_path.name, symbol, data)

    print(f"model={model_path}")
    print(f"bytes={len(data)}")
    print(f"header={header_path}")
    print(f"source={source_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
