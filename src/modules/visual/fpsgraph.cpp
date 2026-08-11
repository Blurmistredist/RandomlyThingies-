#include "fpsgraph.hpp"
#include "modules/ModuleRegistry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <unistd.h>

namespace {
static FPSGraphModule* g_fpsGraphMod = nullptr;

float calcTextWidth(const std::string& text, float size) {
    float width = 0.0f;
    for (char c : text) {
        if (c == 'i' || c == 'l' || c == '1' || c == ':' || c == '.' || c == ' ')
            width += size * 0.30f;
        else if (c == 'm' || c == 'w' || c == 'M' || c == 'W')
            width += size * 0.80f;
        else
            width += size * 0.58f;
    }
    return width;
}

uint32_t applyAlpha(uint32_t color, float alpha) {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    return (static_cast<uint32_t>(alpha * 255.0f) << 24) |
           (color & 0x00FFFFFF);
}
}

FPSGraphModule::FPSGraphModule()
    : Module("FPS Graph", "FPS, frame-time and memory performance graph.") {
    g_fpsGraphMod = this;
}

FPSGraphModule::~FPSGraphModule() {
    if (g_fpsGraphMod == this)
        g_fpsGraphMod = nullptr;
}

void FPSGraphModule::onEnable() {
    m_lastFrame = Clock::now();
    m_fakeStart = Clock::now();
    m_fakeNextJump = m_fakeStart;
    m_fakeHighRange = false;
    m_fakeDisplayedFps = 2500.0f;
    m_hasLastFrame = false;

    m_fpsHistory.clear();
    m_jitterHistory.clear();
    m_ramHistory.clear();
    m_low1History.clear();

    m_currentFps = 0.0f;
    m_averageFps = 0.0f;
    m_jitterMs = 0.0f;
    m_ramGb = 0.0f;
    m_onePercentLow = 0.0f;
}

void FPSGraphModule::onDisable() {
    m_hasLastFrame = false;
    m_fpsHistory.clear();
    m_jitterHistory.clear();
    m_ramHistory.clear();
    m_low1History.clear();
}

float FPSGraphModule::getDisplayedFps(float realFps) {
    if (!m_superPerformanceModeThing)
        return realFps;

    const auto now = Clock::now();
    if (now >= m_fakeNextJump) {
        // Secret mode: normally fake 2,000-3,000 FPS, but occasionally
        // jump into a much more ridiculous 11,000-17,000 FPS range.
        std::bernoulli_distribution chooseHigh(0.35);
        m_fakeHighRange = chooseHigh(m_fakeRng);

        if (m_fakeHighRange) {
            m_fakeDisplayedFps =
                std::uniform_real_distribution<float>(11000.0f, 17000.0f)(m_fakeRng);
        } else {
            m_fakeDisplayedFps =
                std::uniform_real_distribution<float>(2000.0f, 3000.0f)(m_fakeRng);
        }

        const std::chrono::duration<float> delay(
            std::uniform_real_distribution<float>(1.5f, 4.0f)(m_fakeRng));
        m_fakeNextJump = now + std::chrono::duration_cast<Clock::duration>(delay);
    }

    return m_fakeDisplayedFps;
}

void FPSGraphModule::pushHistory(std::vector<float>& history, float value) {
    history.push_back(value);
    const std::size_t maxSize = static_cast<std::size_t>(std::max(16, m_historySize));
    if (history.size() > maxSize) {
        history.erase(history.begin(), history.begin() + (history.size() - maxSize));
    }
}

float FPSGraphModule::readResidentMemoryGb() const {
    std::ifstream file("/proc/self/statm");
    long pages = 0;
    long resident = 0;
    if (!(file >> pages >> resident) || resident <= 0)
        return 0.0f;

    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0)
        return 0.0f;

    const double bytes = static_cast<double>(resident) * static_cast<double>(pageSize);
    return static_cast<float>(bytes / (1024.0 * 1024.0 * 1024.0));
}

