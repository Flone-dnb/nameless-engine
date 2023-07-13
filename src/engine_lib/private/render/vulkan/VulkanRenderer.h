#pragma once

// Custom.
#include "render/Renderer.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    /** Renderer made with Vulkan API. */
    class VulkanRenderer : public Renderer {
    public:
        VulkanRenderer() = delete;
        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        virtual ~VulkanRenderer() override = default;

        /**
         * Creates a new DirectX renderer.
         *
         * @param pGameManager GameManager object that owns this renderer.
         *
         * @return Error if something went wrong (for ex. if the hardware does not support this
         * renderer), otherwise created renderer.
         */
        static std::variant<std::unique_ptr<Renderer>, Error> create(GameManager* pGameManager);

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @return Error if can't find any GPU that supports this renderer,
         * vector with GPU names if successful.
         */
        virtual std::variant<std::vector<std::string>, Error> getSupportedGpuNames() const override;

        /**
         * Returns a list of supported render resolution (pairs of width and height).
         *
         * @return Error if something went wrong, otherwise render resolutions.
         */
        virtual std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRenderResolutions() const override;

        /**
         * Returns a list of supported screen refresh rates (pairs of numerator and denominator).
         *
         * @remark The list of supported refresh rates depends on the currently used GPU,
         * so if changing used GPU this list might return different values.
         *
         * @return Error if something went wrong, otherwise refresh rates.
         */
        virtual std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRefreshRates() const override;

        /**
         * Returns renderer's type.
         *
         * @return Renderer's type.
         */
        virtual RendererType getType() const override;

        /**
         * Returns API version or a feature level that the renderer uses.
         *
         * For example DirectX renderer will return used feature level and Vulkan renderer
         * will return used Vulkan API version.
         *
         * @return Used API version.
         */
        virtual std::string getUsedApiVersion() const override;

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::string getCurrentlyUsedGpuName() const override;

        /**
         * Returns total video memory (VRAM) size in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const override;

        /**
         * Returns the amount of video memory (VRAM) that is currently being used by the renderer.
         *
         * @remark Does not return global (system-wide) used VRAM size, only VRAM used by the renderer
         * (i.e. only VRAM used by this application).
         *
         * @return Size of the video memory used by the renderer in megabytes.
         */
        virtual size_t getUsedVideoMemoryInMb() const override;

        /**
         * Blocks the current thread until the GPU finishes executing all queued commands up to this point.
         *
         * @remark Typically used with @ref getRenderResourcesMutex.
         */
        virtual void waitForGpuToFinishWorkUpToThisPoint() override;

    protected:
        /**
         * Creates an empty (uninitialized) renderer.
         *
         * @remark Use @ref initialize to initialize the renderer.
         *
         * @param pGameManager GameManager object that owns this renderer.
         */
        VulkanRenderer(GameManager* pGameManager);

        /**
         * Compiles/verifies all essential shaders that the engine will use.
         *
         * @remark This is the last step in renderer initialization that is executed after the
         * renderer was tested to support the hardware.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> compileEngineShaders() const override;

        /** Submits a new frame to the GPU. */
        virtual void drawNextFrame() override;

        /**
         * Recreates all render buffers to match current settings.
         *
         * @remark Usually called after some renderer setting was changed or window size changed.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateRenderBuffers() override;

        /**
         * (Re)creates depth/stencil buffer.
         *
         * @remark Make sure that the old depth/stencil buffer (if was) is not used by the GPU before
         * calling this function.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> createDepthStencilBuffer() override;

        /**
         * Tells whether the renderer is initialized or not.
         *
         * Initialized renderer means that the hardware supports it and it's safe to use renderer
         * functionality such as @ref updateRenderBuffers.
         *
         * @return Whether the renderer is initialized or not.
         */
        virtual bool isInitialized() const override;

    private:
        /**
         * Initializes the renderer.
         *
         * @remark This function is usually called after constructing a new empty (uninitialized)
         * DirectX renderer.
         *
         * @return Error if something went wrong (for ex. if the hardware does not support this
         * renderer), otherwise created renderer.
         */
        [[nodiscard]] std::optional<Error> initialize();

        /** Version of the Vulkan API that the renderer uses. */
        static constexpr uint32_t iUsedVulkanVersion = VK_API_VERSION_1_0;
    };
} // namespace ne
