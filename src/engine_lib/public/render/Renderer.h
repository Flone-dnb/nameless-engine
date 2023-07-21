#pragma once

// Standard.
#include <variant>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>

// Custom.
#include "materials/ShaderReadWriteResourceManager.h"
#include "misc/Error.h"
#include "materials/ShaderManager.h"
#include "render/general/resources/GpuResourceManager.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "render/RenderSettings.h"

namespace ne {
    class GameManager;
    class Window;
    class PsoManager;
    class ShaderConfiguration;
    class RenderSettings;

    /** Defines a base class for renderers to implement. */
    class Renderer {
        // Only window should be able to request new frames to be drawn.
        friend class Window;

        // Can update shader configuration.
        friend class ShaderConfiguration;

        // Settings will modify renderer's state.
        friend class RenderSettings;

    public:
        Renderer() = delete;
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        virtual ~Renderer() = default;

        /**
         * Creates a new renderer.
         *
         * @param pGameManager      Game manager object that will own this renderer.
         * @param preferredRenderer Preferred renderer to be used.
         *
         * @return Error if no renderer could be created, otherwise created renderer.
         */
        static std::variant<std::unique_ptr<Renderer>, Error>
        create(GameManager* pGameManager, std::optional<RendererType> preferredRenderer);

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @remark Note that returned array might differ depending on the used renderer.
         *
         * @return Error if can't find any GPU that supports this renderer,
         * otherwise array with GPU names that can be used for this renderer.
         */
        virtual std::variant<std::vector<std::string>, Error> getSupportedGpuNames() const = 0;

        /**
         * Returns a list of supported render resolution (pairs of width and height).
         *
         * @return Error if something went wrong, otherwise render resolutions.
         */
        virtual std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRenderResolutions() const = 0;

        /**
         * Returns a list of supported screen refresh rates (pairs of numerator and denominator).
         *
         * @remark The list of supported refresh rates depends on the currently used GPU,
         * so if changing used GPU this list might return different values.
         *
         * @return Error if something went wrong, otherwise refresh rates.
         */
        virtual std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRefreshRates() const = 0;

        /**
         * Returns renderer's type.
         *
         * @return Renderer's type.
         */
        virtual RendererType getType() const = 0;

        /**
         * Returns API version or a feature level that the renderer uses.
         *
         * For example DirectX renderer will return used feature level and Vulkan renderer
         * will return used Vulkan API version.
         *
         * @return Used API version.
         */
        virtual std::string getUsedApiVersion() const = 0;

        /**
         * Returns render settings that can be configured.
         *
         * @remark Do not delete (free) returned pointer. Returning `std::shared_ptr` in a pair
         * not because you should copy it but because render settings are Serializable
         * and at the time of writing serialization/deserialization for `std::unique_ptr` is
         * not supported (so consider the `std::shared_ptr` in the pair to be a `std::unique_ptr`).
         *
         * @return Non-owning pointer to render settings.
         */
        std::pair<std::recursive_mutex, std::shared_ptr<RenderSettings>>* getRenderSettings();

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::string getCurrentlyUsedGpuName() const = 0;

        /**
         * Returns total video memory (VRAM) size in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const = 0;

        /**
         * Returns the amount of video memory (VRAM) that is currently being used by the renderer.
         *
         * @remark Does not return global (system-wide) used VRAM size, only VRAM used by the renderer
         * (i.e. only VRAM used by this application).
         *
         * @return Size of the video memory used by the renderer in megabytes.
         */
        virtual size_t getUsedVideoMemoryInMb() const = 0;

        /**
         * Blocks the current thread until the GPU finishes executing all queued commands up to this point.
         *
         * @remark Typically used with @ref getRenderResourcesMutex.
         */
        virtual void waitForGpuToFinishWorkUpToThisPoint() = 0;

        /**
         * Returns the current shader configuration (shader settings,
         * represented by a bunch of predefined macros).
         *
         * Must be used with mutex.
         *
         * @return Do not delete (free) returned pointer. Shader configuration.
         */
        std::pair<std::recursive_mutex, std::unique_ptr<ShaderConfiguration>>* getShaderConfiguration();

        /**
         * Returns the window that we render to.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Window we render to.
         */
        Window* getWindow() const;

        /**
         * Game manager object that owns this renderer.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Game manager object.
         */
        GameManager* getGameManager() const;

        /**
         * Returns shader manager used to compile shaders.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Shader manager.
         */
        ShaderManager* getShaderManager() const;

        /**
         * Returns PSO manager used to store graphics and compute PSOs.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return PSO manager.
         */
        PsoManager* getPsoManager() const;

        /**
         * Returns GPU resource manager.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return GPU resource manager.
         */
        GpuResourceManager* getResourceManager() const;

        /**
         * Returns frame resources manager.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Frame resources manager.
         */
        FrameResourcesManager* getFrameResourcesManager() const;

        /**
         * Returns shader read/write resource manager.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Shader read/write resource manager.
         */
        ShaderCpuReadWriteResourceManager* getShaderCpuReadWriteResourceManager() const;

        /**
         * Returns mutex used when reading or writing to render resources.
         * Usually used with @ref waitForGpuToFinishWorkUpToThisPoint.
         *
         * @return Mutex.
         */
        std::recursive_mutex* getRenderResourcesMutex();

    protected:
        /**
         * Constructor.
         *
         * @param pGameManager pGameManager object that owns this renderer.
         */
        Renderer(GameManager* pGameManager);

