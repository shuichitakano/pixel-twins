# フォントコンバーター

`tools/font_converter.py`は、8×9ピクセルのグリフを並べたPNG画像から、Pixel Twinsの
`BitmapFont`として直接リンクできる`.hpp`と`.cpp`を生成します。生成物は実行時の変換や
動的メモリ確保を必要とせず、`const`データとしてmacOS版とマイコン版の両方へリンクできます。

## 入力画像

グリフは左上から文字コード順に並べます。既定値は1行16グリフ、ASCII 32〜126の95文字です。
各セルは8×9ピクセルで、次の3種類の画素だけを使用できます。

- 透明画素: 描画しない
- `#111315`: 縁プレーン
- `#f1ead8`: 本文プレーン

不透明な未定義色があれば、コンバーターは位置を表示して失敗します。減色や近似は行いません。
色は`--outline-color`と`--body-color`で変更できます。

## 実行例

PillowをインストールしたPython環境で実行します。

```sh
python3 tools/font_converter.py \
  path/to/outlined_8x9_ascii.png \
  --header path/to/wizward_font.hpp \
  --source path/to/wizward_font.cpp \
  --namespace wizward::assets \
  --symbol kWizwardFont
```

頻繁に描画するゲームプレイ用フォントは`--sram`を指定すると、生成グリフへ
`PIXEL_TWINS_ASSET_SRAM`を付けてRP2350のSRAMへ配置できます。タイトルなど低頻度用途では省略し、
Flashから直接参照できます。

出力`.cpp`は、95文字の場合に`Glyph`配列1,710バイトと小さな`BitmapFont`定義を持ちます。
`.hpp`は`extern`宣言だけを持つため、複数の翻訳単位からインクルードしても配列は複製されません。
縁のパレット番号は既定で1です。別の固定色を使う場合は`--outline-index`で指定します。

主なレイアウト変更オプションは`--columns`、`--first`、`--count`、`--fallback`です。
