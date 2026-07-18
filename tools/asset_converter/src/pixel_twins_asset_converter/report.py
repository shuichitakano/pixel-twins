from __future__ import annotations

import html
import json
from pathlib import Path
from typing import Any, Dict


def write_html(report: Dict[str, Any], path: Path) -> None:
    rows = []
    for asset in report["assets"]:
        quality = asset["quality"]
        sizes = asset["sizes"]
        rows.append(
            "<tr>"
            f"<td><strong>{html.escape(asset['id'])}</strong><br>{html.escape(asset['kind'])}<br>"
            f"{asset['width']}×{asset['height']}</td>"
            f"<td><img src=\"{html.escape(asset['previews']['source'])}\"></td>"
            f"<td><img src=\"{html.escape(asset['indexed_png'])}\"></td>"
            f"<td><img src=\"{html.escape(asset['previews']['difference'])}\"></td>"
            f"<td>{asset['source_color_count']} → {asset['indexed_color_count']}色<br>"
            f"平均誤差 {quality['mean_rgb_distance']:.2f}<br>"
            f"P95 {quality['p95_rgb_distance']:.2f}<br>最大 {quality['max_rgb_distance']:.2f}<br>"
            f"変更 {quality['changed_pixel_ratio']:.1%}</td>"
            f"<td>元 {sizes['source_file_bytes']:,} B<br>"
            f"raw推定 {sizes['indexed_pixel_bytes']:,} B<br>"
            f"PNG {sizes['indexed_png_bytes']:,} B</td>"
            "</tr>"
        )
    summary = report["summary"]
    document = f"""<!doctype html>
<html lang="ja"><head><meta charset="utf-8"><title>{html.escape(report['name'])} 減色レポート</title>
<style>
body{{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;margin:2rem;color:#222}}
table{{border-collapse:collapse;width:100%}}th,td{{border:1px solid #bbb;padding:.55rem;vertical-align:top}}
th{{background:#eee}}img{{max-width:256px;image-rendering:pixelated;background:#ddd}}
.swatches{{display:flex;flex-wrap:wrap;gap:3px}}.swatch{{width:24px;height:24px;border:1px solid #777}}
</style></head><body>
<h1>{html.escape(report['name'])} 減色レポート</h1>
<p>アセット {summary['asset_count']}件 / 使用パレット {summary['used_palette_entries']}色 /
raw画素 {summary['indexed_pixel_bytes']:,} B / 共有パレット 768 B /
中間PNG {summary['indexed_png_bytes']:,} B</p>
<h2>パレット</h2><div class="swatches">{_swatches(report)}</div>
<h2>アセット</h2><table><thead><tr><th>アセット</th><th>変換前</th><th>変換後</th><th>差分</th><th>品質</th><th>サイズ</th></tr></thead>
<tbody>{''.join(rows)}</tbody></table></body></html>"""
    path.write_text(document, encoding="utf-8")


def _swatches(report: Dict[str, Any]) -> str:
    parts = []
    for entry in report["palette"]:
        if entry["role"] == "unused":
            continue
        r, g, b = entry["rgb"]
        title = html.escape(
            f"{entry['index']}: #{r:02x}{g:02x}{b:02x} "
            f"({entry['role']}, {entry['usage_pixels']} px)"
        )
        parts.append(f'<span class="swatch" title="{title}" style="background:rgb({r},{g},{b})"></span>')
    return "".join(parts)


def write_json(data: Dict[str, Any], path: Path) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
