#include "Runtime.hpp"

#include "modules/ModuleRegistry.hpp"
#include "launcher/ModuleMenu.hpp"
#include "config/ConfigManager.hpp"
#include "core/memory/Hooks.hpp"

#include <bedrocktools/Api.hpp>
#include <bedrocktools/events/EventBus.hpp>
#include <bedrocktools/events/Events.hpp>
#include <pl/Input.hpp>

#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <unistd.h>
#include <fcntl.h>

namespace randomlythingies::core {
namespace {

std::mutex g_installMutex;
thread_local bool g_insideDlopen = false;

using DlopenFn = void*(*)(const char*, int);
DlopenFn g_dlopenOriginal = nullptr;
randomlythingies::hooks::Handle g_dlopenHook = nullptr;

Runtime* g_runtime = nullptr;

void* dlopenDetour(const char* filename, int flags) {
    void* handle = g_dlopenOriginal
        ? g_dlopenOriginal(filename, flags)
        : nullptr;

    if (handle &&
        filename &&
        !g_insideDlopen &&
        std::strstr(filename, "libBedrockTools.so")) {

        g_insideDlopen = true;
        if (g_runtime) {
            g_runtime->tryInstallFromLoadHook();
        }
        g_insideDlopen = false;
    }

    return handle;
}

}

Runtime& Runtime::get() {
    static Runtime runtime;
    g_runtime = &runtime;
    return runtime;
}

bool Runtime::launcherContext() const {
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) return false;

    char command[256]{};
    const auto size = read(fd, command, sizeof(command) - 1);
    close(fd);

    if (size <= 0) return false;

    return std::strcmp(command, "org.levimc.launcher") == 0 ||
           std::strcmp(command, "org.levimc.launcher:minecraft") == 0 ||
           std::strcmp(command, "com.mojang.minecraftpe") == 0;
}

void Runtime::installBedrockToolsLoadHook() {
    if (g_dlopenHook) return;

    auto libdl =
        randomlythingies::hooks::openLibrary("libdl.so");

    if (!libdl) return;

    const auto symbol =
        randomlythingies::hooks::symbol(libdl, "dlopen");

    if (symbol) {
        g_dlopenHook =
            randomlythingies::hooks::install(
                reinterpret_cast<void*>(symbol),
                reinterpret_cast<void*>(dlopenDetour),
                reinterpret_cast<void**>(&g_dlopenOriginal));
    }

    randomlythingies::hooks::closeLibrary(libdl);
}

void Runtime::wireEvents() {
    if (!mSubscriptions.empty()) return;

    using namespace bedrocktools::events;

    const auto* api = bedrocktools::api::find();
    if (!bedrocktools::api::compatible(api) || !api->subscribe)
        return;

    auto subscribe =
        [&](EventType type, bedrocktools::api::EventCallback cb) {
            const auto id =
                api->subscribe(
                    type,
                    EventPriority::Normal,
                    cb,
                    this);

            if (id) mSubscriptions.push_back(id);
        };

    subscribe(EventType::Frame,
        [](EventType, void*, void* user) {
            auto* runtime =
                static_cast<Runtime*>(user);
            (void)runtime;

            FrameEvent event;
            ModuleRegistry::get().onFrame();
        });

    subscribe(EventType::MouseInput,
        [](EventType, void* payload, void*) {
            if (!payload) return;

            auto* original =
                static_cast<MouseInputEvent*>(payload);

            MouseInputEvent local(
                original->button,
                original->down);

            if (ModuleRegistry::get().onMouseEvent(
                    local.button,
                    local.down)) {
                local.cancel();
            }

            if (local.cancelled())
                original->cancel();
        });

    subscribe(EventType::ScreenState,
        [](EventType, void* payload, void*) {
            if (!payload) return;

            auto* original =
                static_cast<ScreenStateEvent*>(payload);

            static int containerDepth = 0;
            static int chatDepth = 0;

            if (original->screen == ScreenKind::Container) {
                if (original->phase == ScreenPhase::Opened)
                    ++containerDepth;
                else if (containerDepth > 0)
                    --containerDepth;
            } else {
                if (original->phase == ScreenPhase::Opened)
                    ++chatDepth;
                else if (chatDepth > 0)
                    --chatDepth;
            }

            ModuleRegistry::get().setKeybindBlocked(
                containerDepth > 0 || chatDepth > 0);
        });
}

bool Runtime::bedrockToolsReady() const {
    // The public API being available is the runtime readiness condition.
    // Do NOT gate the entire extension on one particular signature such as
    // GetPerspective: FPS Graph only needs the event API, while modules that
    // require signatures can resolve those targets individually. Gating the
    // whole registry on GetPerspective made every module silently dead when
    // that one signature was unavailable or had not finished resolving yet.
    const auto* api = bedrocktools::api::find();
    return bedrocktools::api::compatible(api) &&
           api->subscribe &&
           api->unsubscribe;
}

bool Runtime::tryInstallFromLoadHook() {
    return install();
}

bool Runtime::install() {
    std::lock_guard lock(g_installMutex);

    if (mInstalled)
        return true;

    if (!bedrockToolsReady())
        return false;

    registerAllModules();
    wireEvents();
    ModuleRegistry::get().initialize();

    // Load persisted configuration only after all modules have completed
    // onInit(), matching the lifecycle expected by the BedrockTools API.
    randomlythingies::config::ConfigManager::get().load();

    mInstalled = true;
    return true;
}

bool Runtime::load(pl::mod::ModContext& context) {
    mResourceDirectory = context.resourceDir();

    randomlythingies::config::ConfigManager::get()
        .setConfigPath(
            (context.configDir() / "config.json").string());

    if (!launcherContext())
        return true;

    // Register modules before installation so the runtime can initialize
    // every module (onInit) before loading persisted configuration.
    registerAllModules();

    // Register module menu entries immediately. The modules themselves are
    // initialized only once BedrockTools reports that signature resolution
    // has completed.
    if (!mMenuRegistered) {
        registerModulesWithLauncher();
        mMenuRegistered = true;
    }

    if (!install()) {
        installBedrockToolsLoadHook();
    }

    return true;
}

bool Runtime::enable(pl::mod::ModContext& context) {
    (void)context;

    if (!launcherContext())
        return true;

    install();
    return true;
}

bool Runtime::disable(pl::mod::ModContext&) {
    randomlythingies::config::ConfigManager::get().flush();
    return true;
}

bool Runtime::unload(pl::mod::ModContext&) {
    randomlythingies::config::ConfigManager::get().flush();
    return true;
}

}
