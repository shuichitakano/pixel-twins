from __future__ import annotations

import math
import os
import shutil
import hashlib
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Tuple

from PIL import Image

from .manifest import AssetSpec, Manifest, Rgb, load_manifest
from .palette import PaletteMapper, collect_colors, optimize_palette, percentile, squared_distance
from .report import write_html, write_json


def _exact_reserved(manifest: Manifest) -> Dict[Rgb, int]:
    result: Dict[Rgb, int] = {}
    for index in sorted(manifest.reserved):
        if index == 0:
            continue
        color = manifest.reserved[index][1]
        if color not in result or index == 1:
            result[color] = index
    return result


def _make_palette(manifest: Manifest, images: List[Image.Image]) -> Tuple[List[Rgb], List[int]]:
    exact = _exact_reserved(manifest)
    collected = collect_colors(
        (
            (image, spec.kind == "sprite", spec.alpha_threshold, spec.palette_weight)
            for image, spec in zip(images, manifest.assets)
        ),
        manifest.max_pixels_per_asset,
    )
    capacity = manifest.asset_last - manifest.asset_first + 1
    dynamic = optimize_palette(
        collected,
        capacity,
        manifest.sample_pixels,
        exact,
        quantizer=manifest.quantizer,
    )
    palette: List[Rgb] = [(0, 0, 0)] * 256
    for index, (_, color) in manifest.reserved.items():
        palette[index] = color
    indices = list(range(manifest.asset_first, manifest.asset_first + len(dynamic)))
    for index, color in zip(indices, dynamic):
        palette[index] = color
    return palette, indices


def _convert_asset(
    image: Image.Image,
    spec: AssetSpec,
    mapper: PaletteMapper,
    palette: List[Rgb],
    output: Path,
) -> Dict[str, Any]:
    rgba = image.convert("RGBA")
    indices = []
    used = Counter()
    source_colors = set()
    errors: Counter[float] = Counter()
    total_opaque = 0
    changed = 0
    max_error = 0.0
    sum_error = 0.0
    sum_squared_error = 0.0
    for r, g, b, a in rgba.getdata():
        source = (r, g, b)
        if spec.kind == "sprite" and a <= spec.alpha_threshold:
            index = 0
        else:
            index = mapper.map_rgb(source)
            target = palette[index]
            error = math.sqrt(squared_distance(source, target))
            errors[error] += 1
            total_opaque += 1
            sum_error += error
            sum_squared_error += error * error
            max_error = max(max_error, error)
            changed += int(source != target)
            source_colors.add(source)
        indices.append(index)
        used[index] += 1

    indexed = Image.new("P", rgba.size)
    indexed.putpalette([channel for color in palette for channel in color])
    indexed.putdata(indices)
    asset_path = output / "assets" / f"{spec.asset_id}.png"
    save_args = {"transparency": 0} if spec.kind == "sprite" else {}
    indexed.save(asset_path, optimize=True, **save_args)

    source_preview = output / "previews" / f"{spec.asset_id}-source.png"
    difference_preview = output / "previews" / f"{spec.asset_id}-difference.png"
    rgba.save(source_preview, optimize=True)
    difference = Image.new("RGB", rgba.size)
    difference.putdata(
        [
            (0, 0, 0)
            if spec.kind == "sprite" and a <= spec.alpha_threshold
            else tuple(min(255, abs(source[i] - palette[index][i]) * 4) for i in range(3))
            for source, (_, _, _, a), index in zip(
                ((r, g, b) for r, g, b, _ in rgba.getdata()), rgba.getdata(), indices
            )
        ]
    )
    difference.save(difference_preview, optimize=True)

    relative_asset = asset_path.relative_to(output).as_posix()
    quality = {
        "metric": "Euclidean distance in RGB888 (0..441.67)",
        "opaque_pixels": total_opaque,
        "transparent_pixels": len(indices) - total_opaque,
        "mean_rgb_distance": sum_error / total_opaque if total_opaque else 0.0,
        "rms_rgb_distance": math.sqrt(sum_squared_error / total_opaque) if total_opaque else 0.0,
        "p95_rgb_distance": percentile(errors, total_opaque, 0.95),
        "max_rgb_distance": max_error,
        "changed_pixels": changed,
        "changed_pixel_ratio": changed / total_opaque if total_opaque else 0.0,
    }
    return {
        "id": spec.asset_id,
        "kind": spec.kind,
        "pack_kind": spec.pack_kind,
        "frame": spec.frame,
        "source": Path(os.path.relpath(spec.source, output)).as_posix(),
        "indexed_png": relative_asset,
        "width": image.width,
        "height": image.height,
        "alpha_threshold": spec.alpha_threshold,
        "palette_weight": spec.palette_weight,
        "source_color_count": len(source_colors),
        "indexed_color_count": len(used),
        "used_indices": sorted(used),
        "index_usage": {str(index): count for index, count in sorted(used.items())},
        "quality": quality,
        "sizes": {
            "source_file_bytes": spec.source.stat().st_size,
            "indexed_pixel_bytes": image.width * image.height,
            "indexed_png_bytes": asset_path.stat().st_size,
        },
        "previews": {
            "source": source_preview.relative_to(output).as_posix(),
            "difference": difference_preview.relative_to(output).as_posix(),
        },
    }


