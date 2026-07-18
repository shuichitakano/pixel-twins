#!/usr/bin/env python3
"""SFX定義JSONをPixel TwinsのC++固定プリセットへ変換する。"""

import argparse
import json
from pathlib import Path

WAVES = {"sine", "bell", "square", "pulse", "thinPulse", "saw", "triangle", "spark", "noise"}


def ident(name):
    value = "".join(word.capitalize() for word in name.replace("-", "_").split("_"))
    if not value.isidentifier():
        raise ValueError(f"不正なSFX名です: {name}")
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--namespace", default="wizward::audio")
    args = parser.parse_args()
    presets = json.loads(args.input.read_text(encoding="utf-8"))["presets"]
    parts = args.namespace.split("::")
    openings = "\n".join(f"namespace {part} {{" for part in parts)
    closings = "\n".join(f"}} // namespace {part}" for part in reversed(parts))
    declarations = []
    definitions = []
    for preset in presets:
        name = ident(preset["name"])
        wave = preset["wave"]
        if wave not in WAVES:
            raise ValueError(f"不明な波形です: {wave}")
        env = preset.get("envelope", {})
        f = lambda key, default: float(preset.get(key, default))
        definitions.append(
            f"const pixel_twins::SfxPreset k{name}{{"
            f"{{&pixel_twins::kStandardWaves.{wave}, "
            f"{{{float(env.get('attack', 0))}F, {float(env.get('decay', 0))}F, "
            f"{float(env.get('sustain', 1))}F, {float(env.get('release', 0))}F}}, "
            f"{f('volume', 1)}F, {f('pan', 0)}F}}, "
            f"{f('frequency', 440)}F, {f('endFrequency', preset.get('frequency', 440))}F, "
            f"{f('pitchSeconds', 0)}F, {f('holdSeconds', 0)}F, {f('velocity', 1)}F, "
            f"{int(preset.get('priority', 0))}}};")
        declarations.append(f"extern const pixel_twins::SfxPreset k{name};")
    header = "// sfx_converter.pyによる生成物。編集しないでください。\n#pragma once\n\n#include \"pixel_twins/audio_system.hpp\"\n\n" + openings + "\n\n" + "\n".join(declarations) + "\n\n" + closings + "\n"
    source = "// sfx_converter.pyによる生成物。編集しないでください。\n#include \"sfx_data.hpp\"\n#include \"pixel_twins/sound_waves.hpp\"\n\n" + openings + "\n\n" + "\n".join(definitions) + "\n\n" + closings + "\n"
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.source.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(header, encoding="utf-8")
    args.source.write_text(source, encoding="utf-8")


if __name__ == "__main__":
    main()
