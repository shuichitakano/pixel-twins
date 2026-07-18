from __future__ import annotations

import math
from collections import Counter
from typing import Dict, Iterable, List, Mapping, Sequence, Tuple

from PIL import Image

Rgb = Tuple[int, int, int]


def collect_colors(images: Iterable[Tuple[Image.Image, bool, int, int]], max_pixels_per_asset: int) -> Counter[Rgb]:
    colors: Counter[Rgb] = Counter()
    for image, uses_transparency, threshold, weight in images:
        rgba = image.convert("RGBA")
        if uses_transparency:
            asset_colors = Counter((r, g, b) for r, g, b, a in rgba.getdata() if a > threshold)
        else:
            asset_colors = Counter((r, g, b) for r, g, b, _ in rgba.getdata())
        total = sum(asset_colors.values())
        scale = min(1.0, max_pixels_per_asset / total) if total else 1.0
        colors.update(
            {
                color: max(1, round(count * scale * weight))
                for color, count in asset_colors.items()
            }
        )
    return colors


def optimize_palette(
    colors: Counter[Rgb],
    color_count: int,
    sample_pixels: int,
    exact_reserved: Mapping[Rgb, int],
    quantizer: str = "median_cut",
) -> List[Rgb]:
    dynamic = Counter({color: count for color, count in colors.items() if color not in exact_reserved})
    if not dynamic:
        return []
    if len(dynamic) <= color_count:
        return [color for color, _ in dynamic.most_common()]

    total = sum(dynamic.values())
    if total <= sample_pixels:
        sample = [color for color, count in dynamic.items() for _ in range(count)]
    else:
        # 累積頻度上へ等間隔に標本点を置き、最大長を厳密に守りながら使用頻度を保存する。
        ordered = dynamic.most_common()
        sample = []
        item_index = 0
        cumulative = ordered[0][1]
        for sample_index in range(sample_pixels):
            target = (sample_index + 0.5) * total / sample_pixels
            while cumulative < target and item_index + 1 < len(ordered):
                item_index += 1
                cumulative += ordered[item_index][1]
            sample.append(ordered[item_index][0])
    image = Image.new("RGB", (len(sample), 1))
    image.putdata(sample)
    methods = {
        "median_cut": Image.Quantize.MEDIANCUT,
        "max_coverage": Image.Quantize.MAXCOVERAGE,
        "fast_octree": Image.Quantize.FASTOCTREE,
    }
    quantized = image.quantize(colors=color_count, method=methods[quantizer], dither=Image.Dither.NONE)
    raw = quantized.getpalette() or []
    palette: List[Rgb] = []
    for index in range(color_count):
        base = index * 3
        color = tuple(raw[base : base + 3])
        if len(color) == 3 and color not in palette:
            palette.append(color)  # type: ignore[arg-type]
    return palette


def squared_distance(left: Rgb, right: Rgb) -> int:
    return sum((a - b) * (a - b) for a, b in zip(left, right))


class PaletteMapper:
    def __init__(
        self,
        palette: Sequence[Rgb],
        asset_indices: Sequence[int],
        exact_reserved: Mapping[Rgb, int],
        reserved_indices: Sequence[int] = (),
    ):
        self.palette = palette
        self.mapping_indices = sorted(set(asset_indices) | {index for index in reserved_indices if index != 0})
        self.exact_reserved = exact_reserved
        self.cache: Dict[Rgb, int] = {}

    def map_rgb(self, color: Rgb) -> int:
        if color in self.exact_reserved:
            return self.exact_reserved[color]
        cached = self.cache.get(color)
        if cached is not None:
            return cached
        if not self.mapping_indices:
            raise ValueError("アセット色の割り当て先がありません")
        result = min(self.mapping_indices, key=lambda index: squared_distance(color, self.palette[index]))
        self.cache[color] = result
        return result


def percentile(weighted_errors: Counter[float], total: int, ratio: float) -> float:
    target = max(1, math.ceil(total * ratio))
    count = 0
    for value, occurrences in sorted(weighted_errors.items()):
        count += occurrences
        if count >= target:
            return value
    return 0.0
