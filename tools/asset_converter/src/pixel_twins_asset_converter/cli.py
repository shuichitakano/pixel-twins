from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import List, Optional

from .manifest import ManifestError
from .pipeline import build


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Pixel Twins共通パレット・中間アセット生成")
    parser.add_argument("manifest", type=Path, help="JSONマニフェスト")
    parser.add_argument("-o", "--output", type=Path, default=Path("build/assets"))
    parser.add_argument("--clean", action="store_true", help="出力先を削除してから生成する")
    args = parser.parse_args(argv)
    try:
        report = build(args.manifest, args.output, clean=args.clean)
    except (ManifestError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    summary = report["summary"]
    print(
        f"{summary['asset_count']} assets, {summary['used_palette_entries']} palette entries, "
        f"{summary['indexed_pixel_bytes']} raw indexed bytes"
    )
    print(args.output / "report.html")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

