#pragma once

// Standard.
#include <variant>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>

// Custom.
#include "shader/general/resources/cpuwrite/ShaderCpuWriteResourceManager.h"
#include "shader/general/resources/texture/ShaderTextureResourceManager.h"
#include "misc/Error.h"
#include "shader/ShaderManager.h"
#include "render/general/resources/GpuResourceManager.h"
#include "render/general/resources/frame/FrameResourcesManager.h"
#include "render/RenderSettings.h"
#include "game/camera/CameraProperties.h"
#include "misc/shapes/AABB.h"
#include "shader/general/resources/LightingShaderResourceManager.h"
#include "material/Material.h"

namespace ne {
    class GameManager;
    class Window;
    class PipelineManager;
    class ShaderConfiguration;
    class RenderSettings;
    class EnvironmentNode;
    class MeshNode;
    class Material;
    class Pipeline;

    /** Defines a base class for renderers to implement. */
    class Renderer {
        // Only window should be able to request new frames to be drawn.
        friend class Window;

        // Can update shader configuration.
        friend class ShaderConfiguration;

        // Settings will modify renderer's state.
        friend class RenderSettings;

        // This node sets itself to the renderer when spawned so that its parameters will be used
        // in the rendering (also removes itself when despawned).
        friend class EnvironmentNode;

        // Notifies when active camera changes.
        friend class CameraManager;

    public:
        Renderer() = delete;
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        virtual ~Renderer() = default;

        /**
         * Returns the minimum value for depth.
         *
         * @return Minimum depth value.
         */
        static constexpr float getMinDepth() { return minDepth; }

        /**
         * Returns the maximum value for depth.
         *
         * @return Maximum depth value.
         */
        static constexpr float getMaxDepth() { return maxDepth; }

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
         * Returns the number of frames that the renderer produced in the last second.
         *
         * @return Zero if not calculated yet (wait at least 1 second), otherwise FPS.
         */
        size_t getFramesPerSecond() const;

        /**
         * Returns the total number of draw calls made last frame.
         *
         * @return Draw call count.
         */
        size_t getLastFrameDrawCallCount() const;

        /**
         * Returns time in milliseconds that was spent last frame waiting for GPU to catch up.
         * If returned value is constantly bigger than zero then this might mean that you are GPU bound,
         * if constantly zero (0.0F) then this might mean that you are CPU bound.
         *
         * @return Time in milliseconds.
         */
        float getTimeSpentLastFrameWaitingForGpu() const;

        /**
         * Returns time in milliseconds that was spent last frame doing frustum culling.
         *
         * @return Time in milliseconds.
         */
        float getTimeSpentLastFrameOnFrustumCulling() const;

        /**
         * Returns the total number of objects (draw entities) that were discarded from submitting to
         * rendering during the last frame.
         *
         * @return Object count.
         */
        size_t getLastFrameCulledObjectCount() const;

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @remark Note that returned array might differ depending on the used renderer.
         *
         * @return Empty array if no GPU supports used renderer,
         * otherwise array with GPU names that can be used for this renderer.
         */
        virtual std::vector<std::string> getSupportedGpuNames() const = 0;

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
         * @remark If integrated GPU is used this function might return shared video memory
         * (includes both dedicated VRAM and system RAM).
         *
         * @return Total video memory size in megabytes.
         */
        size_t getTotalVideoMemoryInMb() const;

        /**
         * Returns the amount of video memory (VRAM) that is currently being used by the renderer.
         *
         * @remark Does not return global (system-wide) used VRAM size, only VRAM used by the renderer
         * (i.e. only VRAM used by this application).
         *
         * @return Size of the video memory used by the renderer in megabytes.
         */
        size_t getUsedVideoMemoryInMb() const;

        /**
         * Blocks the current thread until the GPU finishes executing all queued graphics commands up to this
         * point.
         *
         * @remark Typically used while @ref getRenderResourcesMutex is locked.
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
         * Returns size of the render target (size of the underlying render image).
         *
         * @return Render image size in pixels (width and height).
         */
        virtual std::pair<unsigned int, unsigned int> getRenderTargetSize() const = 0;

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
         * Returns pipeline manager used to store graphics and compute pipelines.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Pipeline manager.
         */
        PipelineManager* getPipelineManager() const;

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
         * Returns manager of shader resources with CPU write access.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Manager.
         */
        ShaderCpuWriteResourceManager* getShaderCpuWriteResourceManager() const;

