from pathlib import Path
import tempfile
import unittest

from PIL import Image

import font_converter


OUTLINE = (17, 19, 21, 255)
BODY = (241, 234, 216, 255)


class FontConverterTest(unittest.TestCase):
    def test_reads_two_planes_msb_first(self):
        with tempfile.TemporaryDirectory() as directory:
            image_path = Path(directory) / "font.png"
            image = Image.new("RGBA", (8, 9), (0, 0, 0, 0))
            image.putpixel((0, 0), OUTLINE)
            image.putpixel((7, 0), BODY)
            image.save(image_path)

            glyphs = font_converter.read_glyphs(
                image_path,
                columns=1,
                first=65,
                count=1,
                outline_color=OUTLINE,
                body_color=BODY,
            )

            self.assertEqual(glyphs[0][0], (0x80, 0x01))
            self.assertEqual(glyphs[0][1:], ((0, 0),) * 8)

    def test_rejects_unknown_opaque_color(self):
        with tempfile.TemporaryDirectory() as directory:
            image_path = Path(directory) / "font.png"
            image = Image.new("RGBA", (8, 9), (0, 0, 0, 0))
            image.putpixel((3, 4), (1, 2, 3, 255))
            image.save(image_path)

            with self.assertRaisesRegex(ValueError, "codepoint=65, x=3, y=4"):
                font_converter.read_glyphs(
                    image_path,
                    columns=1,
                    first=65,
                    count=1,
                    outline_color=OUTLINE,
                    body_color=BODY,
                )

    def test_generated_declaration_uses_extern(self):
        header = font_converter.render_header("wizward::assets", "kFont")
        self.assertIn("extern const pixel_twins::BitmapFont kFont;", header)
        self.assertNotIn("Glyph kGlyphs", header)


if __name__ == "__main__":
    unittest.main()
