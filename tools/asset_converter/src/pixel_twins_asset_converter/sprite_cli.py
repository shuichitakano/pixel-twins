from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import List, Optional

from .sprite_pack import pack_sprites


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Pixel Twinsのtrim済みスプライトバイナリを生成")
    parser.add_argument("intermediate", type=Path, help="パレット変換後のintermediate.json")
    parser.add_argument("-o", "--output", type=Path, required=True, help="出力.bin")
    parser.add_argument("--header", type=Path, help="生成するC++アセットIDヘッダー")
    parser.add_argument("--namespace", default="assets", help="生成ヘッダーのC++ namespace")
    args = parser.parse_args(argv)
    try:
        report = pack_sprites(args.intermediate, args.output, args.header, args.namespace)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    summary = report["summary"]
    print(
        f"{summary['asset_count']} assets, {summary['frame_count']} frames, "
        f"{summary['binary_bytes']} bytes"
    )
    print(args.output.with_suffix(".json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
