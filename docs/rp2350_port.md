# RP2350移植設計

## 共通コードと境界

`pixel_twins`ターゲットはmacOS版とRP2350版で同じ`src/`をビルドします。
BG、スプライト、プリミティブ、フォント、フレームバッファ、音源、
シーケンサの代替実装は作りません。

RP2350固有実装は`platform/rp2350/`に限定し、次の完成済みデータの入出力だけを
担当します。

- `Framebuffer::displayBuffer()`と`Palette`からLED転送データを生成・転送
- USBホストから2人分の`ControllerSample`を生成
- `AudioSystem::renderBlock()`で生成した48kHz、16-bitステレオPCMを転送
- ボードのタイマと初期化

## ビルド

Pico SDKの標準的なクロスビルドとホステストを分離します。

```sh
PICO_SDK_PATH=/path/to/pico-sdk cmake -S examples/rp2350 -B build-rp2350 \
  -DPICO_BOARD=pico2 -DCMAKE_BUILD_TYPE=Release
cmake --build build-rp2350 --parallel
```

`pixel_twins::rp2350`はPico SDKと共通ライブラリの依存境界です。RP2350ビルドでは
SDLサンプルとホステストを既定で無効にします。

## SRAM配置

`PIXEL_TWINS_SRAM`を付けた描画、フリップ、音声生成関数はRP2350ビルドで
`.time_critical.pixel_twins`セクションに配置され、起動時にSRAMへコピーされます。LED転送実装も
同じ属性を付けます。リンク後はMAPファイルで対象関数の配置を検査します。

## 未確定事項

次の項目は性能とメモリのトレードオフを含むため、実装前に計測値と選択肢を示して
決定します。

- LEDパネルのピン配置、チェーン方式、PIOステートマシン数
- パレット変換済みLED転送バッファの形式と数
- PCM用DMAバッファの数とコア分担
- USBホストのポートと電源構成
