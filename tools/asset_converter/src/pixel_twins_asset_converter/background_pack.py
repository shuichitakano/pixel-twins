from __future__ import annotations

import hashlib
import json
import os
import re
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional

from PIL import Image

MAGIC = b"PTBG"
VERSION = 1
HEADER_FORMAT = "<4sHHBBBBBBBBIIII"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)


def _relative(path: Path, base: Path) -> str:
    return Path(os.path.relpath(path, base)).as_posix()


def _load_group(config: Dict[str, Any], name: str, expected_items: int) -> List[Dict[str, Any]]:
    groups = config.get(name)
    if not isinstance(groups, list) or not groups:
        raise ValueError(f"{name}を1件以上指定してください")
    seen = set()
    for group in groups:
        if not isinstance(group, dict) or not isinstance(group.get("id"), str):
            raise ValueError(f"{name}の各要素にはidが必要です")
        if group["id"] in seen:
            raise ValueError(f"{name}のidが重複しています: {group['id']}")
        assets = group.get("assets")
        if not isinstance(assets, list) or len(assets) != expected_items:
            raise ValueError(f"{name}.{group['id']}はassetsを{expected_items}件指定してください")
        if not all(isinstance(asset_id, str) for asset_id in assets):
            raise ValueError(f"{name}.{group['id']}.assetsが不正です")
        seen.add(group["id"])
    return groups


def _identifier(value: str) -> str:
    words = re.findall(r"[A-Za-z0-9]+", value)
    result = "".join(word[:1].upper() + word[1:] for word in words) or "Asset"
    return f"_{result}" if result[0].isdigit() else result


def _write_header(path: Path, config: Dict[str, Any], pattern_count: int) -> None:
    namespace = config.get("cpp_namespace", "assets")
    if not isinstance(namespace, str) or not re.fullmatch(r"[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*", namespace):
        raise ValueError("cpp_namespaceが不正です")
    terrain_lines = [f"    {_identifier(group['id'])} = {index}," for index, group in enumerate(config["terrains"])]
    boundary_lines = [f"    {_identifier(group['id'])} = {index}," for index, group in enumerate(config["boundaries"])]
    object_lines = [f"    {_identifier(group['id'])} = {index}," for index, group in enumerate(config["objects"])]
    text = f"""#pragma once

#include <cstdint>

namespace {namespace} {{

enum class TerrainId : std::uint8_t {{
{chr(10).join(terrain_lines)}
}};

enum class BoundaryId : std::uint8_t {{
{chr(10).join(boundary_lines)}
}};

enum class BakedObjectId : std::uint8_t {{
{chr(10).join(object_lines)}
}};

inline constexpr std::uint8_t kBackgroundPatternCount = {pattern_count};
inline constexpr std::uint8_t kTerrainVariantCount = {config['variants_per_terrain']};
inline constexpr std::uint8_t kBoundaryMaskCount = {config['masks_per_boundary']};

}} // namespace {namespace}
"""
    path.write_text(text, encoding="utf-8")


