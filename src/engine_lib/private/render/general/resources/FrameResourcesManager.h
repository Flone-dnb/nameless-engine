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
#include "materials/VulkanAlignmentConstants.hpp"

// External.
#if defined(WIN32)
#include "directx/d3dx12.h"
#endif

// OS.
#if defined(WIN32)
#include <wrl.h>
#endif

#if defined(WIN32)
struct ID3D12CommandAllocator;
#endif

namespace ne {
#if defined(WIN32)
    using namespace Microsoft::WRL;
#endif

    class UploadBuffer;
    class Renderer;

    /**
     * Stores frame-global constants. Used by shaders.
     *
     * @warning Should be structured properly to be packed as a constant buffer in shaders.
     */
    struct FrameConstants {
        /** Camera's view matrix multiplied by camera's projection matrix. */
        alignas(iVkMat4Alignment) glm::mat4x4 viewProjectionMatrix = glm::identity<glm::mat4x4>();

        /** Camera's world location. */
        alignas(iVkVec3Alignment) glm::vec3 cameraPosition = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Time that has passed since the last frame in seconds (i.e. delta time). */
        alignas(iVkScalarAlignment) float timeSincePrevFrameInSec = 0.0F;

        /** Time since the first window was created (in seconds). */
        alignas(iVkScalarAlignment) float totalTimeInSec = 0.0F;

        // don't forget to add padding to 4 floats (if needed) for HLSL packing rules
    };

    /** Stores objects used by one frame. */
    struct FrameResource {
#if defined(WIN32)
        /** Stores frame commands from command lists. */
        ComPtr<ID3D12CommandAllocator> pCommandAllocator;

        /**  */
        unsigned long long iFence = 0;
#endif

        /** Stores frame-global constants, such as camera position, time, various matrices, etc. */
        std::unique_ptr<UploadBuffer> pFrameConstantBuffer;
    };

    /**
     * Controls GPU resources (mostly constant buffers) that contain frame-specific data.
     *
     * Used to not wait for the GPU to finish drawing a frame on the CPU side
     * and instead continue drawing another frame (on the CPU side) without touching resources
     * that can be in use by the GPU because it's drawing the previous frame.
     */
    class FrameResourcesManager {
    public:
        /** Stores index and pointer to the current item in @ref vFrameResources. */
        struct CurrentFrameResource {
            /** Current index in frame resources array. */
            size_t iCurrentFrameResourceIndex = 0;

            /** Pointer to item in the index @ref iCurrentFrameResourceIndex. */
            FrameResource* pResource = nullptr;
        };

        FrameResourcesManager() = delete;
        FrameResourcesManager(const FrameResourcesManager&) = delete;
        FrameResourcesManager& operator=(const FrameResourcesManager&) = delete;

        /**
         * Returns the number of used frame resources.
         *
         * @return Number of frame resources being used.
         */
        static constexpr unsigned int getFrameResourcesCount() { return iFrameResourcesCount; }

        /**
         * Creates a new frame resources manager.
         *
         * @param pRenderer Renderer that owns this manager.
         *
         * @return Error if something went wrong, otherwise created manager.
         */
        static std::variant<std::unique_ptr<FrameResourcesManager>, Error> create(Renderer* pRenderer);

        /**
         * Returns currently used frame resource.
         * Must be used with mutex.
         *
         * @return Current frame resource.
         */
        std::pair<std::recursive_mutex, CurrentFrameResource>* getCurrentFrameResource();

        /**
         * Uses mutex from @ref getCurrentFrameResource to switch to the next available frame resource.
         *
         * @remark After this function is finished calls to @ref getCurrentFrameResource will return next
         * frame resource.
         *
         * @remark Next frame resource (that we switched to) can still be used by the GPU, it's up to the
         * caller to check whether the frame resource is used by the GPU or not.
         */
        void switchToNextFrameResource();

    private:
        /**
         * Creates uninitialized manager.
         *
         * @param pRenderer Renderer that owns this manager.
         */
        FrameResourcesManager(Renderer* pRenderer);

        /** Renderer that owns this manager. */
        Renderer* pRenderer = nullptr;

        /** Points to the currently used item from @ref vFrameResources. */
        std::pair<std::recursive_mutex, CurrentFrameResource> mtxCurrentFrameResource;

        /** Number of frames "in-flight" that the CPU can submit to the GPU without waiting. */
        static constexpr unsigned int iFrameResourcesCount = 3;

        /** Array of frame-specific resources, all contain the same data. */
        std::array<std::unique_ptr<FrameResource>, iFrameResourcesCount> vFrameResources;
    };
} // namespace ne
