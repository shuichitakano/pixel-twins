# スプライトアセットバイナリ仕様

## 1. 目的

スプライトシートは制作・確認用の入力形式としてのみ使用します。実行時バイナリにはシートを
そのまま格納せず、各描画フレームを個別に切り出し、四辺の完全透明領域を除去した行優先の
8-bitパターンとして連続配置します。

切り出し後の画素列は`Sprite::p`が直接参照でき、実行時の展開や復号を必要としません。内部の
透明画素は描画形状に必要なため保持します。同じ幅、高さ、画素列を持つフレームはバイナリ内で
1つのパターンを共有します。

## 2. 描画位置

各フレーム記述子は、元の論理セルに対する`trimX`、`trimY`を保持します。論理セルの描画先左上を
`logicalX`、`logicalY`とすると、Pixel Twinsの`Sprite`は次のように構築します。

```cpp
Sprite sprite{
    static_cast<std::int16_t>(logicalX + frame.trimX),
    static_cast<std::int16_t>(logicalY + frame.trimY),
    frame.width,
    frame.height,
    pixelData + frame.pixelOffset,
};
```

幅または高さが0の空フレームは描画しません。`pixelOffset`は`0xffffffff`です。

## 3. 全体構造

すべての複数バイト整数はリトルエンディアンです。文字列はバイナリへ格納しません。アセット名と
`assetIndex`の対応は同時生成するJSONレポートで管理します。

```text
+------------------------------+
| Header                 24 B  |
+------------------------------+
| AssetDescriptor × N    12 B  |
+------------------------------+
| FrameDescriptor × M     8 B  |
+------------------------------+
| 4-byte alignment padding     |
+------------------------------+
| deduplicated pixel data      |
+------------------------------+
```

### Header

| 型 | 名前 | 内容 |
| --- | --- | --- |
| `char[4]` | magic | `PTSP` |
| `uint16` | version | `1` |
| `uint16` | headerSize | `24` |
| `uint16` | assetCount | アセット数 |
| `uint16` | assetDescriptorSize | `12` |
| `uint32` | frameCount | 全フレーム数 |
| `uint32` | pixelDataOffset | ファイル先頭から画素領域まで |
| `uint32` | pixelDataSize | 重複排除後の画素バイト数 |

### AssetDescriptor

| 型 | 名前 | 内容 |
| --- | --- | --- |
| `uint32` | firstFrame | フレーム表の先頭インデックス |
| `uint16` | frameCount | このアセットのフレーム数 |
| `uint8` | columns | 元シートの列数 |
| `uint8` | rows | 元シートの行数 |
| `uint8` | logicalWidth | trim前のセル幅 |
| `uint8` | logicalHeight | trim前のセル高 |
| `uint16` | reserved | `0` |

フレーム順はシートの左上から行優先です。

### FrameDescriptor

| 型 | 名前 | 内容 |
| --- | --- | --- |
| `uint32` | pixelOffset | 画素領域先頭からの相対位置 |
| `uint8` | trimX | 論理セル左端からの切り出し位置 |
| `uint8` | trimY | 論理セル上端からの切り出し位置 |
| `uint8` | width | 切り出し後の幅 |
| `uint8` | height | 切り出し後の高さ |

## 4. 非採用の圧縮

画素列のRLEなどは、描画前の展開領域または専用描画ループが必要になり、RAM使用量と描画負荷が
増えます。初期形式では採用せず、透明外周除去と完全一致パターンの共有だけを行います。どちらも
実行時の描画負荷を増やしません。

## 5. アニメーション参照

シートは左上から行優先でフレーム表へ格納します。列を時間方向のアニメーションフレーム、行を
方向や状態として構成したシートは、`SpriteAssetPackView::frameAt(asset, column, row)`または
`makeSpriteAt()`で直接参照できます。ゲーム側でフレーム表の先頭位置を計算する必要はありません。

コンバーターが生成する`sprites.hpp`の`SpriteAssetId`を使用すると、JSONの文字列や手書きの
アセット番号を実行時に保持せずに済みます。

`SpriteAssetPackView::resetSplit()`は、アセット・フレーム記述表と画素領域を別ポインタで受け取り
ます。ゲームプレイ中に頻繁に使う構成では両方をSRAMへ、低頻度のタイトル素材は従来の`reset()`で
Flash上の連続バイナリを直接参照できます。