        /**
         * Compiles/verifies all essential shaders that the engine will use.
         *
         * @remark This is the last step in renderer initialization that is executed after the
         * renderer was tested to support the hardware.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> compileEngineShaders() const;

        /**
         * Collects array of engine shaders that will be compiled/verified.
         *
         * @return Array of shader descriptions to compile.
         */
        virtual std::vector<ShaderDescription> getEngineShadersToCompile() const = 0;

        /**
         * Returns the amount of buffers/images the swap chain has.
         *
         * @return The amount of buffers/images the swap chain has.
         */
        static consteval unsigned int getSwapChainBufferCount() { return iSwapChainBufferCount; }

        /** Submits a new frame to the GPU. */
        virtual void drawNextFrame() = 0;

        /**
         * Recreates all render buffers to match current settings.
         *
         * @remark Usually called after some renderer setting was changed or window size changed.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateRenderBuffers() = 0;

        /**
         * (Re)creates depth/stencil buffer.
         *
         * @remark Make sure that the old depth/stencil buffer (if was) is not used by the GPU before
         * calling this function.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> createDepthStencilBuffer() = 0;

        /**
         * Tells whether the renderer is initialized or not.
         *
         * Initialized renderer means that the hardware supports it and it's safe to use renderer
         * functionality such as @ref updateRenderBuffers.
         *
         * @return Whether the renderer is initialized or not.
         */
        virtual bool isInitialized() const = 0;

        /**
         * Initializes some parts of the renderer that require final renderer object to be constructed.
         *
         * @remark Must be called by derived classes during their initialization, specifically
         * before they start to initialize as derived classes should first initialize the base class
         * which setups essential things like render settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> initializeRenderer();

        /**
         * Initializes various resource managers.
         *
         * @remark Must be called by derived classes after base initialization
         * (for ex. in DirectX after device and video adapter were created).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> initializeResourceManagers();

    private:
        /**
         * Creates a new renderer and nothing else.
         *
         * This function is only used to pick a renderer including the specified preference
         * without doing any renderer finalization.
         *
         * @param pGameManager      Game manager object that will own this renderer.
         * @param preferredRenderer Renderer to prefer to create.
         *
         * @return `nullptr` if no renderer could be created, otherwise created renderer.
         */
        static std::unique_ptr<Renderer>
        createRenderer(GameManager* pGameManager, std::optional<RendererType> preferredRenderer);

        /**
         * Attempts to create a renderer of the specified type.
         *
         * This function is only used to pick a renderer including the specified preference
         * without doing any renderer finalization.
         *
         * @param type         Type of the renderer to create.
         * @param pGameManager Game manager object that will own this renderer.
         *
         * @return Error if the hardware/OS does not support this type of renderer, otherwise
         * created renderer.
         */
        static std::variant<std::unique_ptr<Renderer>, Error>
        createRenderer(RendererType type, GameManager* pGameManager);

        /**
         * Updates the current shader configuration (settings) based on the current value
         * from @ref getShaderConfiguration.
         *
         * @remark Flushes the command queue and recreates PSOs' internal resources so that they
         * will use new shader configuration.
         */
        void updateShaderConfiguration();

        /**
         * Initializes @ref mtxRenderSettings.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> initializeRenderSettings();

        /** Lock when reading or writing to render resources. Usually used with @ref
         * waitForGpuToFinishWorkUpToThisPoint. */
        std::recursive_mutex mtxRwRenderResources;

        /** Used to create various GPU resources. */
        std::unique_ptr<GpuResourceManager> pResourceManager;

        /** Used to compile shaders. */
        std::unique_ptr<ShaderManager> pShaderManager;

        /** Used to store various graphics and compute PSOs. */
        std::unique_ptr<PsoManager> pPsoManager;

        /** Stores frame-specific GPU resources. */
        std::unique_ptr<FrameResourcesManager> pFrameResourcesManager;

        /** Stores all shader resources with CPU read/write access. */
        std::unique_ptr<ShaderCpuReadWriteResourceManager> pShaderCpuReadWriteResourceManager;

        /**
         * Shader parameters.
         * Must be used with mutex.
         */
        std::pair<std::recursive_mutex, std::unique_ptr<ShaderConfiguration>> mtxShaderConfiguration;

        /**
         * Render setting objects that configure the renderer.
         * Must be used with mutex.
         */
        std::pair<std::recursive_mutex, std::shared_ptr<RenderSettings>> mtxRenderSettings;

        /** Do not delete (free) this pointer. Game manager object that owns this renderer. */
        GameManager* pGameManager = nullptr;

        /** Number of buffers in swap chain. */
        static constexpr unsigned int iSwapChainBufferCount = 3;
    };

    /** Describes a group of shader macros. */
    class ShaderConfiguration {
    public:
        ShaderConfiguration() = delete;

        /**
         * Saves render to use.
         *
         * @param pRenderer Renderer to use.
         */
        ShaderConfiguration(Renderer* pRenderer) { this->pRenderer = pRenderer; }

        /**
         * Updates the current shader configuration (settings) for the current renderer based on the current
         * values from this struct.
         *
         * @remark Flushes the command queue and recreates PSOs' internal resources so that they
         * will use new shader configuration.
         */
        void updateShaderConfiguration() { pRenderer->updateShaderConfiguration(); }

        /** Vertex shader macros. */
        std::set<ShaderMacro> currentVertexShaderConfiguration;

        /** Pixel shader macros. */
        std::set<ShaderMacro> currentPixelShaderConfiguration;

    private:
        /** Used renderer. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
