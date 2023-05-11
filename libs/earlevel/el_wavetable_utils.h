//
//  WaveUtils.h
//
//  Created by Nigel Redmon on 2/18/13
//
//

#pragma once

#include "el_wavetable.h"
#include <memory>

namespace Earlevel
{
int fillTables(WaveTableOsc* osc, double* freqWaveRe, double* freqWaveIm, int numSamples);
int fillTables2(WaveTableOsc* osc, double* freqWaveRe, double* freqWaveIm, int numSamples, double minTop = 0.4, double maxTop = 0);
float makeWaveTable(WaveTableOsc* osc, int len, double* ar, double* ai, double scale, double topFreq);

void fft(int N, double* ar, double* ai);

// examples
std::shared_ptr<WaveTableOsc> sawOsc(void);
WaveTableOsc* waveOsc(double* waveSamples, int tableLen);
}

