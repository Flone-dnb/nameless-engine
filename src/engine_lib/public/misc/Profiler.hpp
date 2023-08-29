#pragma once

#if defined(ENABLE_PROFILER)

#include "Remotery/lib/Remotery.h"

// Source: https://github.com/Celtoys/Remotery/issues/46
#define RMT_BEGIN_CPU_SAMPLE_AUTO_NAME()                                                                     \
    RMT_OPTIONAL(RMT_ENABLED, {                                                                              \
        static rmtU32 rmt_sample_hash_##__LINE__ = 0;                                                        \
        _rmt_BeginCPUSample(__FUNCTION__, 0, &rmt_sample_hash_##__LINE__);                                   \
    })
#define PROFILE_FUNC                                                                                         \
    RMT_OPTIONAL(RMT_ENABLED, RMT_BEGIN_CPU_SAMPLE_AUTO_NAME());                                             \
    RMT_OPTIONAL(RMT_ENABLED, rmt_EndCPUSampleOnScopeExit rmt_ScopedCPUSample##__LINE__);

#define PROFILE_SCOPE(name) rmt_ScopedCPUSample(name, 0);

#define PROFILE_SCOPE_START(name) rmt_BeginCPUSample(name, 0);
#define PROFILE_SCOPE_END rmt_EndCPUSample();

namespace ne {
    /** Singleton helper class to globally initialize/terminate Remotery. */
    class Profiler {
    public:
        Profiler(const Profiler&) = delete;
        Profiler& operator=(const Profiler&) = delete;

        /** Terminates Remotery. */
        ~Profiler() { rmt_DestroyGlobalInstance(pRemotery); }

        /** Creates a static Profiler instance. */
        static void initialize() { static Profiler profiler; }

    private:
        /** Initializes Remotery. */
        Profiler() { rmt_CreateGlobalInstance(&pRemotery); }

        /** Stores Remotery instance. */
        Remotery* pRemotery = nullptr;
    };
} // namespace ne

#else

#define PROFILE_FUNC
#define PROFILE_SCOPE(name)
#define PROFILE_SCOPE_START(name)
#define PROFILE_SCOPE_END

#endif
