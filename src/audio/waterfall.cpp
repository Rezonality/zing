// Waterfall.cpp
#include <zing/audio/waterfall.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <implot.h>

namespace {

inline float ToDb_FromPower(float p) {
    // power can be tiny
    constexpr float eps = 1e-20f;
    return 10.0f * std::log10(std::max(p, eps));
}

inline float Clamp(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

// Estimate noise floor from a dB line: mean of bottom 20% bins.
// IMPORTANT: must not scramble the stored line.
float EstimateNoiseDb_BottomMean(const float* lineDb, int bins) {
    static thread_local std::vector<float> work;
    work.assign(lineDb, lineDb + bins);

    const int k = std::max(1, bins / 5);
    std::nth_element(work.begin(), work.begin() + k, work.end());

    float sum = 0.0f;
    for (int i = 0; i < k; ++i) sum += work[i];
    return sum / float(k);
}

float ReduceNoiseWindowSpan(const float* data, int count, bool useMedian) {
    if (!data || count <= 0) return -120.0f;
    if (!useMedian) {
        float sum = 0.0f;
        for (int i = 0; i < count; ++i) sum += data[i];
        return sum / float(count);
    }

    static thread_local std::vector<float> work;
    work.assign(data, data + count);
    const size_t mid = work.size() / 2;
    std::nth_element(work.begin(), work.begin() + mid, work.end());
    if (work.size() % 2 == 1)
        return work[mid];

    const float hi = work[mid];
    std::nth_element(work.begin(), work.begin() + (mid - 1), work.end());
    const float lo = work[mid - 1];
    return 0.5f * (lo + hi);
}

// Push a fully-formed dB line into the ring buffer, and update emaNoiseDb if not locked/manual.
void PushLineDb(Waterfall& wf, const float* lineDb) {
    // Update auto noise estimate unless user says "nope"
    if (!wf.manualFloor && !wf.lockNoiseFloor) {
        const float noiseNow = EstimateNoiseDb_BottomMean(lineDb, wf.bins);
        if (wf.noiseWindowN < 1) wf.noiseWindowN = 1;
        if ((int)wf.noiseWinDb.size() != wf.noiseWindowN) {
            wf.noiseWinDb.assign(size_t(wf.noiseWindowN), noiseNow);
            wf.noiseWinHead = 0;
            wf.noiseWinCount = 0;
        }
        if (wf.noiseWinCount < wf.noiseWindowN) {
            wf.noiseWinDb[size_t(wf.noiseWinCount++)] = noiseNow;
        } else {
            wf.noiseWinDb[size_t(wf.noiseWinHead)] = noiseNow;
            wf.noiseWinHead = (wf.noiseWinHead + 1) % wf.noiseWindowN;
        }

        const int noiseCount = std::max(1, wf.noiseWinCount);
        const float noiseAvg = ReduceNoiseWindowSpan(wf.noiseWinDb.data(), noiseCount, wf.useMedianNoise);
        if (wf.noiseWinCount < wf.noiseWindowN) {
            // Bootstrap to a reasonable starting floor while window fills.
            wf.emaNoiseDb = noiseAvg;
        } else {
            const float a = Clamp(wf.adapt, 0.0f, 1.0f);
            wf.emaNoiseDb = (1.0f - a) * wf.emaNoiseDb + a * noiseAvg;
        }
    }

    // Store ordered line
    float* dst = wf.ringDb.data() + size_t(wf.head) * size_t(wf.bins);
    std::memcpy(dst, lineDb, size_t(wf.bins) * sizeof(float));

    wf.head = (wf.head + 1) % wf.rows;
    wf.rowsWritten = std::min(wf.rowsWritten + 1, wf.rows);
}

} // namespace

void Waterfall_Init(Waterfall& wf, int bins, int rows) {
    wf.bins = std::max(0, bins);
    wf.rows = std::max(1, rows);
    wf.head = 0;
    wf.rowsWritten = 0;

    wf.emaNoiseDb = -90.0f;
    wf.lockedNoiseDb = wf.emaNoiseDb;

    wf.ringDb.assign(size_t(wf.rows) * size_t(wf.bins), -120.0f);
    wf.uploadDb.assign(wf.ringDb.size(), -120.0f);

    wf.accumulateN = std::max(1, wf.accumulateN);
    wf.accCount = 0;
    wf.accPowerSum.assign(size_t(wf.bins), 0.0f);

    wf.noiseWindowN = std::max(1, wf.noiseWindowN);
    wf.noiseWinHead = 0;
    wf.noiseWinCount = 0;
    wf.noiseWinDb.assign(size_t(wf.noiseWindowN), wf.emaNoiseDb);
}

void Waterfall_Reset(Waterfall& wf) {
    wf.head = 0;
    wf.rowsWritten = 0;
    wf.emaNoiseDb = -90.0f;
    wf.lockedNoiseDb = wf.emaNoiseDb;

    std::fill(wf.ringDb.begin(), wf.ringDb.end(), -120.0f);
    std::fill(wf.uploadDb.begin(), wf.uploadDb.end(), -120.0f);

    wf.accCount = 0;
    std::fill(wf.accPowerSum.begin(), wf.accPowerSum.end(), 0.0f);

    wf.noiseWindowN = std::max(1, wf.noiseWindowN);
    wf.noiseWinHead = 0;
    wf.noiseWinCount = 0;
    wf.noiseWinDb.assign(size_t(wf.noiseWindowN), wf.emaNoiseDb);
}

void Waterfall_AccumulateMag(Waterfall& wf, const float* spectrumMag, int spectrumCount) {
    if (!wf.enabled) return;
    if (wf.bins <= 0 || wf.rows <= 0) return;
    if (!spectrumMag) return;
    if (spectrumCount < wf.bins) return;

    wf.accumulateN = std::max(1, wf.accumulateN);
    if ((int)wf.accPowerSum.size() != wf.bins)
        wf.accPowerSum.assign(size_t(wf.bins), 0.0f);

    // accumulate POWER = mag^2
    for (int i = 0; i < wf.bins; ++i) {
        const float m = spectrumMag[i];
        wf.accPowerSum[i] += m * m;
    }

    wf.accCount++;
    if (wf.accCount < wf.accumulateN)
        return;

    // Average power -> dB(power)
    static thread_local std::vector<float> lineDb;
    lineDb.resize(size_t(wf.bins));

    const float invN = 1.0f / float(wf.accCount);
    for (int i = 0; i < wf.bins; ++i) {
        const float pAvg = wf.accPowerSum[i] * invN;
        lineDb[i] = ToDb_FromPower(pAvg);
    }

    // reset accumulator
    std::fill(wf.accPowerSum.begin(), wf.accPowerSum.end(), 0.0f);
    wf.accCount = 0;

    // commit
    PushLineDb(wf, lineDb.data());
}

void Waterfall_BuildUpload(Waterfall& wf) {
    if (wf.bins <= 0 || wf.rows <= 0) return;
    if (wf.uploadDb.size() != wf.ringDb.size())
        wf.uploadDb.resize(wf.ringDb.size());

    // Newest row starts at top and moves downward as rows arrive.
    if (wf.rowsWritten <= 0) {
        const float fill = Waterfall_FloorDb(wf);
        std::fill(wf.uploadDb.begin(), wf.uploadDb.end(), fill);
        return;
    }

    const int newestRow = (wf.head - 1 + wf.rows) % wf.rows;
    for (int y = 0; y < wf.rows; ++y) {
        const float* src = nullptr;
        if (y < wf.rowsWritten) {
            const int ringRow = (newestRow - y + wf.rows) % wf.rows;
            src = wf.ringDb.data() + size_t(ringRow) * size_t(wf.bins);
        }

        float* dst = wf.uploadDb.data() + size_t(y) * size_t(wf.bins);
        if (src) {
            std::memcpy(dst, src, size_t(wf.bins) * sizeof(float));
        } else {
            const float fill = Waterfall_FloorDb(wf);
            std::fill(dst, dst + wf.bins, fill);
        }
    }
}

float Waterfall_FloorDb(const Waterfall& wf) {
    if (wf.manualFloor)
        return wf.manualFloorDb;

    if (wf.lockNoiseFloor)
        return wf.lockedNoiseDb + wf.floorOffsetDb;

    return wf.emaNoiseDb + wf.floorOffsetDb;
}

float Waterfall_CeilDb(const Waterfall& wf) {
    return Waterfall_FloorDb(wf) + wf.rangeDb;
}

void Waterfall_DrawControls(Waterfall& wf) {
    ImGui::SliderFloat("WF Range (dB)", &wf.rangeDb, 10.0f, 90.0f, "%.1f");
    ImGui::SliderFloat("WF Adapt", &wf.adapt, 0.001f, 0.30f, "%.3f");
    ImGui::SliderInt("WF Speed (spectra/row)", &wf.accumulateN, 1, 64);

    ImGui::Separator();

    ImGui::SliderFloat("WF Offset (dB)", &wf.floorOffsetDb, -40.0f, 40.0f, "%.1f");
    ImGui::SliderInt("WF Noise Window (rows)", &wf.noiseWindowN, 1, 64);
    ImGui::Checkbox("WF Noise Median", &wf.useMedianNoise);

    if (ImGui::Checkbox("WF Lock Noise", &wf.lockNoiseFloor)) {
        if (wf.lockNoiseFloor) {
            // lock current auto estimate as baseline
            wf.lockedNoiseDb = wf.emaNoiseDb;
        }
    }

    ImGui::Checkbox("WF Manual Floor", &wf.manualFloor);
    if (wf.manualFloor) {
        ImGui::SliderFloat("WF Floor (dB)", &wf.manualFloorDb, -160.0f, -10.0f, "%.1f");
    }

    if (ImGui::Button("WF Reset")) {
        Waterfall_Reset(wf);
    }

    // Optional debug readout (handy while tuning)
    ImGui::Text("AutoNoise: %.1f dB  Floor: %.1f dB  Ceil: %.1f dB",
                wf.emaNoiseDb, Waterfall_FloorDb(wf), Waterfall_CeilDb(wf));
}

void Waterfall_DrawPlot(Waterfall& wf, const char* plotTitle, float maxHz, ImVec2 plotSize) {
    if (!wf.enabled) return;
    if (wf.bins <= 0 || wf.rows <= 0) return;

    Waterfall_BuildUpload(wf);

    const float plotWidth = plotSize.x > 0.0f ? plotSize.x : ImGui::GetContentRegionAvail().x;
    const float plotHeight = plotSize.y > 0.0f ? plotSize.y : ImGui::GetContentRegionAvail().y;

    const double x0 = 0.0;
    const double x1 = (double)maxHz;
    const double y0 = (double)wf.rows;
    const double y1 = 0.0;

    ImVec2 plotPos = ImVec2(0.0f, 0.0f);
    ImVec2 plotSizeActual = ImVec2(plotWidth, plotHeight);
    ImVec2 plotItemMin = ImVec2(0.0f, 0.0f);
    ImVec2 plotItemMax = ImVec2(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    if (ImPlot::BeginPlot(plotTitle, ImVec2(plotWidth, plotHeight),
        ImPlotFlags_NoLegend | ImPlotFlags_NoFrame | ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoInputs | ImPlotFlags_NoTitle)) {
        ImPlot::SetupAxes("", "", ImPlotAxisFlags_NoLabel, ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels);
        ImPlot::SetupAxisLimits(ImAxis_X1, x0, x1, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, y0, y1, ImPlotCond_Always);

        plotPos = ImPlot::GetPlotPos();
        plotSizeActual = ImPlot::GetPlotSize();

        const ImPlotRect limits = ImPlot::GetPlotLimits();
        const double markerValue = x0 + (x1 - x0) * std::clamp<double>(wf.markerX, 0.0, 1.0);
        const double markerWidthHz = std::max(1.0, double(wf.markerWidthHz));
        const double markerHalfHz = markerWidthHz * 0.5;

        // Classic radio-ish (widely available in older ImPlot)
        ImPlot::PushColormap(ImPlotColormap_Jet);

        ImPlot::PushPlotClipRect();
        ImPlot::PlotHeatmap(
            "##wf",
            wf.uploadDb.data(),
            wf.rows, wf.bins,
            Waterfall_FloorDb(wf),
            Waterfall_CeilDb(wf),
            nullptr,
            ImPlotPoint(x0, limits.Y.Min),
            ImPlotPoint(x1, limits.Y.Max)
        );

        const ImPlotPoint rectMin(markerValue - markerHalfHz, limits.Y.Min);
        const ImPlotPoint rectMax(markerValue + markerHalfHz, limits.Y.Max);
        ImPlot::GetPlotDrawList()->AddRectFilled(
            ImPlot::PlotToPixels(rectMin),
            ImPlot::PlotToPixels(rectMax),
            IM_COL32(255, 255, 255, 40));
        const ImVec2 lineMin = ImPlot::PlotToPixels(ImPlotPoint(markerValue - markerHalfHz, limits.Y.Min));
        const ImVec2 lineMax = ImPlot::PlotToPixels(ImPlotPoint(markerValue - markerHalfHz, limits.Y.Max));
        const ImVec2 lineMin2 = ImPlot::PlotToPixels(ImPlotPoint(markerValue + markerHalfHz, limits.Y.Min));
        const ImVec2 lineMax2 = ImPlot::PlotToPixels(ImPlotPoint(markerValue + markerHalfHz, limits.Y.Max));
        ImPlot::GetPlotDrawList()->AddLine(lineMin, lineMax, IM_COL32(255, 255, 0, 255), 1.0f);
        ImPlot::GetPlotDrawList()->AddLine(lineMin2, lineMax2, IM_COL32(255, 255, 0, 255), 1.0f);
        ImPlot::PopPlotClipRect();

        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            const ImPlotPoint mouse = ImPlot::GetPlotMousePos();
            const double t = (mouse.x - x0) / (x1 - x0);
            wf.markerX = std::clamp(static_cast<float>(t), 0.0f, 1.0f);
        }

        ImPlot::PopColormap();
        ImPlot::EndPlot();
        plotItemMin = ImGui::GetItemRectMin();
        plotItemMax = ImGui::GetItemRectMax();
    }

    // Capture drag over the plot area so the window doesn't move
    if (plotSizeActual.x > 0.0f && plotSizeActual.y > 0.0f) {
        ImGui::SetCursorScreenPos(plotPos);
        ImGui::InvisibleButton("##wf_plot_drag", plotSizeActual);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            const float t = (ImGui::GetIO().MousePos.x - plotPos.x) / plotSizeActual.x;
            wf.markerX = std::clamp(t, 0.0f, 1.0f);
        }
    }

