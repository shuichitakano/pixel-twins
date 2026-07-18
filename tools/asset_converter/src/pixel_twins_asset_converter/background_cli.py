from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import List, Optional

from .background_pack import pack_background


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Pixel Twinsの固定BGアトラスを生成")
    parser.add_argument("config", type=Path, help="BGパック設定JSON")
    parser.add_argument("-o", "--output", type=Path, required=True, help="出力.bin")
    parser.add_argument("--header", type=Path, help="生成するC++ヘッダー")
    args = parser.parse_args(argv)
    try:
        report = pack_background(args.config, args.output, args.header)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    summary = report["summary"]
    print(
        f"{summary['logical_pattern_count']} logical, "
        f"{summary['physical_pattern_count']} physical patterns, "
        f"{summary['binary_bytes']} bytes"
    )
    print(args.output.with_suffix(".json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