def pack_background(
    config_path: Path, output_path: Path, header_path: Optional[Path] = None
) -> Dict[str, Any]:
    config_path = config_path.resolve()
    config = json.loads(config_path.read_text(encoding="utf-8"))
    if config.get("version") != 1:
        raise ValueError("BG設定のversionは1を指定してください")
    intermediate_path = (config_path.parent / config["intermediate"]).resolve()
    intermediate = json.loads(intermediate_path.read_text(encoding="utf-8"))
    assets_by_id = {asset["id"]: asset for asset in intermediate["assets"]}

    tile_width = config.get("tile_width")
    tile_height = config.get("tile_height")
    variants = config.get("variants_per_terrain")
    masks = config.get("masks_per_boundary")
    max_patterns = config.get("max_patterns", 128)
    if tile_width not in (8, 16, 32, 64, 128) or tile_height not in (8, 16, 32, 64, 128):
        raise ValueError("tile_widthとtile_heightは8〜128の2の冪で指定してください")
    if not isinstance(variants, int) or not 1 <= variants <= 255:
        raise ValueError("variants_per_terrainは1〜255で指定してください")
    if not isinstance(masks, int) or not 1 <= masks <= 255:
        raise ValueError("masks_per_boundaryは1〜255で指定してください")
    if not isinstance(max_patterns, int) or not 1 <= max_patterns <= 128:
        raise ValueError("max_patternsは1〜128で指定してください")

    terrains = _load_group(config, "terrains", variants)
    boundaries = _load_group(config, "boundaries", masks)
    objects = _load_group(config, "objects", 1)
    if any(len(group) > 255 for group in (terrains, boundaries, objects)):
        raise ValueError("BGグループ数が255を超えています")

    ordered_ids = [asset_id for group in terrains for asset_id in group["assets"]]
    ordered_ids += [asset_id for group in boundaries for asset_id in group["assets"]]
    ordered_ids += [asset_id for group in objects for asset_id in group["assets"]]
    pattern_data = bytearray()
    pattern_indices: Dict[bytes, int] = {}
    logical_indices = bytearray()
    logical_report = []
    for asset_id in ordered_ids:
        asset = assets_by_id.get(asset_id)
        if not asset or asset.get("pack_kind") != "background":
            raise ValueError(f"BGアセットが中間データにありません: {asset_id}")
        image_path = intermediate_path.parent / asset["indexed_png"]
        with Image.open(image_path) as image:
            if image.mode != "P" or image.size != (tile_width, tile_height):
                raise ValueError(f"{asset_id}: {tile_width}x{tile_height}のPモード画像ではありません")
            pixels = bytes(image.getdata())
        pattern_index = pattern_indices.get(pixels)
        duplicate_of = None
        if pattern_index is None:
            pattern_index = len(pattern_indices)
            if pattern_index >= max_patterns:
                raise ValueError(f"BGパターン数が上限{max_patterns}を超えています: {pattern_index + 1}")
            pattern_indices[pixels] = pattern_index
            pattern_data.extend(pixels)
        else:
            duplicate_of = next(item["id"] for item in logical_report if item["pattern_index"] == pattern_index)
        logical_indices.append(pattern_index)
        logical_report.append({"id": asset_id, "pattern_index": pattern_index, "duplicate_of": duplicate_of})

    mapping_offset = HEADER_SIZE
    pattern_data_offset = mapping_offset + len(logical_indices)
    padding = (-pattern_data_offset) % 4
    pattern_data_offset += padding
    binary = bytearray(struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        HEADER_SIZE,
        tile_width,
        tile_height,
        len(pattern_indices),
        len(terrains),
        variants,
        len(boundaries),
        masks,
        len(objects),
        mapping_offset,
        pattern_data_offset,
        len(pattern_data),
        0,
    ))
    binary.extend(logical_indices)
    binary.extend(b"\0" * padding)
    binary.extend(pattern_data)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(binary)

    config_for_header = dict(config)
    config_for_header.update({"terrains": terrains, "boundaries": boundaries, "objects": objects})
    if header_path is None:
        header_path = output_path.with_suffix(".hpp")
    _write_header(header_path, config_for_header, len(pattern_indices))
    report = {
        "format": "pixel-twins-background-pack",
        "version": VERSION,
        "source_intermediate": _relative(intermediate_path, output_path.parent),
        "source_config": _relative(config_path, output_path.parent),
        "binary": output_path.name,
        "header": _relative(header_path, output_path.parent),
        "sha256": hashlib.sha256(binary).hexdigest(),
        "layout": {
            "byte_order": "little-endian",
            "header_bytes": HEADER_SIZE,
            "mapping_offset": mapping_offset,
            "mapping_bytes": len(logical_indices),
            "alignment_padding_bytes": padding,
            "pattern_data_offset": pattern_data_offset,
        },
        "summary": {
            "terrain_count": len(terrains),
            "boundary_count": len(boundaries),
            "object_count": len(objects),
            "logical_pattern_count": len(logical_indices),
            "physical_pattern_count": len(pattern_indices),
            "deduplicated_pattern_count": len(logical_indices) - len(pattern_indices),
            "pattern_bytes": len(pattern_data),
            "binary_bytes": len(binary),
            "max_patterns": max_patterns,
        },
        "terrains": [{"id": group["id"], "patterns": logical_report[index * variants:(index + 1) * variants]} for index, group in enumerate(terrains)],
        "boundaries": [{"id": group["id"], "patterns": logical_report[len(terrains) * variants + index * masks:len(terrains) * variants + (index + 1) * masks]} for index, group in enumerate(boundaries)],
        "objects": [{"id": group["id"], "pattern": logical_report[len(terrains) * variants + len(boundaries) * masks + index]} for index, group in enumerate(objects)],
    }
    output_path.with_suffix(".json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report