void FPSGraphModule::rebuildStatistics() {
    if (m_fpsHistory.empty()) {
        m_averageFps = 0.0f;
        m_onePercentLow = 0.0f;
        return;
    }

    const float sum = std::accumulate(m_fpsHistory.begin(), m_fpsHistory.end(), 0.0f);
    m_averageFps = sum / static_cast<float>(m_fpsHistory.size());

    // Standard 1% low: average of the slowest 1% of samples.
    std::vector<float> sorted = m_fpsHistory;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t count = std::max<std::size_t>(1, sorted.size() / 100);
    const float lowSum = std::accumulate(sorted.begin(), sorted.begin() + count, 0.0f);
    m_onePercentLow = lowSum / static_cast<float>(count);
}

void FPSGraphModule::drawText(std::vector<PLModMenu_DrawCommand>& cmds,
                              const std::string& text,
                              float x, float y, float size, uint32_t color) const {
    PLModMenu_DrawCommand cmd = {};
    cmd.type = PL_DRAW_TEXT;
    cmd.x = x;
    cmd.y = y;
    cmd.w = calcTextWidth(text, size) + 8.0f;
    cmd.h = size + 2.0f;
    cmd.color = color;
    cmd.size = size;
    cmd.text = text.c_str();
    cmds.push_back(cmd);
}

void FPSGraphModule::drawMetricRow(
    std::vector<PLModMenu_DrawCommand>& cmds,
    const char* label,
    float value,
    const char* unit,
    const std::vector<float>& history,
    float minValue,
    float maxValue,
    uint32_t color,
    float x,
    float y,
    float width,
    float rowHeight) const {

    char valueBuffer[48];
    if (std::string(unit) == "gb")
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.1f", value);
    else if (std::string(unit) == "ms")
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.1f", value);
    else if (std::string(unit) == "tps")
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.1f", value);
    else
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.0f", value);

    const float labelX = x + 8.0f;
    const float valueX = x + width * 0.57f;
    const float unitX = x + width * 0.83f;

    drawText(cmds, label, labelX, y, m_size, 0xFFD9E2EE);
    drawText(cmds, valueBuffer, valueX, y, m_size + 1.0f, color);
    drawText(cmds, unit, unitX, y + 1.0f, m_size - 1.0f, 0xFFD0A66A);

    const float graphY = y + m_size + 5.0f;
    const float graphH = std::max(8.0f, rowHeight - m_size - 7.0f);

    PLModMenu_DrawCommand base = {};
    base.type = PL_DRAW_RECT_FILLED;
    base.x = x + 8.0f;
    base.y = graphY;
    base.w = width - 16.0f;
    base.h = graphH;
    base.color = applyAlpha(0x182331, 0.92f);
    cmds.push_back(base);

    if (history.empty())
        return;

    const float range = std::max(0.001f, maxValue - minValue);
    const float step = (width - 16.0f) / static_cast<float>(std::max<std::size_t>(1, history.size()));
    const float barW = std::max(1.0f, step * 0.82f);

    const std::size_t start = history.size() > static_cast<std::size_t>(m_historySize)
        ? history.size() - static_cast<std::size_t>(m_historySize)
        : 0;

    for (std::size_t i = start; i < history.size(); ++i) {
        const float sample = std::clamp(history[i], minValue, maxValue);
        const float normalized = (sample - minValue) / range;
        const float barH = std::max(1.0f, graphH * normalized);
        const float bx = x + 8.0f + static_cast<float>(i - start) * step;
        const float by = graphY + graphH - barH;

        PLModMenu_DrawCommand bar = {};
        bar.type = PL_DRAW_RECT_FILLED;
        bar.x = bx;
        bar.y = by;
        bar.w = barW;
        bar.h = barH;
        bar.color = color;
        cmds.push_back(bar);
    }
}

