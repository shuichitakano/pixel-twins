from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

Rgb = Tuple[int, int, int]


class ManifestError(ValueError):
    pass


@dataclass(frozen=True)
class AssetSpec:
    asset_id: str
    source: Path
    kind: str
    alpha_threshold: int
    palette_weight: int
    pack_kind: str
    frame: Optional[Dict[str, int]]


@dataclass(frozen=True)
class Manifest:
    name: str
    reserved: Dict[int, Tuple[str, Rgb]]
    asset_first: int
    asset_last: int
    sample_pixels: int
    max_pixels_per_asset: int
    quantizer: str
    assets: List[AssetSpec]


_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")
_SYSTEM_COLORS = {
    0: ("transparent/background black", (0, 0, 0)),
    1: ("drawable black", (0, 0, 0)),
    255: ("white", (255, 255, 255)),
}


def _rgb(value: object, field: str) -> Rgb:
    if isinstance(value, str):
        text = value.removeprefix("#")
        if len(text) == 6:
            try:
                return tuple(int(text[i : i + 2], 16) for i in (0, 2, 4))  # type: ignore[return-value]
            except ValueError:
                pass
    if isinstance(value, list) and len(value) == 3 and all(isinstance(v, int) for v in value):
        rgb = tuple(value)
        if all(0 <= v <= 255 for v in rgb):
            return rgb  # type: ignore[return-value]
    raise ManifestError(f"{field}は#rrggbbまたは0〜255のRGB配列で指定してください")


def load_manifest(path: Path) -> Manifest:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ManifestError(f"マニフェストを読み込めません: {exc}") from exc
    if raw.get("version") != 1:
        raise ManifestError("versionは1を指定してください")

    palette = raw.get("palette", {})
    asset_range = palette.get("asset_range", [2, 254])
    if (
        not isinstance(asset_range, list)
        or len(asset_range) != 2
        or not all(isinstance(v, int) for v in asset_range)
    ):
        raise ManifestError("palette.asset_rangeは[first, last]で指定してください")
    first, last = asset_range
    if first < 2 or last > 254 or first > last:
        raise ManifestError("asset_rangeは2〜254の範囲で指定してください")

    reserved = dict(_SYSTEM_COLORS)
    for item in palette.get("reserved", []):
        try:
            index = item["index"]
            name = item["name"]
            color = _rgb(item["color"], f"reserved[{index}].color")
        except (KeyError, TypeError) as exc:
            raise ManifestError("予約色にはindex、name、colorが必要です") from exc
        if not isinstance(index, int) or not 0 <= index <= 255:
            raise ManifestError("予約色indexは0〜255で指定してください")
        if index in _SYSTEM_COLORS and color != _SYSTEM_COLORS[index][1]:
            raise ManifestError(f"システム予約色{index}のRGB値は変更できません")
        reserved[index] = (str(name), color)

    overlap = sorted(index for index in reserved if first <= index <= last)
    if overlap:
        raise ManifestError(f"asset_rangeと予約色が重複しています: {overlap}")

    assets = []
    seen = set()
    for item in raw.get("assets", []):
        try:
            asset_id = item["id"]
            source = item["source"]
            kind = item["kind"]
        except (KeyError, TypeError) as exc:
            raise ManifestError("各アセットにはid、source、kindが必要です") from exc
        if not isinstance(asset_id, str) or not _ID_RE.fullmatch(asset_id):
            raise ManifestError(f"不正なアセットidです: {asset_id!r}")
        if asset_id in seen:
            raise ManifestError(f"アセットidが重複しています: {asset_id}")
        if kind not in ("sprite", "background"):
            raise ManifestError(f"{asset_id}: kindはspriteまたはbackgroundです")
        threshold = item.get("alpha_threshold", 0)
        if not isinstance(threshold, int) or not 0 <= threshold <= 255:
            raise ManifestError(f"{asset_id}: alpha_thresholdは0〜255で指定してください")
        palette_weight = item.get("palette_weight", 1)
        if not isinstance(palette_weight, int) or not 1 <= palette_weight <= 100:
            raise ManifestError(f"{asset_id}: palette_weightは1〜100の整数で指定してください")
        pack_kind = item.get("pack_kind", kind)
        if pack_kind not in ("sprite", "background", "font"):
            raise ManifestError(f"{asset_id}: pack_kindが不正です")
        frame = item.get("frame")
        if frame is not None:
            if not isinstance(frame, dict) or set(frame) != {"width", "height"}:
                raise ManifestError(f"{asset_id}: frameにはwidthとheightを指定してください")
            if not all(isinstance(frame[key], int) and 1 <= frame[key] <= 255 for key in frame):
                raise ManifestError(f"{asset_id}: frame寸法は1〜255で指定してください")
        source_path = (path.parent / str(source)).resolve()
        if not source_path.is_file():
            raise ManifestError(f"{asset_id}: 画像がありません: {source_path}")
        seen.add(asset_id)
        assets.append(
            AssetSpec(asset_id, source_path, kind, threshold, palette_weight, pack_kind, frame)
        )
    if not assets:
        raise ManifestError("assetsを1件以上指定してください")

    sample_pixels = palette.get("sample_pixels", 1_000_000)
    if not isinstance(sample_pixels, int) or sample_pixels < 256:
        raise ManifestError("palette.sample_pixelsは256以上で指定してください")
    max_pixels_per_asset = palette.get("max_pixels_per_asset", 65_536)
    if not isinstance(max_pixels_per_asset, int) or max_pixels_per_asset < 256:
        raise ManifestError("palette.max_pixels_per_assetは256以上で指定してください")
    quantizer = palette.get("quantizer", "median_cut")
    if quantizer not in ("median_cut", "max_coverage", "fast_octree"):
        raise ManifestError("palette.quantizerはmedian_cut、max_coverage、fast_octreeのいずれかです")
    return Manifest(
        name=str(raw.get("name", path.stem)),
        reserved=reserved,
        asset_first=first,
        asset_last=last,
        sample_pixels=sample_pixels,
        max_pixels_per_asset=max_pixels_per_asset,
        quantizer=quantizer,
        assets=assets,
    )
