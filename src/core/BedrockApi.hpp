#pragma once

#include <bedrocktools/Api.hpp>
#include <bedrocktools/memory/Signatures.hpp>
#include <cstdint>

namespace randomlythingies::bedrock {

inline const bedrocktools::api::ApiV1* api() {
    return bedrocktools::api::find();
}

inline std::uintptr_t resolve(
    bedrocktools::memory::SignatureId id) {

    const auto* runtime = api();
    return bedrocktools::api::resolve(id, runtime);
}

inline bedrocktools::sdk::ClientInstance* clientInstance() {
    const auto* runtime = api();
    return (bedrocktools::api::compatible(runtime) &&
            runtime->clientInstance)
        ? runtime->clientInstance()
        : nullptr;
}

}