        /**
         * Returns manager of shader resources that reference textures.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Manager.
         */
        ShaderTextureResourceManager* getShaderTextureResourceManager() const;

        /**
         * Returns manager that controls GPU resources of lighting shader resources.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Manager.
         */
        LightingShaderResourceManager* getLightingShaderResourceManager() const;

        /**
         * Returns mutex that is used when reading or writing to GPU resources that may be used
         * by the GPU.
         *
         * @remark This mutex is generally locked when the renderer is submitting a new frame.
         *
         * @remark Usually after locking this mutex you would use @ref waitForGpuToFinishWorkUpToThisPoint
         * before actually starting to write/modify GPU resources.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Mutex.
         */
        std::recursive_mutex* getRenderResourcesMutex();

        /**
         * Returns pointer to the texture resource that represents renderer's depth texture
         * without multisampling (resolved resource).
         *
         * @warning If MSAA is enabled this function will return one resource (pointer to a separate
         * depth resolved resource), if it's disabled it will return the other resource (pointer to depth
         * texture). So it may be a good idea to query this pointer every time you need it instead of saving
         * it and reusing it because every frame this pointer may change (due to other reasons such as
         * render target resize and etc).
         *
         * @warning Please note that it's only safe to call this function inside of the `drawNextFrame`
         * function (because returned pointer will not be changed during this function), if you need to use
         * depth texture in your game code create an issue and I will add a GameInstance callback that will be
         * called each frame inside of the `drawNextFrame` so you can use this function.
         *
         * @return Pointer to depth texture.
         */
        virtual GpuResource* getDepthTextureNoMultisampling() = 0;

    protected:
        /** Groups information about meshes in active camera's frustum. */
        struct MeshesInFrustum {
            /** Groups information about index buffers of some mesh that use the same material. */
            struct MeshInFrustumInfo {
                /** Mesh node. */
                MeshNode* pMeshNode = nullptr;

                /** Index buffers of @ref pMeshNode that use the same material. */
                std::vector<MeshIndexBufferInfo> vIndexBuffers;
            };

            /** Groups information about meshes that use the same material. */
            struct MaterialInFrustumInfo {
                /** Material. */
                Material* pMaterial = nullptr;

                /** Meshes that use @ref pMaterial. */
                std::vector<MeshInFrustumInfo> vMeshes;
            };

            /** Stores information about materials that use a specific pipeline. */
            struct PipelineInFrustumInfo {
                /** Pipeline. */
                Pipeline* pPipeline = nullptr;

                /** Materials that use @ref pPipeline. */
                std::vector<MaterialInFrustumInfo> vMaterials;
            };

            /** Meshes in frustum that use opaque pipeline. */
            std::vector<PipelineInFrustumInfo> vOpaquePipelines;

            /** Meshes in frustum that use pipeline with pixel blending. */
            std::vector<PipelineInFrustumInfo> vTransparentPipelines;
        };

        /**
         * Returns the number of swap chain buffers/images that we prefer to use.
         *
         * @remark Frame resources expect that the number of swap chain images is equal to the number
         * of frame resources because frame resources store synchronization objects such as
         * fences and semaphores that expect one swap chain image per frame resource. If your renderer
         * wants to use other number swap chain images you would might to implement a custom logic
         * that will make sure everything is synchronized. For example, if you want to have less swap
         * chain images than there is frame resources then you will need to store something like a
         * pair of "swap chain image" - "frame resource" and each frame check if some swap chain image is
         * used by some frame resource or not and wait if it's being used.
         *
         * @return The recommended number of swap chain buffers/images that will work without any additional
         * logic.
         */
        static consteval unsigned int getRecommendedSwapChainBufferCount() {
            return iRecommendedSwapChainBufferCount;
        }

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
         * Takes the current frame resource and updates frame data constant buffer that it stores (by copying
         * new (up to date) constants to it).
         *
         * @remark Expected to be called by derived renderers only when they know that frame resources
         * are not being used by the GPU.
         *
         * @param pCurrentFrameResource Current frame resource.
         * @param pCameraProperties     Camera properties to use.
         */
        void
        updateFrameConstantsBuffer(FrameResource* pCurrentFrameResource, CameraProperties* pCameraProperties);