void FPSGraphModule::onFrame() {
    if (!enabled)
        return;

    const auto now = Clock::now();
    float realFps = m_currentFps;
    float frameMs = 0.0f;

    if (m_hasLastFrame) {
        const float seconds = std::chrono::duration<float>(now - m_lastFrame).count();
        if (seconds > 0.000001f && seconds < 2.0f) {
            realFps = 1.0f / seconds;
            frameMs = seconds * 1000.0f;
        }
    } else {
        m_hasLastFrame = true;
    }

    m_lastFrame = now;

    const float displayedFps = getDisplayedFps(realFps);
    m_currentFps = displayedFps;

    // Jitter is the standard deviation of recent frame times.
    pushHistory(m_jitterHistory, frameMs);
    if (m_jitterHistory.size() >= 2) {
        const float mean = std::accumulate(m_jitterHistory.begin(), m_jitterHistory.end(), 0.0f) /
                           static_cast<float>(m_jitterHistory.size());
        float variance = 0.0f;
        for (float sample : m_jitterHistory) {
            const float d = sample - mean;
            variance += d * d;
        }
        m_jitterMs = std::sqrt(variance / static_cast<float>(m_jitterHistory.size()));
    }

    m_ramGb = readResidentMemoryGb();

    // Keep diagnostics based on real FPS, not the cosmetic fake-performance value.
    pushHistory(m_fpsHistory, realFps);
    pushHistory(m_ramHistory, m_ramGb);
    rebuildStatistics();
    pushHistory(m_low1History, m_onePercentLow);

    std::vector<PLModMenu_DrawCommand> cmds;

    const float x = hudPosX;
    const float y = hudPosY;
    const float w = std::max(260.0f, m_width);
    const float h = std::max(120.0f, m_height);

    if (m_background) {
        PLModMenu_DrawCommand bg = {};
        bg.type = PL_DRAW_RECT_FILLED;
        bg.x = x;
        bg.y = y;
        bg.w = w;
        bg.h = h;
        bg.color = applyAlpha(0x070B10, m_backgroundOpacity);
        cmds.push_back(bg);
    }

    if (m_numbersOnly) {
        const float left = x + 12.0f;
        const float valueX = x + w * 0.58f;
        const float unitX = x + w * 0.83f;
        float row = y + 8.0f;

        auto numberRow = [&](const char* label, float value, const char* unit,
                             int decimals, uint32_t color) {
            char buf[64];
            if (decimals == 0)
                std::snprintf(buf, sizeof(buf), "%.0f", value);
            else if (decimals == 1)
                std::snprintf(buf, sizeof(buf), "%.1f", value);
            else
                std::snprintf(buf, sizeof(buf), "%.2f", value);

            drawText(cmds, label, left, row, m_size, 0xFFD9E2EE);
            drawText(cmds, buf, valueX, row, m_size + 1.0f, color);
            drawText(cmds, unit, unitX, row + 1.0f, m_size - 1.0f, 0xFFD0A66A);
            row += m_size + 8.0f;
        };

        numberRow("FPS", m_currentFps, "fps", 0, 0xFF39F08A);
        numberRow("Avg", m_averageFps, "fps", 0, 0xFF39F08A);

        PLModMenu_DrawCommand divider = {};
        divider.type = PL_DRAW_RECT_FILLED;
        divider.x = x + 12.0f;
        divider.y = row + 2.0f;
        divider.w = w - 24.0f;
        divider.h = 2.0f;
        divider.color = 0xFF344553;
        cmds.push_back(divider);
        row += 10.0f;

        numberRow("Jitter", m_jitterMs, "ms", 1, 0xFFB66CFF);
        numberRow("RAM", m_ramGb, "gb", 1, 0xFF4EEA83);
        numberRow("1%Low", m_onePercentLow, "fps", 0, 0xFFFFB24C);

        submitDrawCommands(moduleId, cmds);
        return;
    }

    // Header: FPS and average.
    drawText(cmds, "FPS", x + 12.0f, y + 8.0f, m_size + 2.0f, 0xFFD9E2EE);
    char fpsBuf[32];
    std::snprintf(fpsBuf, sizeof(fpsBuf), "%.0f", m_currentFps);
    drawText(cmds, fpsBuf, x + w * 0.62f, y + 5.0f, m_size + 5.0f, 0xFF39F08A);
    drawText(cmds, "fps", x + w * 0.84f, y + 9.0f, m_size, 0xFFD0A66A);

    char avgBuf[32];
    std::snprintf(avgBuf, sizeof(avgBuf), "%.0f", m_averageFps);
    drawText(cmds, "Avg", x + 12.0f, y + 31.0f, m_size + 1.0f, 0xFFD9E2EE);
    drawText(cmds, avgBuf, x + w * 0.62f, y + 28.0f, m_size + 3.0f, 0xFF39F08A);
    drawText(cmds, "fps", x + w * 0.84f, y + 32.0f, m_size - 1.0f, 0xFFD0A66A);

    PLModMenu_DrawCommand divider = {};
    divider.type = PL_DRAW_RECT_FILLED;
    divider.x = x + 12.0f;
    divider.y = y + 55.0f;
    divider.w = w - 24.0f;
    divider.h = 2.0f;
    divider.color = 0xFF344553;
    cmds.push_back(divider);

    const float startY = y + 63.0f;
    const float available = h - 69.0f;
    const float rowH = std::max(27.0f, available / 3.0f);

    // The graph is deliberately made from filled rectangular samples rather
    // than a single connected line, matching the blocky reference UI.
    drawMetricRow(cmds, "Jitter", m_jitterMs, "ms", m_jitterHistory,
                  0.0f, std::max(5.0f, m_jitterMs * 2.0f + 1.0f),
                  0xFF9E69E8, x, startY + rowH * 0.0f, w, rowH);

    drawMetricRow(cmds, "RAM", m_ramGb, "gb", m_ramHistory,
                  0.0f, std::max(2.0f, m_ramGb * 1.5f + 0.5f),
                  0xFF55C66B, x, startY + rowH * 1.0f, w, rowH);

    drawMetricRow(cmds, "1%Low", m_onePercentLow, "fps", m_low1History,
                  0.0f, std::max(60.0f, m_scaleFps),
                  0xFFD18C27, x, startY + rowH * 2.0f, w, rowH);

    submitDrawCommands(moduleId, cmds);
}

