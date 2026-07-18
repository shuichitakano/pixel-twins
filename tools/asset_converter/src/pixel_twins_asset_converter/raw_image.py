from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
from typing import Any, Dict

from PIL import Image


def pack_raw_image(intermediate_path: Path, asset_id: str, output_path: Path) -> Dict[str, Any]:
    intermediate_path = intermediate_path.resolve()
    intermediate = json.loads(intermediate_path.read_text(encoding="utf-8"))
    asset = next((item for item in intermediate["assets"] if item["id"] == asset_id), None)
    if asset is None:
        raise ValueError(f"アセットがありません: {asset_id}")
    image_path = intermediate_path.parent / asset["indexed_png"]
    with Image.open(image_path) as image:
        if image.mode != "P":
            raise ValueError(f"{asset_id}: Pモード画像ではありません")
        pixels = bytes(image.getdata())
        width, height = image.size
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(pixels)
    report = {
        "format": "pixel-twins-raw-image",
        "version": 1,
        "source_intermediate": Path(os.path.relpath(intermediate_path, output_path.parent)).as_posix(),
        "asset_id": asset_id,
        "binary": output_path.name,
        "width": width,
        "height": height,
        "pixel_bytes": len(pixels),
        "sha256": hashlib.sha256(pixels).hexdigest(),
    }
    output_path.with_suffix(".json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return report