        /**
         * Calculates some frame-related statistics.
         *
         * @remark Must be called by derived renderers as the last function in `drawNextFrame`
         * to notify the renderer to calculate some frame-related statistics.
         */
        void calculateFrameStatistics();

        /**
         * Sets `nullptr` to resource manager's unique ptr to force destroy it (if exists).
         *
         * @warning Avoid using this function. Only use it if you need a special destruction order
         * in your renderer.
         */
        void resetGpuResourceManager();

        /**
         * Sets `nullptr` to pipeline manager's unique ptr to force destroy it (if exists).
         *
         * @warning Avoid using this function. Only use it if you need a special destruction order
         * in your renderer.
         */
        void resetPipelineManager();

        /**
         * Sets `nullptr` to frame resources manager's unique ptr to force destroy it (if exists).
         *
         * @warning Avoid using this function. Only use it if you need a special destruction order
         * in your renderer.
         */
        void resetFrameResourcesManager();

        /**
         * Sets `nullptr` to lighting shader resource manager's unique ptr to force destroy it (if exists).
         *
         * @warning Avoid using this function. Only use it if you need a special destruction order
         * in your renderer.
         */
        void resetLightingShaderResourceManager();

        /**
         * Returns the maximum anti-aliasing quality that can be used on the picked
         * GPU (@ref getCurrentlyUsedGpuName).
         *
         * @remark Note that the maximum supported AA quality can differ depending on the used GPU/renderer.
         *
         * @return Error if something went wrong,
         * otherwise `DISABLED` if AA is not supported or the maximum supported AA quality.
         */
        virtual std::variant<MsaaState, Error> getMaxSupportedAntialiasingQuality() const = 0;

        /**
         * Called when the framebuffer size was changed.
         *
         * @param iWidth  New width of the framebuffer (in pixels).
         * @param iHeight New height of the framebuffer (in pixels).
         */
        virtual void onFramebufferSizeChanged(int iWidth, int iHeight) {}

        /** Submits a new frame to the GPU. */
        virtual void drawNextFrame() = 0;

        /**
         * Called after some render setting is changed to recreate internal resources to match the current
         * settings.
         *
         * @param bShadowMapSizeChanged `true` if shadow map size was changed, `false` otherwise.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> onRenderSettingsChanged(bool bShadowMapSizeChanged = false);

        /**
         * Called from @ref onRenderSettingsChanged after some render setting is changed to recreate internal
         * resources to match the current settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> onRenderSettingsChangedDerived() = 0;

        /**
         * Blocks the current thread until the GPU is finished using the specified frame resource.
         *
         * @remark Generally the current frame resource will be passed and so the current frame resource
         * mutex will be locked at the time of calling and until the function is not finished it will not
         * be unlocked.
         *
         * @param pFrameResource Frame resource to wait for.
         */
        virtual void waitForGpuToFinishUsingFrameResource(FrameResource* pFrameResource) = 0;

        /**
         * Tells whether the renderer is initialized or not.
         *
         * Initialized renderer means that the hardware supports it and it's safe to use renderer
         * functionality such as @ref onRenderSettingsChanged.
         *
         * @return Whether the renderer is initialized or not.
         */
        virtual bool isInitialized() const = 0;

        /**
         * Initializes some essential parts of the renderer (such as RenderSettings).
         *
         * @warning Must be called by derived classes in the beginning of their initialization.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> initializeRenderer();

        /**
         * Initializes various resource managers.
         *
         * @warning Must be called by derived classes after base initialization
         * (for ex. in DirectX after device and video adapter were created).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> initializeResourceManagers();

        /**
         * Called by derived class after they have created essential API objects
         * (D3D device / Vulkan physical device) so that RenderSettings can query maximum supported
         * settings and clamp the current values (if needed).
         *
         * @warning Must be called by derived classes after they have created essential API objects
         * (D3D device / Vulkan physical device).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> clampSettingsToMaxSupported();

        /**
         * Updates internal resources (frame constants, shader resources, camera's aspect ratio and etc.)
         * for the next frame.
         *
         * @remark Waits before frame resource for the new frame will not be used by the GPU so it's
         * safe to work with frame resource after calling this function.
         *
         * @param iRenderTargetWidth  Width (in pixels) of the image that will be used for the next frame.
         * @param iRenderTargetHeight Height (in pixels) of the image that will be used for the next frame.
         * @param pCameraProperties   Camera properties to use.
         */
        void updateResourcesForNextFrame(
            unsigned int iRenderTargetWidth,
            unsigned int iRenderTargetHeight,
            CameraProperties* pCameraProperties);

