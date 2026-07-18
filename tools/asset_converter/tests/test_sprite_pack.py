import json
import struct

from PIL import Image

from pixel_twins_asset_converter.sprite_pack import (
    ASSET_FORMAT,
    ASSET_SIZE,
    FRAME_FORMAT,
    HEADER_FORMAT,
    HEADER_SIZE,
    MAGIC,
    pack_sprites,
)


def test_sprite_pack_trims_each_frame_and_deduplicates_pixels(tmp_path):
    sheet = Image.new("P", (8, 4))
    sheet.putpalette([value for index in range(256) for value in (index, index, index)])
    pixels = [0] * 32
    pixels[1 + 1 * 8] = 5
    pixels[2 + 1 * 8] = 5
    pixels[4 + 0 * 8] = 5
    pixels[5 + 0 * 8] = 5
    sheet.putdata(pixels)
    (tmp_path / "assets").mkdir()
    sheet.save(tmp_path / "assets/test.png")
    intermediate = {
        "assets": [
            {
                "id": "test",
                "pack_kind": "sprite",
                "frame": {"width": 4, "height": 4},
                "indexed_png": "assets/test.png",
            }
        ]
    }
    (tmp_path / "intermediate.json").write_text(json.dumps(intermediate), encoding="utf-8")

    report = pack_sprites(tmp_path / "intermediate.json", tmp_path / "sprites.bin")
    binary = (tmp_path / "sprites.bin").read_bytes()
    header = struct.unpack_from(HEADER_FORMAT, binary)
    asset = struct.unpack_from(ASSET_FORMAT, binary, HEADER_SIZE)
    frame0 = struct.unpack_from(FRAME_FORMAT, binary, HEADER_SIZE + ASSET_SIZE)
    frame1 = struct.unpack_from(FRAME_FORMAT, binary, HEADER_SIZE + ASSET_SIZE + 8)

    assert header[0] == MAGIC
    assert asset[1:6] == (2, 2, 1, 4, 4)
    assert frame0 == (0, 1, 1, 2, 1)
    assert frame1 == (0, 0, 0, 2, 1)
    assert report["summary"]["untrimmed_pixel_bytes"] == 32
    assert report["summary"]["trimmed_pixel_bytes"] == 4
    assert report["summary"]["unique_pixel_bytes"] == 2
    assert report["summary"]["transparent_border_saved_bytes"] == 28
    assert report["summary"]["dedup_saved_bytes"] == 2
    header_text = (tmp_path / "sprites.hpp").read_text(encoding="utf-8")
    assert "enum class SpriteAssetId" in header_text
    assert "Test = 0" in header_text
