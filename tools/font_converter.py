#!/usr/bin/env python3
"""8×9の2色ビットマップフォント画像をPixel Twins用C++へ変換する。"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Iterable, Optional, Sequence, Tuple

from PIL import Image


Color = Tuple[int, int, int, int]
GlyphRows = Tuple[Tuple[int, int], ...]


def parse_color(value: str) -> Color:
    if not re.fullmatch(r"#[0-9a-fA-F]{6}", value):
        raise argparse.ArgumentTypeError("色は#rrggbbで指定してください")
    return tuple(int(value[i : i + 2], 16) for i in (1, 3, 5)) + (255,)


def validate_identifier(value: str) -> str:
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", value):
        raise argparse.ArgumentTypeError("C++の識別子として使用できません")
    return value


def validate_namespace(value: str) -> str:
    if not all(validate_identifier(part) for part in value.split("::")):
        raise argparse.ArgumentTypeError("C++の名前空間として使用できません")
    return value


def read_glyphs(
    image_path: Path,
    *,
    columns: int,
    first: int,
    count: int,
    outline_color: Color,
    body_color: Color,
) -> Tuple[GlyphRows, ...]:
    if columns <= 0 or count <= 0:
        raise ValueError("columnsとcountは1以上でなければなりません")
    if first < 0 or first > 255 or first + count > 256:
        raise ValueError("文字コードの範囲は0〜255に収めてください")

    image = Image.open(image_path).convert("RGBA")
    rows = (count + columns - 1) // columns
    required_width = columns * 8
    required_height = rows * 9
    if image.width != required_width or image.height < required_height:
        raise ValueError(
            f"画像寸法は幅{required_width}、高さ{required_height}以上が必要です"
            f"（実際は{image.width}x{image.height}）"
        )

    glyphs = []
    for glyph_index in range(count):
        origin_x = (glyph_index % columns) * 8
        origin_y = (glyph_index // columns) * 9
        glyph_rows = []
        for y in range(9):
            outline = 0
            body = 0
            for x in range(8):
                pixel = image.getpixel((origin_x + x, origin_y + y))
                if pixel[3] == 0:
                    continue
                mask = 0x80 >> x
                if pixel == outline_color:
                    outline |= mask
                elif pixel == body_color:
                    body |= mask
                else:
                    codepoint = first + glyph_index
                    raise ValueError(
                        f"未定義色{pixel}を検出しました: codepoint={codepoint}, x={x}, y={y}"
                    )
            glyph_rows.append((outline, body))
        glyphs.append(tuple(glyph_rows))
    return tuple(glyphs)


def namespace_open(namespace: str) -> str:
    return "\n".join(f"namespace {part} {{" for part in namespace.split("::"))


def namespace_close(namespace: str) -> str:
    return "\n".join(f"}} // namespace {part}" for part in reversed(namespace.split("::")))


def render_header(namespace: str, symbol: str) -> str:
    return f"""// このファイルはfont_converter.pyにより生成されました。編集しないでください。
#pragma once

#include "pixel_twins/font.hpp"

{namespace_open(namespace)}

extern const pixel_twins::BitmapFont {symbol};

{namespace_close(namespace)}
"""


def render_source(
    header_name: str,
    namespace: str,
    symbol: str,
    glyphs: Iterable[GlyphRows],
    first: int,
    fallback: int,
    outline_index: int,
) -> str:
    glyphs = tuple(glyphs)
    entries = []
    for glyph in glyphs:
        rows = ", ".join(f"pixel_twins::GlyphRow{{0x{o:02x}, 0x{b:02x}}}" for o, b in glyph)
        entries.append(f"    pixel_twins::Glyph{{{{{rows}}}}}")
    body = ",\n".join(entries)
    return f"""// このファイルはfont_converter.pyにより生成されました。編集しないでください。
#include "{header_name}"

#include <array>

{namespace_open(namespace)}
namespace {{

constexpr std::array<pixel_twins::Glyph, {len(glyphs)}> kGlyphs{{{{
{body}
}}}};

}} // namespace

const pixel_twins::BitmapFont {symbol}{{
    kGlyphs.data(), {first}, {len(glyphs)}, {fallback}, {outline_index}
}};

{namespace_close(namespace)}
"""


def write_outputs(header: Path, source: Path, header_text: str, source_text: str) -> None:
    header.parent.mkdir(parents=True, exist_ok=True)
    source.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(header_text, encoding="utf-8")
    source.write_text(source_text, encoding="utf-8")


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="8×9フォント画像をPixel Twins用C++へ変換")
    parser.add_argument("image", type=Path)
    parser.add_argument("--header", type=Path, required=True, help="出力.hpp")
    parser.add_argument("--source", type=Path, required=True, help="出力.cpp")
    parser.add_argument("--symbol", type=validate_identifier, required=True)
    parser.add_argument("--namespace", type=validate_namespace, default="pixel_twins::assets")
    parser.add_argument("--columns", type=int, default=16)
    parser.add_argument("--first", type=int, default=32)
    parser.add_argument("--count", type=int, default=95)
    parser.add_argument("--fallback", type=int, default=63)
    parser.add_argument("--outline-color", type=parse_color, default=parse_color("#111315"))
    parser.add_argument("--body-color", type=parse_color, default=parse_color("#f1ead8"))
    parser.add_argument("--outline-index", type=int, default=1)
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = make_parser().parse_args(argv)
    if args.fallback < args.first or args.fallback >= args.first + args.count:
        raise SystemExit("error: fallbackは収録文字の範囲内でなければなりません")
    if args.outline_index < 0 or args.outline_index > 255:
        raise SystemExit("error: outline-indexは0〜255でなければなりません")
    try:
        glyphs = read_glyphs(
            args.image,
            columns=args.columns,
            first=args.first,
            count=args.count,
            outline_color=args.outline_color,
            body_color=args.body_color,
        )
        write_outputs(
            args.header,
            args.source,
            render_header(args.namespace, args.symbol),
            render_source(
                args.header.name,
                args.namespace,
                args.symbol,
                glyphs,
                args.first,
                args.fallback,
                args.outline_index,
            ),
        )
    except (OSError, ValueError) as exc:
        raise SystemExit(f"error: {exc}") from exc
    print(f"{len(glyphs)} glyphs, {len(glyphs) * 18} bytes")
    print(args.header)
    print(args.source)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