        /**
         * Notifies @ref pLightingShaderResourceManager to recalculate grid of frustums
         * for light culling process.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> recalculateLightTileFrustums();

        /**
         * Iterates over all meshes and returns only meshes inside of the camera's frustum
         * (and their pipelines).
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @remark This function is expected to be called only once per frame.
         *
         * @param pActiveCameraProperties Properties of the currently active camera.
         * @param pGraphicsPipelines      Graphics pipelines.
         *
         * @return Pipelines, material and meshes inside camera's frustum.
         */
        MeshesInFrustum* getMeshesInFrustum(
            CameraProperties* pActiveCameraProperties,
            PipelineManager::GraphicsPipelineRegistry* pGraphicsPipelines);

        /**
         * Returns frame constants.
         *
         * @return Frame constants.
         */
        inline std::pair<std::mutex, FrameConstants>* getFrameConstants() { return &mtxFrameConstants; }

        /**
         * Returns counter for draw calls.
         *
         * @remark Must be used by derived classes to increment draw call counter.
         *
         * @remark Automatically resets in @ref calculateFrameStatistics.
         *
         * @return Draw call counter.
         */
        inline size_t* getDrawCallCounter() { return &iLastFrameDrawCallCount; }

    private:
        /** Groups variables used to calculate frame-related statistics. */
        struct FrameStatsData {
            /**
             * Time when the renderer has finished initializing or when
             * @ref iFramesPerSecond was updated.
             */
            std::chrono::steady_clock::time_point timeAtLastFpsUpdate;

            /**
             * The number of times the renderer presented a new image since the last time we updated
             * @ref iFramesPerSecond.
             */
            size_t iPresentCountSinceFpsUpdate = 0;

            /** The number of frames that the renderer produced in the last second. */
            size_t iFramesPerSecond = 0;

            /** Time when last frame was started to be processed. */
            std::chrono::steady_clock::time_point frameStartTime;

            /** Not empty if FPS limit is set, defines time in nanoseconds that one frame should take. */
            std::optional<double> timeToRenderFrameInNs = {};

            /**
             * Time (in milliseconds) that was spent last frame waiting for the GPU to finish using
             * the new frame resource.
             *
             * @remark If constantly bigger than zero then this might mean that you are GPU bound,
             * if constantly zero then this might mean that you are CPU bound.
             */
            float timeSpentLastFrameWaitingForGpuInMs = 0.0F;

            /**
             * Total time that was spent last frame doing frustum culling.
             *
             * @remark Updated only after a frame is submitted.
             */
            float timeSpentLastFrameOnFrustumCullingInMs = 0.0F;

            /**
             * Total number of objects (draw entries) discarded from submitting due to frustum culling.
             *
             * @remark Updated only after a frame is submitted.
             */
            size_t iLastFrameCulledObjectCount = 0;

            /** The total number of draw calls made during the last frame. */
            size_t iLastFrameDrawCallCount = 0;
        };

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
         * @param vBlacklistedGpuNames Names of GPUs that should not be used, generally this means
         * that these GPUs were previously used to create the renderer but something went wrong.
         *
         * @return Created renderer if successful, otherwise multiple values in a pair: error and a
         * name of the GPU that the renderer tried to use (can be empty if failed before picking a GPU
         * or if all supported GPUs are blacklisted).
         */
        static std::variant<std::unique_ptr<Renderer>, std::pair<Error, std::string>> createRenderer(
            RendererType type,
            GameManager* pGameManager,
            const std::vector<std::string>& vBlacklistedGpuNames);

#if defined(WIN32)
        /**
         * Uses Windows' WaitableTimer to wait for the specified number of nanoseconds.
         *
         * @param iNanoseconds Nanoseconds to wait for.
         */
        static void nanosleep(long long iNanoseconds);
#endif

