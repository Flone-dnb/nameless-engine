#pragma once

// Standard.
#include <array>
#include <memory>
#include <atomic>
#include <mutex>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "math/GLMath.hpp"
#include "render/general/resources/FrameResource.h"

// External.
#include "directx/d3dx12.h"

// OS.
#include <wrl.h>

struct ID3D12CommandAllocator;

namespace ne {
    class Renderer;
#if defined(WIN32)
    using namespace Microsoft::WRL;
#endif

    /** Stores objects used by one frame. */
    struct DirectXFrameResource : public FrameResource {
        virtual ~DirectXFrameResource() override = default;

        /** Stores frame commands from command lists. */
        ComPtr<ID3D12CommandAllocator> pCommandAllocator;

        /** Current fence value of the resource. */
        unsigned long long iFence = 0;

    private:
        /**
         * Called by frame resource manager after a frame resource was constructed to initialize its fields.
         *
         * @param pRenderer Used renderer.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> initialize(Renderer* pRenderer) override;
    };
} // namespace ne
