#pragma once

#include <cstdint>

namespace pixel_twins::rp2350::board {

using GpioPin = std::uint8_t;

// UART
inline constexpr GpioPin kUartTxPin = 0;
inline constexpr GpioPin kUartRxPin = 1;

// LEDデータ。PIOのOUTでGPIO2から12bitを一度に出力する。
// 回路図上の1/2がpanel 1の上/下、3/4がpanel 2の上/下に対応する。
inline constexpr GpioPin kLedDataPinBase = 2;
inline constexpr std::uint8_t kLedDataPinCount = 12;

inline constexpr GpioPin kLedR1Pin = 2;
inline constexpr GpioPin kLedR2Pin = 3;
inline constexpr GpioPin kLedG1Pin = 4;
inline constexpr GpioPin kLedG2Pin = 5;
inline constexpr GpioPin kLedB1Pin = 6;
inline constexpr GpioPin kLedB2Pin = 7;
inline constexpr GpioPin kLedR3Pin = 8;
inline constexpr GpioPin kLedR4Pin = 9;
inline constexpr GpioPin kLedG3Pin = 10;
inline constexpr GpioPin kLedG4Pin = 11;
inline constexpr GpioPin kLedB3Pin = 12;
inline constexpr GpioPin kLedB4Pin = 13;

// LED制御
inline constexpr GpioPin kLedClockPin = 14;
inline constexpr GpioPin kLedLatchPin = 15;
inline constexpr GpioPin kLedRowAPin = 16;
inline constexpr GpioPin kLedRowBPin = 17;
inline constexpr GpioPin kLedRowCPin = 18;
inline constexpr GpioPin kLedOutputEnablePin = 19;

// 2つ目のUSBポート
inline constexpr GpioPin kUsb2DataPlusPin = 20;
inline constexpr GpioPin kUsb2DataMinusPin = 21;

// 設定スイッチ。どちらもONでGNDに接続されるactive-low入力。
inline constexpr GpioPin kConfig1Pin = 26;
inline constexpr GpioPin kConfig2Pin = 22;

// ステレオ音声PWM
inline constexpr GpioPin kAudioPwmLeftPin = 28;
inline constexpr GpioPin kAudioPwmRightPin = 27;

static_assert(kLedB4Pin == kLedDataPinBase + kLedDataPinCount - 1,
              "LEDデータGPIOは12本連続していなければなりません");

} // namespace pixel_twins::rp2350::board
