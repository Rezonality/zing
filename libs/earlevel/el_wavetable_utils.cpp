//
//  WaveUtils.cpp
//
//  Test wavetable oscillator
//
//  Created by Nigel Redmon on 2/18/13
//  EarLevel Engineering: earlevel.com
//  Copyright 2013 Nigel Redmon
//
//  For a complete explanation of the wavetable oscillator and code,
//  read the series of articles by the author, starting here:
//  www.earlevel.com/main/2012/05/03/a-wavetable-oscillator—introduction/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code for your own purposes, free or commercial.
//
//  1.01  njr  2016-01-03   changed "> minVal" to "< minVal" to fix optimization of number of tables
//  1.1   njr  2019-04-30   changed addWaveTable to AddWaveTable to accomodate WaveTableOsc update
//                          added filleTables2, which allows selection of minimum and maximum frequencies
//

#include <cmath>
#include <memory>

#include "el_wavetable_utils.h"

namespace Earlevel
{
#define WT_PI 3.14159265358979f

void fft(int N, double* ar, double* ai);
float makeWaveTable(WaveTableOsc* osc, int len, double* ar, double* ai, double scale, double topFreq);

//
// fillTables:
//
// The main function of interest here; call this with a pointer to an new, empty oscillator,
// and the real and imaginary arrays and their length. The function fills the oscillator with
// all wavetables necessary for full-bandwidth operation, based on one table per octave,
// and returns the number of tables.
//
int fillTables(WaveTableOsc* osc, double* freqWaveRe, double* freqWaveIm, int numSamples)
{
    int idx;

    // zero DC offset and Nyquist
    freqWaveRe[0] = freqWaveIm[0] = 0.0;
    freqWaveRe[numSamples >> 1] = freqWaveIm[numSamples >> 1] = 0.0;

    // determine maxHarmonic, the highest non-zero harmonic in the wave
    int maxHarmonic = numSamples >> 1;
    const double minVal = 0.000001; // -120 dB
    while ((fabs(freqWaveRe[maxHarmonic]) + fabs(freqWaveIm[maxHarmonic]) < minVal) && maxHarmonic)
        --maxHarmonic;

    // calculate topFreq for the initial wavetable
    // maximum non-aliasing playback rate is 1 / (2 * maxHarmonic), but we allow aliasing up to the
    // point where the aliased harmonic would meet the next octave table, which is an additional 1/3
    double topFreq = 2.0 / 3.0 / maxHarmonic;

    // for subsquent tables, double topFreq and remove upper half of harmonics
    double* ar = new double[numSamples];
    double* ai = new double[numSamples];
    double scale = 0.0;
    int numTables = 0;
    while (maxHarmonic)
    {
        // fill the table in with the needed harmonics
        for (idx = 0; idx < numSamples; idx++)
            ar[idx] = ai[idx] = 0.0;
        for (idx = 1; idx <= maxHarmonic; idx++)
        {
            ar[idx] = freqWaveRe[idx];
            ai[idx] = freqWaveIm[idx];
            ar[numSamples - idx] = freqWaveRe[numSamples - idx];
            ai[numSamples - idx] = freqWaveIm[numSamples - idx];
        }

        // make the wavetable
        scale = makeWaveTable(osc, numSamples, ar, ai, scale, topFreq);
        numTables++;

        // prepare for next table
        topFreq *= 2;
        maxHarmonic >>= 1;
    }
    return numTables;
}

//
// fillTables2:
//
// Alternate version that allows you to ninumum and maximum harmonic coverage.
// minTop: the minimum normalized frequency that all wave tables support
//      ex.: 18000/44100.0 ensures harmonics out to 18k (44.1kHz sample rate) at minimum
// maxTop: the maximum normalized freuqency that all wave tables support
//      ex.: 0.5 give full bandwidth without aliasing; 24000/44100.0 allows a top of 24k, some aliasing
// The function fills the oscillator with all wavetables necessary for full-bandwidth operation,
// based on the criteria, and returns the number of tables.
//
int fillTables2(WaveTableOsc* osc, double* freqWaveRe, double* freqWaveIm, int numSamples, double minTop, double maxTop)
{
    // if top not set, assume aliasing is allowed down to minTop
    if (maxTop == 0.0)
        maxTop = 1.0 - minTop;

    // zero DC offset and Nyquist to be safe
    freqWaveRe[0] = freqWaveIm[0] = 0.0;
    freqWaveRe[numSamples >> 1] = freqWaveIm[numSamples >> 1] = 0.0;

    // for subsequent tables, double topFreq and remove upper half of harmonics
    double* ar = new double[numSamples];
    double* ai = new double[numSamples];
    double scale = 0.0;

    unsigned int maxHarmonic = numSamples >> 1; // start with maximum possible harmonic
    int numTables = 0;
    while (maxHarmonic)
    {
        // find next actual harmonic, and the top frequency it will support
        const double minVal = 0.000001; // -120 dB
        while ((abs(freqWaveRe[maxHarmonic]) + abs(freqWaveIm[maxHarmonic]) < minVal) && maxHarmonic)
            --maxHarmonic;
        double topFreq = maxTop / maxHarmonic;

        // fill the table in with the needed harmonics
        for (int idx = 0; idx < numSamples; idx++)
            ar[idx] = ai[idx] = 0.0;
        for (int idx = 1; idx <= (int)maxHarmonic; idx++)
        {
            ar[idx] = freqWaveRe[idx];
            ai[idx] = freqWaveIm[idx];
            ar[numSamples - idx] = freqWaveRe[numSamples - idx];
            ai[numSamples - idx] = freqWaveIm[numSamples - idx];
        }

        // make the wavetable
        scale = makeWaveTable(osc, numSamples, ar, ai, scale, topFreq);
        numTables++;

        // topFreq is new base frequency, so figure how many harmonics will fit withing maxTop
        int temp = int(minTop / topFreq + 0.5); // next table's maximum harmonic
        maxHarmonic = temp >= (int)maxHarmonic ? maxHarmonic - 1 : temp;
    }
    return numTables;
}

//
// example that builds a sawtooth oscillator via frequency domain
//
std::shared_ptr<WaveTableOsc> sawOsc()
{
    int tableLen = 2048; // to give full bandwidth from 20 Hz
    int idx;

    // TODO: Leak
    double* freqWaveRe = new double[tableLen];
    double* freqWaveIm = new double[tableLen];

    // make a sawtooth
    for (idx = 0; idx < tableLen; idx++)
    {
        freqWaveIm[idx] = 0.0;
    }
    freqWaveRe[0] = freqWaveRe[tableLen >> 1] = 0.0;
    for (idx = 1; idx < (tableLen >> 1); idx++)
    {
        freqWaveRe[idx] = 1.0 / idx; // sawtooth spectrum
        freqWaveRe[tableLen - idx] = -freqWaveRe[idx]; // mirror
    }

    // build a wavetable oscillator
    auto osc = std::make_shared<WaveTableOsc>();
    fillTables(osc.get(), freqWaveRe, freqWaveIm, tableLen);

    return osc;
}

//
// example that creates and oscillator from an arbitrary duration domain wave
//
WaveTableOsc* waveOsc(double* waveSamples, int tableLen)
{
    int idx;
    double* freqWaveRe = new double[tableLen];
    double* freqWaveIm = new double[tableLen];

    // take FFT
    for (idx = 0; idx < tableLen; idx++)
    {
        freqWaveIm[idx] = waveSamples[idx];
        freqWaveRe[idx] = 0.0;
    }
    fft(tableLen, freqWaveRe, freqWaveIm);

    // build a wavetable oscillator
    WaveTableOsc* osc = new WaveTableOsc();
    fillTables(osc, freqWaveRe, freqWaveIm, tableLen);

    return osc;
}

//
// if scale is 0, auto-scales
// returns scaling factor (0.0 if failure), and wavetable in ai array
//
float makeWaveTable(WaveTableOsc* osc, int len, double* ar, double* ai, double scale, double topFreq)
{
    fft(len, ar, ai);

    if (scale == 0.0)
    {
        // calc normal
        double max = 0;
        for (int idx = 0; idx < len; idx++)
        {
            double temp = fabs(ai[idx]);
            if (max < temp)
                max = temp;
        }
        scale = 1.0 / max * .999;
    }

    // normalize
    float* wave = new float[len];
    for (int idx = 0; idx < len; idx++)
        wave[idx] = float(ai[idx] * scale);

    if (osc->AddWaveTable(len, wave, topFreq))
        scale = 0.0;

    return float(scale);
}

//
// fft
//
// I grabbed (and slightly modified) this Rabiner & Gold translation...
//
// (could modify for real data, could use a template version, blah blah--just keeping it short)
//
void fft(int N, double* ar, double* ai)
/*
 in-place complex fft

 After Cooley, Lewis, and Welch; from Rabiner & Gold (1975)

 program adapted from FORTRAN
 by K. Steiglitz  (ken@princeton.edu)
 Computer Science Dept.
 Princeton University 08544          */
{
    int i, j, k, L; /* indexes */
    int M, TEMP, LE, LE1, ip; /* M = log N */
    int NV2, NM1;
    double t; /* temp */
    double Ur, Ui, Wr, Wi, Tr, Ti;
    double Ur_old;

    // if ((N > 1) && !(N & (N - 1)))   // make sure we have a power of 2

    NV2 = N >> 1;
    NM1 = N - 1;
    TEMP = N; /* get M = log N */
    M = 0;
    while (TEMP >>= 1)
        ++M;

    /* shuffle */
    j = 1;
    for (i = 1; i <= NM1; i++)
    {
        if (i < j)
        { /* swap a[i] and a[j] */
            t = ar[j - 1];
            ar[j - 1] = ar[i - 1];
            ar[i - 1] = t;
            t = ai[j - 1];
            ai[j - 1] = ai[i - 1];
            ai[i - 1] = t;
        }

        k = NV2; /* bit-reversed counter */
        while (k < j)
        {
            j -= k;
            k /= 2;
        }

        j += k;
    }

    LE = (int)1.;
    for (L = 1; L <= M; L++)
    { // stage L
        LE1 = LE; // (LE1 = LE/2)
        LE *= 2; // (LE = 2^L)
        Ur = 1.0;
        Ui = 0.;
        Wr = cos(WT_PI / (float)LE1);
        Wi = -sin(WT_PI / (float)LE1); // Cooley, Lewis, and Welch have "+" here
        for (j = 1; j <= LE1; j++)
        {
            for (i = j; i <= N; i += LE)
            { // butterfly
                ip = i + LE1;
                Tr = ar[ip - 1] * Ur - ai[ip - 1] * Ui;
                Ti = ar[ip - 1] * Ui + ai[ip - 1] * Ur;
                ar[ip - 1] = ar[i - 1] - Tr;
                ai[ip - 1] = ai[i - 1] - Ti;
                ar[i - 1] = ar[i - 1] + Tr;
                ai[i - 1] = ai[i - 1] + Ti;
            }
            Ur_old = Ur;
            Ur = Ur_old * Wr - Ui * Wi;
            Ui = Ur_old * Wi + Ui * Wr;
        }
    }
}

} // Earlevel
