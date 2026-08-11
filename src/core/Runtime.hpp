#pragma once

#include <pl/Mod.hpp>
#include <filesystem>
#include <cstdint>
#include <vector>

namespace randomlythingies::core {

class Runtime {
public:
    static Runtime& get();

    bool load(pl::mod::ModContext& context);
    bool enable(pl::mod::ModContext& context);
    bool disable(pl::mod::ModContext& context);
    bool unload(pl::mod::ModContext& context);
    bool tryInstallFromLoadHook();

private:
    bool install();
    bool launcherContext() const;
    bool bedrockToolsReady() const;
    void wireEvents();
    void installBedrockToolsLoadHook();

    std::filesystem::path mResourceDirectory;
    std::vector<std::uint64_t> mSubscriptions;
    bool mInstalled = false;
    bool mMenuRegistered = false;
};

}
