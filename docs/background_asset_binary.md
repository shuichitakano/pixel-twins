# BGアセットバイナリ仕様

## 1. 目的

ランタイムで生成する8-bitタイルマップから、固定BGパターンを直接参照するための形式です。
地形バリエーション、境界マスク、焼き込みオブジェクトは論理参照表を介して0〜127の物理
パターン番号へ変換します。完全一致するパターンは同じ番号を共有します。

パターンは展開済みの行優先8-bit画素列であり、描画時の復号や動的メモリ確保は行いません。
`BackgroundAssetPackView::makeBackground()`が返す`Background`は、パターン領域を直接参照します。
`resetSplit()`を使用すると、ヘッダーと論理参照表をFlash、パターン画素をSRAMへ分離できます。

## 2. 全体構造

複数バイト整数はリトルエンディアンです。

```text
+--------------------------------+
| Header                  32 B   |
+--------------------------------+
| terrain lookup                 |
| boundary lookup                |
| baked object lookup            |
+--------------------------------+
| 4-byte alignment padding       |
+--------------------------------+
| pattern pixels                 |
+--------------------------------+
```

### Header

| 型 | 名前 | 内容 |
| --- | --- | --- |
| `char[4]` | magic | `PTBG` |
| `uint16` | version | `1` |
| `uint16` | headerSize | `32` |
| `uint8` | tileWidth | パターン幅 |
| `uint8` | tileHeight | パターン高 |
| `uint8` | patternCount | 物理パターン数、最大128 |
| `uint8` | terrainCount | 地形数 |
| `uint8` | variantsPerTerrain | 地形ごとのバリエーション数 |
| `uint8` | boundaryCount | 境界種別数 |
| `uint8` | masksPerBoundary | 境界ごとのマスク数 |
| `uint8` | objectCount | 焼き込みオブジェクト数 |
| `uint32` | mappingOffset | 論理参照表の位置 |
| `uint32` | patternDataOffset | パターン画素領域の位置 |
| `uint32` | patternDataSize | パターン画素容量 |
| `uint32` | reserved | `0` |

## 3. 論理参照表

すべて`uint8`の物理パターン番号です。次の順で隙間なく格納します。

1. `terrainCount × variantsPerTerrain`
2. `boundaryCount × masksPerBoundary`
3. `objectCount`

ランタイムマップ生成器は`terrainPattern()`、`boundaryPattern()`、`objectPattern()`で番号を取得し、
衝突属性を付ける場合は上位1bitをORします。描画時は`tileIndexMask = 0x7f`を指定します。

## 4. パターン画素

物理パターン番号順の行優先8-bit画素列です。容量は必ず次式と一致します。

```text
patternCount × tileWidth × tileHeight
```

BGには透明色がないため、インデックス0も通常の黒として描画します。
