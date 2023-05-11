#include <cmath>
#include "wavetable.h"

#include "glm/gtc/constants.hpp"

namespace AudioUtils
{

void wave_table_destroy(WaveTable& table)
{
    table.data.clear();
}

void wave_table_create(WaveTable& table, WaveTableType type, float phase, uint32_t size)
{
    int32_t count = (uint32_t)size;
    table.phase = phase;
    table.data.resize(count);
    table.type = type;

    int32_t phaseOffset = int32_t(phase * count);

    switch (type)
    {
    case WaveTableType::Sine:
    {
        for (int32_t i = 0; i < count; i++)
        {
            table.data[i] = float(std::sin(2 * glm::pi<double>() * double(i + phaseOffset) / float(count)));
        }
    }
    break;
    case WaveTableType::Triangle:
    {
        auto slope = 4.0 / double(count);
        for (int32_t i = 0; i < count; i++)
        {
            if ((i + phaseOffset) % count < count / 2)
            {
                table.data[i] = float(slope * double((i + phaseOffset) % count) - 1.0);
            }
            else
            {
                table.data[i] = float(slope * float((-i - phaseOffset) % count) + 3.0);
            }
        }
    }
    break;
    case WaveTableType::Square:
    {
        for (int32_t i = 0; i < count; i++)
        {
            if ((i + phaseOffset) % count < count / 2)
            {
                table.data[i] = -1.0;
            }
            else
            {
                table.data[i] = 1.0;
            }
        }
    }
    break;
    case WaveTableType::SquarePWM:
    {
        for (int32_t i = 0; i < count; i++)
        {
            if ((i + phaseOffset) % count < count / 4)
            {
                table.data[i] = -1.0;
            }
            else
            {
                table.data[i] = 1.0;
            }
        }
    }
    break;

    case WaveTableType::Sawtooth:
    {
        for (int32_t i = 0; i < count; i++)
        {
            table.data[i] = float(-1.0 + 2.0 * float((i + phaseOffset) % count) / float(count));
        }
    }
    break;

    case WaveTableType::ReverseSawtooth:
    {
        for (int32_t i = 0; i < count; i++)
        {
            table.data[i] = float(1.0 - 2.0 * float((i + phaseOffset) % count) / float(count));
        }
    }
    break;

    case WaveTableType::PositiveTriangle:
    {
        float slope = float(2.0) / float(count);
        for (int32_t i = 0; i < count; i++)
        {
            if ((i + phaseOffset) % count < count / 2)
            {
                table.data[i] = slope * float((i + phaseOffset) % count);
            }
            else
            {
                table.data[i] = slope * float((-i - phaseOffset) % count) + 2.0f;
            }
        }
    }
    break;

    case WaveTableType::PositiveSquare:
    {
        for (int32_t i = 0; i < count; i++)
        {
            if ((i + phaseOffset) % count < count / 2)
            {
                table.data[i] = 0.0;
            }
            else
            {
                table.data[i] = 1.0;
            }
        }
    }
    break;

    /// Instantiate the table as a sawtooth wave
    case WaveTableType::PositiveSawtooth:
    {
        for (int32_t i = 0; i < count; i++)
        {
            table.data[i] = float((i + phaseOffset) % count) / float(count);
        }
    }
    break;

    case WaveTableType::PositiveReverseSawtooth:
    {
        for (int32_t i = 0; i < count; i++)
        {
            table.data[i] = float(1.0) - float((i + phaseOffset) % count) / float(count);
        }
    }
    break;

    case WaveTableType::PositiveSine:
    {
        for (int32_t i = 0; i < count; i++)
        {
            table.data[i] = float(0.5 + 0.5 * sin(2 * glm::pi<double>() * float(i + phaseOffset) / float(count)));
        }
    }
    break;

    default:
    case WaveTableType::Zero:
    {
        std::fill_n(table.data.begin(), count, 0.0f);
    }
    break;
    }
}

} // namespace MAudio
