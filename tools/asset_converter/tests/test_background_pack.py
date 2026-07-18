import json
import struct

from PIL import Image

from pixel_twins_asset_converter.background_pack import (
    HEADER_FORMAT,
    HEADER_SIZE,
    MAGIC,
    pack_background,
)


def _image(path, value):
    image = Image.new("P", (8, 8), value)
    image.putpalette([channel for index in range(256) for channel in (index, index, index)])
    image.save(path)


def test_background_pack_deduplicates_and_writes_mapping(tmp_path):
    (tmp_path / "assets").mkdir()
    _image(tmp_path / "assets/a.png", 3)
    _image(tmp_path / "assets/b.png", 4)
    _image(tmp_path / "assets/c.png", 3)
    intermediate = {
        "assets": [
            {"id": name, "pack_kind": "background", "indexed_png": f"assets/{name}.png"}
            for name in ("a", "b", "c")
        ]
    }
    (tmp_path / "intermediate.json").write_text(json.dumps(intermediate), encoding="utf-8")
    config = {
        "version": 1,
        "intermediate": "intermediate.json",
        "tile_width": 8,
        "tile_height": 8,
        "max_patterns": 2,
        "variants_per_terrain": 2,
        "masks_per_boundary": 1,
        "cpp_namespace": "test::assets",
        "terrains": [{"id": "field", "assets": ["a", "b"]}],
        "boundaries": [{"id": "edge", "assets": ["c"]}],
        "objects": [{"id": "rock", "assets": ["b"]}],
    }
    (tmp_path / "background.json").write_text(json.dumps(config), encoding="utf-8")

    report = pack_background(tmp_path / "background.json", tmp_path / "background.bin")
    binary = (tmp_path / "background.bin").read_bytes()
    header = struct.unpack_from(HEADER_FORMAT, binary)

    assert header[0] == MAGIC and header[2] == HEADER_SIZE
    assert header[5:11] == (2, 1, 2, 1, 1, 1)
    assert binary[HEADER_SIZE:HEADER_SIZE + 4] == bytes([0, 1, 0, 1])
    assert report["summary"]["logical_pattern_count"] == 4
    assert report["summary"]["physical_pattern_count"] == 2
    assert report["summary"]["deduplicated_pattern_count"] == 2
    assert "enum class TerrainId" in (tmp_path / "background.hpp").read_text(encoding="utf-8")


def test_background_pack_rejects_pattern_overflow(tmp_path):
    (tmp_path / "assets").mkdir()
    for name, value in (("a", 1), ("b", 2), ("c", 3)):
        _image(tmp_path / f"assets/{name}.png", value)
    (tmp_path / "intermediate.json").write_text(json.dumps({
        "assets": [{"id": name, "pack_kind": "background", "indexed_png": f"assets/{name}.png"} for name in ("a", "b", "c")]
    }), encoding="utf-8")
    config = {
        "version": 1, "intermediate": "intermediate.json", "tile_width": 8, "tile_height": 8,
        "max_patterns": 2, "variants_per_terrain": 1, "masks_per_boundary": 1,
        "terrains": [{"id": "a", "assets": ["a"]}],
        "boundaries": [{"id": "b", "assets": ["b"]}],
        "objects": [{"id": "c", "assets": ["c"]}],
    }
    (tmp_path / "background.json").write_text(json.dumps(config), encoding="utf-8")

    try:
        pack_background(tmp_path / "background.json", tmp_path / "background.bin")
    except ValueError as exc:
        assert "上限2" in str(exc)
    else:
        raise AssertionError("ValueError was not raised")
