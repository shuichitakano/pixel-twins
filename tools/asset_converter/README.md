# Pixel Twinsアセットコンバーター

同じフレームで使用する画像群から共通パレットを生成し、Pixel Twinsのバイナリ生成前の
中間アセットと減色レポートを出力します。この段階ではスプライトやBGの実機用バイナリ化は
行いません。

## セットアップと実行

```sh
cd tools/asset_converter
python3 -m venv .venv
.venv/bin/pip install -e '.[dev]'
.venv/bin/pixel-twins-assets path/to/palette-set.json -o build/gameplay --clean
```

`examples/palette-set.json`は予約色とアセット記述のひな型です。参照する画像パスと固定色を
プロジェクトに合わせて変更してから使用してください。

出力内容は次のとおりです。

- `assets/*.png`: 256色の共通パレットを持つ8-bitインデックスPNG
- `palette.bin`: RGB888順の256色パレット（768バイト）
- `intermediate.json`: 後段のバイナリ生成モジュールが読む中間メタデータ
- `report.json`: CIや履歴比較向けの機械可読レポート
- `report.html`: 元画像、変換画像、差分、色誤差、容量をまとめた目視確認用レポート
- `previews/`: HTMLレポート用画像

PNGは見た目だけでなく画素値そのものがPixel Twinsの色インデックスです。スプライトでは
アルファ値が`alpha_threshold`以下の画素をインデックス0へ変換します。BGはアルファを
使用せず、すべての画素を不透明として扱います。

## マニフェスト

```json
{
  "version": 1,
  "name": "gameplay",
  "palette": {
    "asset_range": [32, 254],
    "sample_pixels": 1000000,
    "max_pixels_per_asset": 4096,
    "quantizer": "fast_octree",
    "reserved": [
      {"index": 2, "name": "effect red", "color": "#ff3040"},
      {"index": 18, "name": "font outline", "color": "#203040"}
    ]
  },
  "assets": [
    {"id": "player", "kind": "sprite", "source": "images/player.png", "alpha_threshold": 0, "palette_weight": 4},
    {"id": "map", "kind": "background", "source": "images/map.png"}
  ]
}
```

インデックス0、1、255はツールがそれぞれ透明／背景黒、描画可能な黒、白として自動予約します。
追加予約色は`asset_range`の外側へ置いてください。同じRGBが予約されている場合、完全一致する
不透明画素はその予約インデックスへ割り当てます。黒はインデックス1が優先されます。予約色も
減色時の最近傍候補になるため、画像中の近似色が固定インデックスを利用する場合があります。

`sample_pixels`は共通パレット生成時の最大標本画素数です。巨大な画像セットでのメモリ消費を
抑えるための設定で、色の使用頻度を保った決定的な標本を作ります。品質指標は標本ではなく
全画素から計算します。

`max_pixels_per_asset`は、1枚の巨大画像だけでパレットが決まることを防ぐアセット単位の
標本上限です。小さく重要な画像は`palette_weight`を2以上にするとパレット生成時の比重を
上げられます。変換後の品質集計とデータ容量は常に元画像の全画素を使用します。

`quantizer`は`median_cut`、`max_coverage`、`fast_octree`から選べます。多数の小さなゲーム画像と
BGを混在させる場合は`fast_octree`、単一の写真・イラストに近い画像では`median_cut`を基準に
レポートを比較してください。

## サイズ表示

レポートはアセットごとに元ファイル容量、`幅×高さ`で求めた非圧縮インデックス画素容量、
中間PNG容量を表示します。共有パレットの論理容量は常にRGB888の768バイトです。後段で
ヘッダー、タイルマップ、アラインメントが確定するまでは、それらを実機データ容量へ加算しません。

## スプライトバイナリ

パレット変換後のスプライトをフレーム単位に分割し、透明外周を除去したバイナリを生成できます。

```sh
pixel-twins-sprites build/gameplay/intermediate.json -o build/gameplay/sprites.bin
```

同時に`sprites.json`を生成し、アセット番号、各フレームのtrim位置、切り出し前後の容量、
重複パターン共有による削減量を記録します。バイナリ形式は
[スプライトアセットバイナリ仕様](../../docs/sprite_asset_binary.md)で定義します。

`sprites.hpp`には`SpriteAssetId`を生成します。`SpriteAssetPackView::frameAt()`と
`makeSpriteAt()`は列をアニメーションフレーム、行を方向として直接参照できます。

## BGバイナリ

ランタイム生成マップで使う固定パターンアトラスは、BG設定JSONから生成します。

```sh
pixel-twins-background background.json -o build/gameplay/background.bin
```

地形バリエーション、境界マスク、焼き込みオブジェクトの論理参照表と、重複排除した固定サイズ
パターンを格納します。物理パターン数は最大128で、超過時は変換エラーになります。同時生成する
`background.hpp`には地形、境界、オブジェクトのenumが含まれます。

## 一枚絵バイナリ

圧縮や高速描画を必要としない一枚絵は、Pモード画像の画素値をそのまま行優先で出力できます。

```sh
pixel-twins-raw-image intermediate.json asset_id -o screen.bin
```

出力画素はFlashから直接参照でき、同じセットの`palette.bin`と組み合わせます。
