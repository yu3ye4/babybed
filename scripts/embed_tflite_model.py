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
    parser.add_argument("--source-name", help="Generated C source filename")
    parser.add_argument("--header-name", help="Generated C header filename")
    parser.add_argument("--section", help="Optional C section for the model data")
    parser.add_argument("--align", type=int, help="Optional C alignment in bytes")
    parser.add_argument(
        "--retain",
        action="store_true",
        help="Add used/retain attributes so linker GC keeps the model",
    )
    return parser.parse_args()


def require_c_identifier(name: str) -> str:
    if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name):
        raise ValueError(f"invalid C identifier: {name}")
    return name


def header_guard(symbol: str) -> str:
    return f"{symbol.upper()}_H"


def require_filename(name: str | None, default_name: str) -> str:
    filename = name or default_name
    if Path(filename).name != filename:
        raise ValueError(f"filename must not include a directory: {filename}")
    return filename


def build_attribute(section: str | None, align: int | None, retain: bool) -> str:
    attrs: list[str] = []
    if retain:
        attrs.extend(["used", "retain"])
    if align:
        if align <= 0 or align & (align - 1):
            raise ValueError("--align must be a positive power of two")
        attrs.append(f"aligned({align})")
    if section:
        attrs.append(f'section("{section}")')
    return f" __attribute__(({', '.join(attrs)}))" if attrs else ""


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


def write_source(
    path: Path, header_name: str, symbol: str, data: bytes, attribute: str
) -> None:
    body = format_bytes(data)
    path.write_text(
        "\n".join(
            [
                f'#include "{header_name}"',
                "",
                f"const unsigned char {symbol}[]{attribute} = {{",
                body,
                "};",
                "",
                f"const unsigned int {symbol}_len{attribute} = {len(data)}u;",
                "",
            ]
        ),
        encoding="utf-8",
        newline="\n",
    )


def main() -> int:
    args = parse_args()
    symbol = require_c_identifier(args.symbol)
    source_name = require_filename(args.source_name, f"{symbol}.c")
    header_name = require_filename(args.header_name, f"{symbol}.h")
    attribute = build_attribute(args.section, args.align, args.retain)
    model_path = Path(args.model)
    out_dir = Path(args.out_dir)

    data = model_path.read_bytes()
    out_dir.mkdir(parents=True, exist_ok=True)

    header_path = out_dir / header_name
    source_path = out_dir / source_name
    write_header(header_path, symbol)
    write_source(source_path, header_path.name, symbol, data, attribute)

    print(f"model={model_path}")
    print(f"bytes={len(data)}")
    print(f"header={header_path}")
    print(f"source={source_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
