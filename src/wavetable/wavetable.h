#pragma once

#include <cstdint>
#include <vector>

namespace AudioUtils
{

enum class WaveTableType
{
    Sine,
    Triangle,
    Square,
    Sawtooth,
    SquarePWM,
    ReverseSawtooth,
    PositiveSine,
    PositiveTriangle,
    PositiveSquare,
    PositiveSawtooth,
    PositiveReverseSawtooth,
    Zero
};

struct WaveTable
{
    std::vector<float> data;
    float phase = 0.0f;
    WaveTableType type = WaveTableType::Sine;
};

void wave_table_create(WaveTable& table, WaveTableType type, float phase, uint32_t size);
void wave_table_destroy(WaveTable& table);

} // namespace AudioUtils
