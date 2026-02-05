// Waterfall.h
#pragma once

#include <vector>

// Forward-declare to keep this header light.
// Include <imgui.h> and <implot.h> in Waterfall.cpp and in any TU that calls Draw().
struct ImVec2;

struct Waterfall
{
    // ---- Dimensions ----
    int bins = 0;  // number of frequency bins displayed
    int rows = 50; // history lines displayed

    // ---- Waterfall speed (time decimation) ----
    int accumulateN = 8; // spectra per committed row
    int accCount = 0;
    std::vector<float> accPowerSum; // sum of POWER per bin (mag^2) across accCount

    // ---- Auto noise tracking ----
    float emaNoiseDb = -90.0f; // internal auto estimate (dB)
    float adapt = 0.08f;       // EMA alpha (0..1); higher = faster tracking

    // ---- User controls ----
    float rangeDb = 27.0f;      // dynamic range shown above floor
    float floorOffsetDb = -2.0f; // manual bias applied to auto (and locked) floor

    bool lockNoiseFloor = false;
    float lockedNoiseDb = -190.0f;

    bool manualFloor = false;
    float manualFloorDb = -190.0f;

    bool enabled = true;

    // ---- Buffers (stored as dB values) ----
    int head = 0;                // next write row (ring)
    int rowsWritten = 0;         // how many rows have been committed
    std::vector<float> ringDb;   // rows*bins (ring layout)
    std::vector<float> uploadDb; // rows*bins (chronological oldest->newest for PlotHeatmap)

    int noiseWindowN = 10; // how many committed rows to use
    int noiseWinHead = 0;
    int noiseWinCount = 0;
    std::vector<float> noiseWinDb; // size noiseWindowN
    bool useMedianNoise = true;    // median vs mean

    // ---- Marker (UI) ----
    float markerX = 0.5f; // normalized [0..1] position
    float markerWidthHz = 500.0f;
};

// Lifecycle
void Waterfall_Init(Waterfall& wf, int bins, int rows = 50);
void Waterfall_Reset(Waterfall& wf);

// Feed one spectrum snapshot (magnitudes). This ACCUMULATES and commits every wf.accumulateN calls.
void Waterfall_AccumulateMag(Waterfall& wf, const float* spectrumMag, int spectrumCount);

// Build ordered buffer for drawing (oldest at top, newest at bottom)
void Waterfall_BuildUpload(Waterfall& wf);

// Scale helpers (effective display scale)
float Waterfall_FloorDb(const Waterfall& wf);
float Waterfall_CeilDb(const Waterfall& wf);

// UI + draw (ImGui + ImPlot)
void Waterfall_DrawControls(Waterfall& wf);
void Waterfall_DrawPlot(Waterfall& wf, const char* plotTitle, float maxHz, ImVec2 plotSize);

// Shared marker access
Waterfall& Waterfall_Get();
