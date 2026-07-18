#pragma once

#include "pixel_twins/sound.hpp"

namespace pixel_twins {

struct StandardWaves {
    WaveTable sine;
    WaveTable bell;
    WaveTable square;
    WaveTable pulse;
    WaveTable thinPulse;
    WaveTable saw;
    WaveTable triangle;
    WaveTable spark;
    WaveTable noise;
};

extern const StandardWaves kStandardWaves;

static_assert(sizeof(StandardWaves) == 9 * sizeof(WaveTable));

} // namespace pixel_twins
