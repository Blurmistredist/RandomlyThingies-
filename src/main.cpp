#include "core/Runtime.hpp"
#include <pl/Mod.hpp>

class RandomlyThingiesMod {
public:
    static RandomlyThingiesMod& instance() {
        static RandomlyThingiesMod mod;
        return mod;
    }

    bool load(pl::mod::ModContext& context) {
        return randomlythingies::core::Runtime::get().load(context);
    }

    bool enable(pl::mod::ModContext& context) {
        return randomlythingies::core::Runtime::get().enable(context);
    }

    bool disable(pl::mod::ModContext& context) {
        return randomlythingies::core::Runtime::get().disable(context);
    }

    bool unload(pl::mod::ModContext& context) {
        return randomlythingies::core::Runtime::get().unload(context);
    }
};

PL_REGISTER_MOD(RandomlyThingiesMod, RandomlyThingiesMod::instance())
