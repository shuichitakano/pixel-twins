from collections import Counter
import json
from pathlib import Path

from PIL import Image

from pixel_twins_asset_converter.manifest import ManifestError, load_manifest
from pixel_twins_asset_converter.palette import PaletteMapper, optimize_palette
from pixel_twins_asset_converter.pipeline import build


def _write_manifest(path: Path, assets):
    path.write_text(
        json.dumps(
            {
                "version": 1,
                "name": "test",
                "palette": {
                    "asset_range": [32, 40],
                    "max_pixels_per_asset": 256,
                    "reserved": [
                        {"index": 2, "name": "effect", "color": "#ff0000"},
                        {"index": 18, "name": "font", "color": "#00ff00"},
                    ],
                },
                "assets": assets,
            }
        ),
        encoding="utf-8",
    )


def test_build_keeps_shared_palette_and_transparency(tmp_path):
    sprite = Image.new("RGBA", (3, 1))
    sprite.putdata([(10, 20, 30, 0), (0, 0, 0, 255), (255, 0, 0, 255)])
    sprite.save(tmp_path / "sprite.png")
    background = Image.new("RGB", (2, 1))
    background.putdata([(12, 34, 56), (90, 100, 110)])
    background.save(tmp_path / "background.png")
    manifest = tmp_path / "assets.json"
    _write_manifest(
        manifest,
        [
            {"id": "hero", "kind": "sprite", "source": "sprite.png"},
            {"id": "map", "kind": "background", "source": "background.png"},
        ],
    )

    report = build(manifest, tmp_path / "out")
    hero = Image.open(tmp_path / "out/assets/hero.png")
    bg = Image.open(tmp_path / "out/assets/map.png")

    assert hero.mode == "P" and bg.mode == "P"
    assert list(hero.getdata()) == [0, 1, 2]
    assert set(bg.getdata()).issubset(set(range(32, 41)))
    assert hero.getpalette() == bg.getpalette()
    assert report["summary"]["indexed_pixel_bytes"] == 5
    assert report["summary"]["palette_rgb888_bytes"] == 768
    assert report["palette_binary"]["bytes"] == 768
    assert (tmp_path / "out/palette.bin").read_bytes() == bytes(
        channel for entry in report["palette"] for channel in entry["rgb"]
    )
    assert sum(entry["usage_pixels"] for entry in report["palette"]) == 5
    assert (tmp_path / "out/report.html").is_file()
    assert (tmp_path / "out/report.json").is_file()


def test_manifest_rejects_reserved_asset_overlap(tmp_path):
    image = Image.new("RGB", (1, 1))
    image.save(tmp_path / "image.png")
    manifest = tmp_path / "assets.json"
    manifest.write_text(
        json.dumps(
            {
                "version": 1,
                "palette": {
                    "asset_range": [2, 254],
                    "reserved": [{"index": 2, "name": "effect", "color": "#ff0000"}],
                },
                "assets": [{"id": "map", "kind": "background", "source": "image.png"}],
            }
        ),
        encoding="utf-8",
    )

    try:
        load_manifest(manifest)
    except ManifestError as exc:
        assert "重複" in str(exc)
    else:
        raise AssertionError("ManifestError was not raised")


def test_palette_sampling_honors_limit_and_favors_frequent_colors():
    colors = Counter({(255, 0, 0): 10000, (0, 255, 0): 100, (0, 0, 255): 1})

    palette = optimize_palette(colors, color_count=2, sample_pixels=256, exact_reserved={})

    assert len(palette) == 2
    assert any(red > green and red > blue for red, green, blue in palette)


def test_reserved_colors_are_available_to_nearest_mapping():
    palette = [(0, 0, 0)] * 256
    palette[2] = (255, 0, 0)
    palette[32] = (0, 0, 255)
    mapper = PaletteMapper(palette, [32], {}, reserved_indices=[0, 1, 2, 255])

    assert mapper.map_rgb((240, 10, 10)) == 2
