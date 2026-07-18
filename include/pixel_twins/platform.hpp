#pragma once

#if defined(PICO_RP2350)
#define PIXEL_TWINS_SRAM __attribute__((section(".time_critical.pixel_twins")))
#else
#define PIXEL_TWINS_SRAM
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PIXEL_TWINS_FORCE_INLINE inline __attribute__((always_inline))
#else
#define PIXEL_TWINS_FORCE_INLINE inline
#endif
