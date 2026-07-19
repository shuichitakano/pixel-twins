#!/usr/bin/env python3
"""プロトタイプBGM JSONをPixel TwinsのC++シーケンスへ変換する。"""

import argparse
import bisect
import json
from pathlib import Path

BLOCK_SECONDS = 512 / 48000
TRACK_INSTRUMENT = {"Bass": 0, "Synth": 1, "Lead": 2, "LeadDelay": 3, "Keyboard": 4}
TRACK_ID = {"Bass": 0, "Drums": 1, "Percussion": 2, "Synth": 3,
            "Lead": 4, "LeadDelay": 5, "Keyboard": 6}


def instrument(track, note):
    if track not in ("Drums", "Percussion"):
        return TRACK_INSTRUMENT[track]
    return {36: 5, 38: 6, 42: 7, 46: 8, 45: 9, 50: 10}.get(note, 11)


def convert(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    seconds_per_tick = 60.0 / data["bpm"] / data["ppqn"]
    events = []
    for track in data["tracks"]:
        for event in track["events"]:
            block = round(event["tick"] * seconds_per_tick / BLOCK_SECONDS)
            duration = max(1, round(event["duration"] * seconds_per_tick / BLOCK_SECONDS))
            events.append((block, duration, event["note"], event["velocity"], event["voice"],
                           instrument(track["name"], event["note"]), TRACK_ID[track["name"]]))
    events.sort(key=lambda value: value[0])
    end_block = max(1, round(data["endTick"] * seconds_per_tick / BLOCK_SECONDS))
    loop = data.get("playMode") != "oneshot" and data.get("loopStartTick") is not None
    loop_start_tick = data.get("loopStartTick") or 0
    loop_start = round(loop_start_tick * seconds_per_tick / BLOCK_SECONDS)
    loop_index = bisect.bisect_left([event[0] for event in events], loop_start)
    return data["title"], events, end_block, loop_start, loop_index, loop


def identifier(name):
    result = "".join(word.capitalize() for word in name.replace("-", "_").split("_"))
    if not result.isidentifier():
        raise ValueError(f"不正なシーケンス名です: {name}")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sequence", action="append", nargs=2, metavar=("NAME", "JSON"), required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--namespace", default="wizward::audio")
    args = parser.parse_args()
    converted = [(identifier(name), convert(Path(path))) for name, path in args.sequence]
    openings = "\n".join(f"namespace {part} {{" for part in args.namespace.split("::"))
    closings = "\n".join(f"}} // namespace {part}" for part in reversed(args.namespace.split("::")))
    declarations = "\n".join(f"extern const pixel_twins::Sequence k{key};" for key, _ in converted)
    header = f'''// sequence_converter.pyによる生成物。編集しないでください。\n#pragma once\n\n#include "pixel_twins/sequencer.hpp"\n\n{openings}\n\nextern const pixel_twins::SequenceInstrument kBgmInstruments[12];\n{declarations}\n\n{closings}\n'''
    definitions = []
    for key, (_, events, end, loop_start, loop_index, loop) in converted:
        rows = ",\n".join(f"    {{{b}, {d}, {n}, {v}, {voice}, {inst}, {track}}}" for b, d, n, v, voice, inst, track in events)
        definitions.append(f'''namespace {{\nconstexpr pixel_twins::SequenceEvent k{key}Events[]{{\n{rows}\n}};\n}}\nconst pixel_twins::Sequence k{key}{{k{key}Events, {len(events)}, kBgmInstruments, 12, {end}, {loop_start}, {loop_index}, {str(loop).lower()}}};''')
    source = f'''// sequence_converter.pyによる生成物。編集しないでください。\n#include "bgm_data.hpp"\n\n{openings}\n\n{chr(10).join(definitions)}\n\n{closings}\n'''
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.source.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(header, encoding="utf-8")
    args.source.write_text(source, encoding="utf-8")
    print(sum(len(item[1][1]) for item in converted), "events")


if __name__ == "__main__":
    main()
