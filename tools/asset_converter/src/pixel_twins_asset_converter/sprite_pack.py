from __future__ import annotations

import hashlib
import json
import re
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from PIL import Image

MAGIC = b"PTSP"
VERSION = 1
HEADER_FORMAT = "<4sHHHHIII"
ASSET_FORMAT = "<IHBBBBH"
FRAME_FORMAT = "<IBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
ASSET_SIZE = struct.calcsize(ASSET_FORMAT)
FRAME_SIZE = struct.calcsize(FRAME_FORMAT)
EMPTY_PIXEL_OFFSET = 0xFFFFFFFF


@dataclass(frozen=True)
class PackedFrame:
    pixel_offset: int
    trim_x: int
    trim_y: int
    width: int
    height: int


def _trim_frame(image: Image.Image) -> Tuple[int, int, int, int, bytes]:
    width, height = image.size
    pixels = list(image.getdata())
    opaque = [index for index, value in enumerate(pixels) if value != 0]
    if not opaque:
        return 0, 0, 0, 0, b""
    left = min(index % width for index in opaque)
    right = max(index % width for index in opaque) + 1
    top = min(index // width for index in opaque)
    bottom = max(index // width for index in opaque) + 1
    body = bytes(
        pixels[y * width + x]
        for y in range(top, bottom)
        for x in range(left, right)
    )
    return left, top, right - left, bottom - top, body


def _relative(path: Path, base: Path) -> str:
    import os

    return Path(os.path.relpath(path, base)).as_posix()


def _identifier(value: str) -> str:
    value = value.split("__")[-1]
    words = re.findall(r"[A-Za-z0-9]+", value)
    result = "".join(word[:1].upper() + word[1:] for word in words) or "Asset"
    return f"_{result}" if result[0].isdigit() else result


def _write_header(path: Path, namespace: str, assets: List[Dict[str, Any]]) -> None:
    if not re.fullmatch(r"[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*", namespace):
        raise ValueError("C++ namespaceが不正です")
    identifiers = [_identifier(asset["id"]) for asset in assets]
    if len(set(identifiers)) != len(identifiers):
        raise ValueError("C++識別子へ変換すると重複するスプライトidがあります")
    entries = "\n".join(
        f"    {identifier} = {asset['asset_index']},"
        for identifier, asset in zip(identifiers, assets)
    )
    path.write_text(f"""#pragma once

#include <cstdint>

namespace {namespace} {{

enum class SpriteAssetId : std::uint16_t {{
{entries}
}};

inline constexpr std::uint16_t kSpriteAssetCount = {len(assets)};

[[nodiscard]] constexpr std::uint16_t spriteAssetIndex(SpriteAssetId id) noexcept {{
    return static_cast<std::uint16_t>(id);
}}

}} // namespace {namespace}
""", encoding="utf-8")


def pack_sprites(
    intermediate_path: Path,
    output_path: Path,
    header_path: Optional[Path] = None,
    cpp_namespace: str = "assets",
) -> Dict[str, Any]:
    intermediate_path = intermediate_path.resolve()
    intermediate_root = intermediate_path.parent
    intermediate = json.loads(intermediate_path.read_text(encoding="utf-8"))
    source_assets = [asset for asset in intermediate["assets"] if asset.get("pack_kind") == "sprite"]
    if len(source_assets) > 0xFFFF:
        raise ValueError("スプライトアセット数が65535を超えています")

    pixel_data = bytearray()
    unique_patterns: Dict[Tuple[int, int, bytes], int] = {}
    asset_records = []
    all_frames: List[PackedFrame] = []
    untrimmed_pixel_bytes = 0
    trimmed_pixel_bytes = 0

    for asset_index, asset in enumerate(source_assets):
        frame_spec = asset.get("frame")
        if not frame_spec:
            raise ValueError(f"{asset['id']}: frame指定がありません")
        frame_width = frame_spec["width"]
        frame_height = frame_spec["height"]
        image_path = intermediate_root / asset["indexed_png"]
        with Image.open(image_path) as image:
            if image.mode != "P":
                raise ValueError(f"{asset['id']}: Pモード画像ではありません")
            if image.width % frame_width != 0 or image.height % frame_height != 0:
                raise ValueError(
                    f"{asset['id']}: シート寸法{image.size}をフレーム{frame_width}x{frame_height}で分割できません"
                )
            columns = image.width // frame_width
            rows = image.height // frame_height
            if columns > 255 or rows > 255:
                raise ValueError(f"{asset['id']}: 行数または列数が255を超えています")
            first_frame = len(all_frames)
            local_unique = set()
            asset_untrimmed = columns * rows * frame_width * frame_height
            asset_trimmed = 0
            empty_frames = 0
            frame_reports = []
            for row in range(rows):
                for column in range(columns):
                    cell = image.crop(
                        (
                            column * frame_width,
                            row * frame_height,
                            (column + 1) * frame_width,
                            (row + 1) * frame_height,
                        )
                    )
                    trim_x, trim_y, width, height, body = _trim_frame(cell)
                    if body:
                        key = (width, height, body)
                        pixel_offset = unique_patterns.get(key)
                        if pixel_offset is None:
                            pixel_offset = len(pixel_data)
                            unique_patterns[key] = pixel_offset
                            pixel_data.extend(body)
                        local_unique.add(key)
                    else:
                        pixel_offset = EMPTY_PIXEL_OFFSET
                        empty_frames += 1
                    frame = PackedFrame(pixel_offset, trim_x, trim_y, width, height)
                    all_frames.append(frame)
                    asset_trimmed += len(body)
                    frame_reports.append(
                        {
                            "index": len(frame_reports),
                            "column": column,
                            "row": row,
                            "pixel_offset": pixel_offset,
                            "trim_x": trim_x,
                            "trim_y": trim_y,
                            "width": width,
                            "height": height,
                            "pixel_bytes": len(body),
                        }
                    )
            frame_count = columns * rows
            if frame_count > 0xFFFF:
                raise ValueError(f"{asset['id']}: フレーム数が65535を超えています")
            local_unique_bytes = sum(len(key[2]) for key in local_unique)
            asset_records.append(
                {
                    "id": asset["id"],
                    "asset_index": asset_index,
                    "first_frame": first_frame,
                    "frame_count": frame_count,
                    "columns": columns,
                    "rows": rows,
                    "logical_width": frame_width,
                    "logical_height": frame_height,
                    "empty_frames": empty_frames,
                    "untrimmed_pixel_bytes": asset_untrimmed,
                    "trimmed_pixel_bytes": asset_trimmed,
                    "local_unique_pixel_bytes": local_unique_bytes,
                    "transparent_border_saved_bytes": asset_untrimmed - asset_trimmed,
                    "local_dedup_saved_bytes": asset_trimmed - local_unique_bytes,
                    "frames": frame_reports,
                }
            )
            untrimmed_pixel_bytes += asset_untrimmed
            trimmed_pixel_bytes += asset_trimmed

    if len(all_frames) > 0xFFFFFFFF:
        raise ValueError("総フレーム数が32-bit範囲を超えています")
    frame_table_offset = HEADER_SIZE + len(asset_records) * ASSET_SIZE
    pixel_data_offset = frame_table_offset + len(all_frames) * FRAME_SIZE
    padding = (-pixel_data_offset) % 4
    pixel_data_offset += padding
    binary = bytearray(
        struct.pack(
            HEADER_FORMAT,
            MAGIC,
            VERSION,
            HEADER_SIZE,
            len(asset_records),
            ASSET_SIZE,
            len(all_frames),
            pixel_data_offset,
            len(pixel_data),
        )
    )
    for asset in asset_records:
        binary.extend(
            struct.pack(
                ASSET_FORMAT,
                asset["first_frame"],
                asset["frame_count"],
                asset["columns"],
                asset["rows"],
                asset["logical_width"],
                asset["logical_height"],
                0,
            )
        )
    for frame in all_frames:
        binary.extend(
            struct.pack(
                FRAME_FORMAT,
                frame.pixel_offset,
                frame.trim_x,
                frame.trim_y,
                frame.width,
                frame.height,
            )
        )
    binary.extend(b"\0" * padding)
    binary.extend(pixel_data)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(binary)
    if header_path is None:
        header_path = output_path.with_suffix(".hpp")
    _write_header(header_path, cpp_namespace, asset_records)

    report = {
        "format": "pixel-twins-sprite-pack",
        "version": VERSION,
        "source_intermediate": _relative(intermediate_path, output_path.parent),
        "binary": output_path.name,
        "header": _relative(header_path, output_path.parent),
        "sha256": hashlib.sha256(binary).hexdigest(),
        "layout": {
            "byte_order": "little-endian",
            "header_bytes": HEADER_SIZE,
            "asset_descriptor_bytes": ASSET_SIZE,
            "frame_descriptor_bytes": FRAME_SIZE,
            "pixel_data_offset": pixel_data_offset,
            "empty_pixel_offset": EMPTY_PIXEL_OFFSET,
        },
        "summary": {
            "asset_count": len(asset_records),
            "frame_count": len(all_frames),
            "empty_frame_count": sum(asset["empty_frames"] for asset in asset_records),
            "untrimmed_pixel_bytes": untrimmed_pixel_bytes,
            "trimmed_pixel_bytes": trimmed_pixel_bytes,
            "unique_pixel_bytes": len(pixel_data),
            "transparent_border_saved_bytes": untrimmed_pixel_bytes - trimmed_pixel_bytes,
            "dedup_saved_bytes": trimmed_pixel_bytes - len(pixel_data),
            "asset_table_bytes": len(asset_records) * ASSET_SIZE,
            "frame_table_bytes": len(all_frames) * FRAME_SIZE,
            "alignment_padding_bytes": padding,
            "binary_bytes": len(binary),
        },
        "assets": asset_records,
    }
    report_path = output_path.with_suffix(".json")
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report
