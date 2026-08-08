# prototype-2026-07

元のKiCadプロジェクトで2026年7月9日に生成されていたGerberとdrillのスナップショットです。

## 内容

- `twin-led-driver-gerber.zip` — Gerber、PTH/NPTH drill、drill map

ZIPにはF.Cu、B.Cu、F.Mask、B.Mask、F.Silkscreen、B.Silkscreen、F.Paste、B.Paste、
Edge.Cuts、PTH、NPTHが含まれます。部品表は[ハードウェアページのBOM](../../README.md#bom)を
参照してください。

## 注意点

- 電源入力は基板シルク上で`+5V 6.2A`と表記されています。電源、配線、コネクターなどの
  定格と極性を実機構成に合わせて確認してください。
- 正式な製造リリースではありません。発注前にGerber Viewerで内容を再確認してください。
