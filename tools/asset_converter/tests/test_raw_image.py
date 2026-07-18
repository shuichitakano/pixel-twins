import json

from PIL import Image

from pixel_twins_asset_converter.raw_image import pack_raw_image


def test_raw_image_writes_index_bytes(tmp_path):
    (tmp_path / "assets").mkdir()
    image = Image.new("P", (2, 2))
    image.putpalette([channel for index in range(256) for channel in (index, index, index)])
    image.putdata([1, 2, 3, 4])
    image.save(tmp_path / "assets/title.png")
    (tmp_path / "intermediate.json").write_text(json.dumps({
        "assets": [{"id": "title", "indexed_png": "assets/title.png"}]
    }), encoding="utf-8")

    report = pack_raw_image(tmp_path / "intermediate.json", "title", tmp_path / "title.bin")

    assert (tmp_path / "title.bin").read_bytes() == bytes([1, 2, 3, 4])
    assert report["width"] == 2 and report["height"] == 2