def build(manifest_path: Path, output: Path, clean: bool = False) -> Dict[str, Any]:
    manifest = load_manifest(manifest_path.resolve())
    if clean and output.exists():
        shutil.rmtree(output)
    (output / "assets").mkdir(parents=True, exist_ok=True)
    (output / "previews").mkdir(parents=True, exist_ok=True)
    images = []
    try:
        for spec in manifest.assets:
            images.append(Image.open(spec.source))
        palette, asset_indices = _make_palette(manifest, images)
        mapper = PaletteMapper(
            palette,
            asset_indices,
            _exact_reserved(manifest),
            reserved_indices=list(manifest.reserved),
        )
        assets = [
            _convert_asset(image, spec, mapper, palette, output)
            for image, spec in zip(images, manifest.assets)
        ]
    finally:
        for image in images:
            image.close()

    used_indices = sorted({index for asset in assets for index in asset["used_indices"]})
    global_usage = Counter()
    for asset in assets:
        global_usage.update({int(index): count for index, count in asset["index_usage"].items()})
    palette_entries = []
    for index, color in enumerate(palette):
        if index in manifest.reserved:
            role = "reserved"
            name = manifest.reserved[index][0]
        elif index in asset_indices:
            role = "asset"
            name = ""
        else:
            role = "unused"
            name = ""
        palette_entries.append(
            {
                "index": index,
                "rgb": list(color),
                "role": role,
                "name": name,
                "usage_pixels": global_usage[index],
            }
        )
    summary = {
        "asset_count": len(assets),
        "used_palette_entries": len(used_indices),
        "reserved_entries": len(manifest.reserved),
        "generated_asset_entries": len(asset_indices),
        "palette_rgb888_bytes": 768,
        "source_file_bytes": sum(a["sizes"]["source_file_bytes"] for a in assets),
        "indexed_pixel_bytes": sum(a["sizes"]["indexed_pixel_bytes"] for a in assets),
        "indexed_png_bytes": sum(a["sizes"]["indexed_png_bytes"] for a in assets),
    }
    palette_binary = bytes(channel for color in palette for channel in color)
    palette_path = output / "palette.bin"
    palette_path.write_bytes(palette_binary)
    report = {
        "format": "pixel-twins-indexed-assets",
        "version": 1,
        "name": manifest.name,
        "manifest": Path(os.path.relpath(manifest_path.resolve(), output)).as_posix(),
        "quantizer": manifest.quantizer,
        "palette_binary": {
            "file": palette_path.name,
            "format": "RGB888",
            "entry_count": 256,
            "bytes": len(palette_binary),
            "sha256": hashlib.sha256(palette_binary).hexdigest(),
        },
        "palette": palette_entries,
        "summary": summary,
        "assets": assets,
    }
    write_json(report, output / "intermediate.json")
    write_json(report, output / "report.json")
    write_html(report, output / "report.html")
    return report
