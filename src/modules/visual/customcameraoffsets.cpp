#include "customcameraoffsets.hpp"

#include <bedrocktools/events/EventBus.hpp>
#include <bedrocktools/sdk/client/ClientInstance.hpp>
#include <bedrocktools/sdk/render/LevelRenderer.hpp>
#include <bedrocktools/sdk/render/LevelRendererPlayer.hpp>
#include <bedrocktools/sdk/world/Actor.hpp>
#include <bedrocktools/sdk/world/BlockSource.hpp>

#include <algorithm>
#include <cmath>

namespace {
using namespace bedrocktools;

static inline sdk::Vec3 add(const sdk::Vec3& a, const sdk::Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
static inline sdk::Vec3 sub(const sdk::Vec3& a, const sdk::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
static inline sdk::Vec3 mul(const sdk::Vec3& a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}
static inline float len(const sdk::Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
static inline sdk::Vec3 norm(const sdk::Vec3& v) {
    float l = len(v);
    return l > 0.0001f ? mul(v, 1.0f / l) : sdk::Vec3{0, 0, 1};
}
static inline float deg2rad(float d) {
    return d * 0.01745329251994329577f;
}
}

CustomCameraOffsetsModule::CustomCameraOffsetsModule()
    : Module("custom-camera-offsets",
             "Custom third-person camera offsets with collision correction") {}

void CustomCameraOffsetsModule::onInit() {
    m_initialized = true;

    // Use the same runtime/event infrastructure as BedrockTools rather than
    // installing a second RenderLevel/GetPerspective/HudCursor detour.
    m_clientUpdateSubscription =
        bedrocktools::events::bus().subscribe<
            bedrocktools::events::ClientInstanceUpdateEvent>(
            [this](const auto& event) {
                onClientInstanceUpdate(event);
            },
            bedrocktools::events::EventPriority::Last);
}

void CustomCameraOffsetsModule::onEnable() {
    m_haveCamera = false;
}

void CustomCameraOffsetsModule::onDisable() {
    m_haveCamera = false;
}

void CustomCameraOffsetsModule::onFrame() {
    // Vanilla Bedrock cursor remains untouched. Bedrock's normal input/raycast
    // path therefore remains responsible for interaction targeting.
}

void CustomCameraOffsetsModule::onClientInstanceUpdate(
    const bedrocktools::events::ClientInstanceUpdateEvent& event) {

    if (!m_initialized || !event.clientInstance)
        return;

    updateCamera(event.clientInstance);
}

bool CustomCameraOffsetsModule::isThirdPersonActive(
    bedrocktools::sdk::ClientInstance* client) const {

    if (!client || !client->levelRenderer() || !client->levelRenderer()->playerRenderer())
        return false;

    auto* player = client->localPlayer();
    if (!player)
        return false;

    // Vanilla camera distance from the player is used as the third-person
    // discriminator. This avoids owning the GetPerspective hook.
    const auto playerPos = player->position();
    const auto cameraPos =
        client->levelRenderer()->playerRenderer()->cameraPosition();

    return len(sub(cameraPos, playerPos)) > 1.0f;
}

bedrocktools::sdk::Vec3 CustomCameraOffsetsModule::getDesiredCameraPosition(
    bedrocktools::sdk::ClientInstance* client) const {

    auto* player = client->localPlayer();
    auto* renderer = client->levelRenderer();
    auto* playerRenderer = renderer ? renderer->playerRenderer() : nullptr;

    if (!player || !playerRenderer)
        return {};

    const auto playerPos = player->position();
    const float yaw = deg2rad(player->rotation().y + m_yawOffset);
    const float pitch = deg2rad(player->rotation().x + m_pitchOffset);

    const sdk::Vec3 forward{
        -std::sin(yaw) * std::cos(pitch),
        -std::sin(pitch),
        std::cos(yaw) * std::cos(pitch)
    };
    const sdk::Vec3 right{
        std::cos(yaw),
        0.0f,
        std::sin(yaw)
    };

    sdk::Vec3 origin{
        playerPos.x,
        playerPos.y + m_height,
        playerPos.z
    };

    sdk::Vec3 desired = sub(origin, mul(norm(forward), m_distance));
    desired = add(desired, mul(right, m_shoulderX));
    return desired;
}

bedrocktools::sdk::Vec3 CustomCameraOffsetsModule::collisionCorrect(
    bedrocktools::sdk::ClientInstance* client,
    const sdk::Vec3& origin,
    const sdk::Vec3& desired) const {

    auto* region = client ? client->region() : nullptr;
    if (!region)
        return desired;

    const auto delta = sub(desired, origin);
    const float distance = len(delta);
    if (distance <= 0.001f)
        return desired;

    const auto direction = mul(delta, 1.0f / distance);
    constexpr float step = 0.08f;

    sdk::Vec3 lastSafe = origin;

    const std::size_t count =
        static_cast<std::size_t>(std::ceil(distance / step));

    auto* solidFn = reinterpret_cast<
        bool (*)(void*, const sdk::BlockPos*)
    >(bedrocktools::api::resolve(
        bedrocktools::memory::SignatureId::BlockSourceIsSolidBlockingBlock,
        bedrocktools::api::find()));

    if (!solidFn)
        return desired;

    for (std::size_t i = 1; i <= count; ++i) {
        const float d = std::min(distance, step * static_cast<float>(i));
        const sdk::Vec3 p = add(origin, mul(direction, d));

        sdk::BlockPos pos{
            static_cast<int>(std::floor(p.x)),
            static_cast<int>(std::floor(p.y)),
            static_cast<int>(std::floor(p.z))
        };

        if (solidFn(region, &pos))
            break;

        lastSafe = p;
    }

    return lastSafe;
}

void CustomCameraOffsetsModule::updateCamera(
    bedrocktools::sdk::ClientInstance* client) {

    if (!client || !m_initialized)
        return;

    if (m_onlyThirdPerson && !isThirdPersonActive(client))
        return;

    auto* renderer = client->levelRenderer();
    auto* playerRenderer = renderer ? renderer->playerRenderer() : nullptr;
    auto* player = client->localPlayer();

    if (!renderer || !playerRenderer || !player)
        return;

    const auto playerPos = player->position();
    const sdk::Vec3 origin{
        playerPos.x,
        playerPos.y + m_height,
        playerPos.z
    };

    const auto desired = collisionCorrect(
        client,
        origin,
        getDesiredCameraPosition(client));

    if (!m_haveCamera || m_positionSmoothing <= 0.0f) {
        m_smoothedCamera = desired;
        m_haveCamera = true;
    } else {
        const float alpha =
            std::clamp(m_positionSmoothing, 0.0f, 1.0f);
        m_smoothedCamera.x +=
            (desired.x - m_smoothedCamera.x) * alpha;
        m_smoothedCamera.y +=
            (desired.y - m_smoothedCamera.y) * alpha;
        m_smoothedCamera.z +=
            (desired.z - m_smoothedCamera.z) * alpha;
    }

    playerRenderer->cameraPosition() = m_smoothedCamera;
}
