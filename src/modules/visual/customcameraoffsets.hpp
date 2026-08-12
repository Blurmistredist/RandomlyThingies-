#pragma once

#include "../Module.hpp"
#include <bedrocktools/Api.hpp>
#include <bedrocktools/events/Events.hpp>
#include <bedrocktools/sdk/Types.hpp>

class CustomCameraOffsetsModule : public Module {
public:
    CustomCameraOffsetsModule();

    void onInit() override;
    void onEnable() override;
    void onDisable() override;
    void onFrame() override;
    void loadConfig(const nlohmann::json& j) override;
    void saveConfig(nlohmann::json& j) override;

private:
    void updateCamera(bedrocktools::sdk::ClientInstance* client);
    bool isThirdPersonActive(
        bedrocktools::sdk::ClientInstance* client) const;

    bedrocktools::sdk::Vec3 getDesiredCameraPosition(
        bedrocktools::sdk::ClientInstance* client) const;
    bedrocktools::sdk::Vec3 collisionCorrect(
        bedrocktools::sdk::ClientInstance* client,
        const bedrocktools::sdk::Vec3& origin,
        const bedrocktools::sdk::Vec3& desired) const;

    float m_distance = 4.0f;
    float m_minDistance = 0.75f;
    float m_maxDistance = 10.0f;
    float m_shoulderX = 0.0f;
    float m_height = 1.55f;
    float m_yawOffset = 0.0f;
    float m_pitchOffset = 0.0f;
    float m_positionSmoothing = 0.0f;
    bool m_dynamicDistance = false;
    bool m_onlyThirdPerson = true;

    bool m_initialized = false;
    bool m_haveCamera = false;
    bedrocktools::sdk::Vec3 m_smoothedCamera{};
};
