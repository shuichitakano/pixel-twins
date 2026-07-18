from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .raw_image import pack_raw_image


def main() -> int:
    parser = argparse.ArgumentParser(description="8-bitインデックス画像を生画素バイナリへ変換")
    parser.add_argument("intermediate", type=Path)
    parser.add_argument("asset_id")
    parser.add_argument("-o", "--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = pack_raw_image(args.intermediate, args.asset_id, args.output)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    print(f"{report['width']}x{report['height']}, {report['pixel_bytes']} bytes")
    print(args.output.with_suffix(".json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
