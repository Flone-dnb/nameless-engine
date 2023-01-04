#pragma once

// Standard.
#include <variant>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>

// Custom.
#include "misc/Error.h"
#include "materials/ShaderManager.h"
#include "render/RenderSettings.h"
#include "misc/ProjectPaths.h"

namespace ne {
    class Game;
    class Window;
    class PsoManager;
    class ShaderConfiguration;
    class RenderSettings;

    /**
     * Defines a base class for renderers to implement.
     */
    class Renderer {
    public:
        Renderer() = delete;
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        virtual ~Renderer() = default;

        /**
         * Creates a new platform-specific renderer.
         *
         * @param pGame Game object that will own this renderer.
         *
         * @return Created renderer.
         */
        static std::unique_ptr<Renderer> create(Game* pGame);

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @return Error if can't find any GPU that supports this renderer,
         * vector with GPU names if successful.
         */
        virtual std::variant<std::vector<std::string>, Error> getSupportedGpuNames() const = 0;

        /**
         * Returns a list of supported render resolution (pairs of width and height).
         *
         * @return Error if something went wrong, otherwise render mode.
         */
        virtual std::variant<std::vector<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRenderResolutions() const = 0;

        /**
         * Returns render settings that can be configured.
         *
         * @return Do not delete (free) returned pointer. Non-owning pointer to render settings.
         */
        std::pair<std::recursive_mutex, std::shared_ptr<RenderSettings>>* getRenderSettings();

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::string getCurrentlyUsedGpuName() const = 0;

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const = 0;

        /**
         * Returns used video memory size (VRAM) in megabytes.
         *
         * @return Used video memory size in megabytes.
         */
        virtual size_t getUsedVideoMemoryInMb() const = 0;

        /**
         * Blocks the current thread until the GPU finishes executing all queued commands up to this point.
         *
         * @remark Typically used with @ref getRenderResourcesMutex.
         */
        virtual void flushCommandQueue() = 0;

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
         * Game object that owns this renderer.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Game object.
         */
        Game* getGame() const;

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
         * Returns mutex used when reading or writing to render resources.
         * Usually used with @ref flushCommandQueue.
         *
         * @return Mutex.
         */
        std::recursive_mutex* getRenderResourcesMutex();

    protected:
        // Only window should be able to request new frames to be drawn.
        friend class Window;

        // Can update shader configuration.
        friend class ShaderConfiguration;

        // Settings will modify renderer's state.
        friend class RenderSettings;

        /**
         * Constructor.
         *
         * @param pGame Game object that owns this renderer.
         */
        Renderer(Game* pGame);

        /**
         * Returns the amount of buffers the swap chain has.
         *
         * @return The amount of buffers the swap chain has.
         */
        static consteval unsigned int getSwapChainBufferCount() { return iSwapChainBufferCount; }

        /** Update internal resources for the next frame. */
        virtual void updateResourcesForNextFrame() = 0;

        /** Draw new frame. */
        virtual void drawNextFrame() = 0;

        /**
         * Recreates all render buffers to match current settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateRenderBuffers() = 0;

        /**
         * (Re)creates depth/stencil buffer.
         *
         * @remark Make sure that the old depth/stencil buffer (if was) is not used by the GPU.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> createDepthStencilBuffer() = 0;

        /**
         * Tells whether the renderer is initialized or not
         * (whether it's safe to use renderer functionality or not).
         *
         * @return Whether the renderer is initialized or not.
         */
        virtual bool isInitialized() const = 0;

        /** Initializes render settings, must be called in the beginning of the constructor. */
        void initializeRenderSettings();

    private:
        /**
         * Updates the current shader configuration (settings) based on the current value
         * from @ref getShaderConfiguration.
         *
         * @remark Flushes the command queue and recreates PSOs' internal resources so that they
         * will use new shader configuration.
         */
        void updateShaderConfiguration();

        /** Lock when reading or writing to render resources. Usually used with @ref flushCommandQueue. */
        std::recursive_mutex mtxRwRenderResources;

        /** Used to compile shaders. */
        std::unique_ptr<ShaderManager> pShaderManager;

        /** Used to store various graphics and compute PSOs. */
        std::unique_ptr<PsoManager> pPsoManager;

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

        /** Do not delete (free) this pointer. Game object that owns this renderer. */
        Game* pGame = nullptr;

        /** Number of buffers in swap chain. */
        static constexpr unsigned int iSwapChainBufferCount = 2;

        /** Name of the category used for logging. */
        inline static const char* sRendererLogCategory = "Renderer";
    };

    /** Describes shader parameters. */
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

        /** Vertex shader parameters. */
        std::set<ShaderParameter> currentVertexShaderConfiguration;

        /** Pixel shader parameters. */
        std::set<ShaderParameter> currentPixelShaderConfiguration;

    private:
        /** Used renderer. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