        /**
         * Tests frustum culling on the specified AABB.
         *
         * @warning Expects that camera's frustum is updated (contains up to date data).
         *
         * @param pActiveCameraProperties Camera properties of the active camera.
         * @param aabb                    AABB in model space.
         * @param worldMatrix             Matrix that transforms AABB to world space.
         *
         * @return `true` if this AABB is outside of camera's frustum and it the object should not be
         * rendered, `false` otherwise.
         */
        inline bool isAabbOutsideCameraFrustum(
            CameraProperties* pActiveCameraProperties, const AABB& aabb, const glm::mat4x4& worldMatrix) {

            // (camera's frustum should be updated at this point)
            const auto bIsVisible =
                pActiveCameraProperties->getCameraFrustum()->isAabbInFrustum(aabb, worldMatrix);

            // Increment culled object count.
            iLastFrameCulledObjectCount += static_cast<size_t>(!bIsVisible);

            return !bIsVisible;
        }

        /** Called by camera manager after active camera was changed. */
        void onActiveCameraChanged();

        /** Looks for FPS limit setting in RenderSettings and update renderer's state. */
        void updateFpsLimitSetting();

        /**
         * Updates the current shader configuration (settings) based on the current value
         * from @ref getShaderConfiguration.
         *
         * @remark Flushes the command queue and recreates pipelines' internal resources so that they
         * will use new shader configuration.
         */
        void updateShaderConfiguration();

        /** Initializes @ref frameStats to be used. */
        void setupFrameStats();

        /**
         * Initializes @ref mtxRenderSettings.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> initializeRenderSettings();

        /**
         * Lock when reading or writing to render resources. Usually used with @ref
         * waitForGpuToFinishWorkUpToThisPoint.
         */
        std::recursive_mutex mtxRwRenderResources;

        /** Used to create various GPU resources. */
        std::unique_ptr<GpuResourceManager> pResourceManager;

        /** Used to compile shaders. */
        std::unique_ptr<ShaderManager> pShaderManager;

        /** Used to store various graphics and compute pipelines. */
        std::unique_ptr<PipelineManager> pPipelineManager;

        /** Stores frame-specific GPU resources. */
        std::unique_ptr<FrameResourcesManager> pFrameResourcesManager;

        /** Stores all shader resources with CPU write access. */
        std::unique_ptr<ShaderCpuWriteResourceManager> pShaderCpuWriteResourceManager;

        /** Stores all shader resources that reference textures. */
        std::unique_ptr<ShaderTextureResourceManager> pShaderTextureResourceManager;

        /** Stores data of all spawned light sources that is used in shaders. */
        std::unique_ptr<LightingShaderResourceManager> pLightingShaderResourceManager;

        /**
         * A bunch of shader macros that match renderer's configuration (render settings).
         * Must be used with mutex.
         */
        std::pair<std::recursive_mutex, std::unique_ptr<ShaderConfiguration>> mtxShaderConfiguration;

        /**
         * Render setting objects that configure the renderer.
         * Must be used with mutex.
         */
        std::pair<std::recursive_mutex, std::shared_ptr<RenderSettings>> mtxRenderSettings;

        /** Meshes that were in camera's frustum last frame. */
        MeshesInFrustum meshesInFrustumLastFrame;

        /**
         * Time in milliseconds spent last frame on frustum culling.
         *
         * @remark Accumulated in @ref isAabbOutsideCameraFrustum.
         */
        float accumulatedTimeSpentLastFrameOnFrustumCullingInMs = 0.0F;

        /**
         * Total number of objects (draw entries) discarded from submitting due to frustum culling.
         *
         * @remark Incremented in @ref isAabbOutsideCameraFrustum.
         */
        size_t iLastFrameCulledObjectCount = 0;

        /** Stores the total number of draw calls made last frame. */
        size_t iLastFrameDrawCallCount = 0;

        /** Spawned environment node which parameters are used in the rendering. */
        std::pair<std::mutex, EnvironmentNode*> mtxSpawnedEnvironmentNode;

        /** Up to date frame-global constant data. */
        std::pair<std::mutex, FrameConstants> mtxFrameConstants;

        /** Frame-related statistics. */
        FrameStatsData frameStats;

        /** Do not delete (free) this pointer. Game manager object that owns this renderer. */
        GameManager* pGameManager = nullptr;

        /** The number of buffers/images in swap chain that we prefer to use. */
        static constexpr unsigned int iRecommendedSwapChainBufferCount = 2;

        /** Minimum value for depth. */
        static constexpr float minDepth = 0.0F;

        /** Maximum value for depth. */
        static constexpr float maxDepth = 1.0F;
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
         * @remark Flushes the command queue and recreates pipelines' internal resources so that they
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
