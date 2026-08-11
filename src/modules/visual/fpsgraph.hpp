#pragma once

#include "../Module.hpp"
#include "../ModuleRegistry.hpp"
#include <chrono>
#include <random>
#include <vector>

class FPSGraphModule : public Module {
public:
    FPSGraphModule();
    ~FPSGraphModule() override;

    void onEnable() override;
    void onDisable() override;
    void onFrame() override;
    void loadConfig(const nlohmann::json& j) override;
    void saveConfig(nlohmann::json& j) override;

    float hudPosX = 24.0f;
    float hudPosY = 24.0f;
    bool isHudModule = true;

    // Main panel appearance.
    float m_width = 360.0f;
    float m_height = 300.0f;
    float m_size = 16.0f;
    int   m_historySize = 96;
    float m_scaleFps = 240.0f;
    bool  m_background = true;
    float m_backgroundOpacity = 0.86f;
    bool  m_showStats = true;
    bool  m_showGrid = true;

    // Compact alternative to the graph panel.
    bool m_numbersOnly = false;

    // Cosmetic easter egg. It changes displayed FPS only; it does not
    // change the game's real rendering performance.
    bool m_superPerformanceModeThing = false;

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_lastFrame{};
    Clock::time_point m_fakeStart{};
    Clock::time_point m_fakeNextJump{};
    bool m_hasLastFrame = false;
    bool m_fakeHighRange = false;
    float m_fakeDisplayedFps = 2500.0f;
    std::mt19937 m_fakeRng{std::random_device{}()};

    std::vector<float> m_fpsHistory;
    std::vector<float> m_jitterHistory;
    std::vector<float> m_ramHistory;
    std::vector<float> m_low1History;

    float m_currentFps = 0.0f;
    float m_averageFps = 0.0f;
    float m_jitterMs = 0.0f;
    float m_ramGb = 0.0f;
    float m_onePercentLow = 0.0f;

    float getDisplayedFps(float realFps);
    void pushHistory(std::vector<float>& history, float value);
    void rebuildStatistics();
    float readResidentMemoryGb() const;

    void drawText(std::vector<PLModMenu_DrawCommand>& cmds,
                  const std::string& text,
                  float x, float y, float size, uint32_t color) const;

    void drawMetricRow(std::vector<PLModMenu_DrawCommand>& cmds,
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
                       float rowHeight) const;
};