void FPSGraphModule::loadConfig(const nlohmann::json& j) {
    Module::loadConfig(j);

    if (j.contains("hudPosX")) hudPosX = j["hudPosX"].get<float>();
    if (j.contains("hudPosY")) hudPosY = j["hudPosY"].get<float>();
    if (j.contains("isHudModule")) isHudModule = j["isHudModule"].get<bool>();
    if (j.contains("m_width")) m_width = j["m_width"].get<float>();
    if (j.contains("m_height")) m_height = j["m_height"].get<float>();
    if (j.contains("m_size")) m_size = j["m_size"].get<float>();
    if (j.contains("m_historySize")) m_historySize = j["m_historySize"].get<int>();
    if (j.contains("m_scaleFps")) m_scaleFps = j["m_scaleFps"].get<float>();
    if (j.contains("m_background")) m_background = j["m_background"].get<bool>();
    if (j.contains("m_backgroundOpacity")) m_backgroundOpacity = j["m_backgroundOpacity"].get<float>();
    if (j.contains("m_showStats")) m_showStats = j["m_showStats"].get<bool>();
    if (j.contains("m_showGrid")) m_showGrid = j["m_showGrid"].get<bool>();
    if (j.contains("m_numbersOnly")) m_numbersOnly = j["m_numbersOnly"].get<bool>();
    if (j.contains("m_superPerformanceModeThing")) m_superPerformanceModeThing = j["m_superPerformanceModeThing"].get<bool>();

    m_width = std::max(260.0f, m_width);
    m_height = std::max(120.0f, m_height);
    m_size = std::max(10.0f, m_size);
    m_historySize = std::clamp(m_historySize, 16, 240);
    m_scaleFps = std::max(30.0f, m_scaleFps);
    m_backgroundOpacity = std::clamp(m_backgroundOpacity, 0.0f, 1.0f);
}

void FPSGraphModule::saveConfig(nlohmann::json& j) {
    Module::saveConfig(j);

    j["hudPosX"] = hudPosX;
    j["hudPosY"] = hudPosY;
    j["isHudModule"] = isHudModule;
    j["m_width"] = m_width;
    j["m_height"] = m_height;
    j["m_size"] = m_size;
    j["m_historySize"] = m_historySize;
    j["m_scaleFps"] = m_scaleFps;
    j["m_background"] = m_background;
    j["m_backgroundOpacity"] = m_backgroundOpacity;
    j["m_showStats"] = m_showStats;
    j["m_showGrid"] = m_showGrid;
    j["m_numbersOnly"] = m_numbersOnly;

    // Keep this as the final FPS-specific setting in the JSON object.
    j["m_superPerformanceModeThing"] = m_superPerformanceModeThing;
}