    ImGui::PopStyleVar(2);

    // Custom marker strip under the plot
    const float stripHeight = 24.0f;
    const float stripY = plotItemMax.y + ImGui::GetStyle().ItemSpacing.y;
    ImGui::SetCursorScreenPos(ImVec2(plotPos.x, stripY));
    ImGui::InvisibleButton("##wf_marker_strip", ImVec2(plotSizeActual.x, stripHeight));
    const ImVec2 stripMin = ImGui::GetItemRectMin();
    const ImVec2 stripMax = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(stripMin, stripMax, IM_COL32(18, 18, 18, 255));
    drawList->AddRect(stripMin, stripMax, IM_COL32(60, 60, 60, 255));

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float t = (ImGui::GetIO().MousePos.x - stripMin.x) / (stripMax.x - stripMin.x);
        wf.markerX = std::clamp(t, 0.0f, 1.0f);
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const float t = (ImGui::GetIO().MousePos.x - stripMin.x) / (stripMax.x - stripMin.x);
        wf.markerX = std::clamp(t, 0.0f, 1.0f);
    }

    const float markerX = stripMin.x + wf.markerX * (stripMax.x - stripMin.x);
    const float tipY = stripMin.y + 4.0f;
    const float baseY = stripMax.y - 4.0f;
    const float halfWidth = 6.0f;
    drawList->AddTriangleFilled(
        ImVec2(markerX, tipY),
        ImVec2(markerX - halfWidth, baseY),
        ImVec2(markerX + halfWidth, baseY),
        IM_COL32(220, 220, 220, 255));
}

Waterfall& Waterfall_Get()
{
    static Waterfall wf;
    return wf;
}
