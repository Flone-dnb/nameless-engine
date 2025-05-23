﻿#pragma once

// Standard.
#include <memory>
#include <unordered_set>

// Custom.
#include "render/general/pipeline/Pipeline.h"
#include "render/general/resource/frame/FrameResourceManager.h"
#include "shader/general/resource/ShaderArrayIndexManager.h"
#include "misc/StdHashes.hpp"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    class VulkanRenderer;
    class GlslShader;

    /** VkPipeline object wrapper. */
    class VulkanPipeline : public Pipeline {
    public:
        /** Stores internal resources. */
        struct InternalResources {
            InternalResources() = default;

            /** Created pipeline layout. */
            VkPipelineLayout pPipelineLayout = nullptr;

            /** Created pipeline. */
            VkPipeline pPipeline = nullptr;

            /** Created descriptor set layout. */
            VkDescriptorSetLayout pDescriptorSetLayout = nullptr;

            /** Created descriptor pool. */
            VkDescriptorPool pDescriptorPool = nullptr;

            /**
             * Created descriptor set per each frame resource.
             *
             * @remark Initial motivation for 1 set per frame resource is for shader resources
             * with CPU write access. For example we have 1 `uniform` buffer with frame constant data
             * per frame resource, so if we have 3 frame resources we have 3 buffers with frame data
             * and each descriptor set will point to a different buffer. Just like with other CPU write
             * shader resources when we want to update such a resource we mark it as "needs update"
             * but we don't update it immediately because it may be currently used by the GPU
             * and so in order to not wait for the GPU and not halt the rendering we update
             * all shader resources with CPU access marked as "needs update" in the beginning of the
             * `draw` function and we only update shader resources of the current (next) frame resource
             * (that is no longer being used). This way we don't need to halt the rendering when we want
             * to update some shader resource with CPU write access, thus we have 1 descriptor set per
             * frame resource.
             */
            std::array<VkDescriptorSet, FrameResourceManager::getFrameResourceCount()> vDescriptorSets;

            /**
             * Map of pairs "shader resource name" (from GLSL code) - "layout binding index".
             *
             * @remark Binding index in the map reference descriptor sets from @ref vDescriptorSets.
             *
             * @remark Generally used to bind/update data of some GLSL resource to a specific
             * descriptor in a descriptor set.
             */
            std::unordered_map<std::string, uint32_t, StdStringHash, std::equal_to<>> resourceBindings;

            /**
             * Stores pairs of "shader resource name" - "shader array index manager".
             *
             * @warning Do not clear this map (and thus destroy index managers) when releasing
             * internal resources to restore them later because there might be shader
             * resources that reference indices of some index manager and if the index manager
             * will be destroyed it will show an error that there are some active (used) indices
             * that reference the index manager.
             *
             * @remark Refers to shader arrays from GLSL.
             *
             * @remark Since pipeline does not care which resources are shader arrays and
             * which are not, shader resources that reference shader arrays must create new
             * entries in this map when no index manager for a requested shader resource is found.
             * All descriptors for a shader array are allocated per pipeline so shader resources
             * that reference a shader array in some pipeline can have just one index manager per
             * shader array.
             */
            std::unordered_map<std::string, std::unique_ptr<ShaderArrayIndexManager>>
                shaderArrayIndexManagers;

            /** Whether resources were created or not. */
            bool bIsReadyForUsage = false;
        };

        VulkanPipeline() = delete;
        VulkanPipeline(const VulkanPipeline&) = delete;
        VulkanPipeline& operator=(const VulkanPipeline&) = delete;

        virtual ~VulkanPipeline() override;

        /**
         * Assigns vertex and pixel shaders to create a graphics pipeline (for usual rendering).
         *
         * @param pRenderer              Used renderer.
         * @param pPipelineManager       Pipeline manager that owns this pipeline.
         * @param pPipelineConfiguration Settings that determine pipeline usage and usage details.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate pipeline,
         * otherwise created pipeline.
         */
        static std::variant<std::shared_ptr<VulkanPipeline>, Error> createGraphicsPipeline(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration);

        /**
         * Assigns compute shader to create a compute pipeline.
         *
         * @param pRenderer          Used renderer.
         * @param pPipelineManager   Pipeline manager that owns this pipeline.
         * @param sComputeShaderName Name of the compiled compute shader (see ShaderManager::compileShaders).
         *
         * @return Error if shader was not found in ShaderManager or if failed to generate pipeline,
         * otherwise created pipeline.
         */
        static std::variant<std::shared_ptr<VulkanPipeline>, Error> createComputePipeline(
            Renderer* pRenderer, PipelineManager* pPipelineManager, const std::string& sComputeShaderName);

        /**
         * Binds the specified GPU resources (buffers, not images) to this pipeline if it uses the specified
         * shader resource.
         *
         * @param vResources          Resources to bind.
         * @param sShaderResourceName Name of the shader resource (name from shader code) to bind to.
         * @param descriptorType      Type of the descriptor to bind.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindBuffersIfUsed(
            const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResources,
            const std::string& sShaderResourceName,
            VkDescriptorType descriptorType);

        /**
         * Binds the specified GPU resource (image, not buffer) to this pipeline if it uses the specified
         * shader resource.
         *
         * @param pImageResourceToBind Image to bind.
         * @param sShaderResourceName  Name of the shader resource (name from shader code) to bind to.
         * @param descriptorType       Type of the descriptor to bind.
         * @param imageLayout          Layout of the image.
         * @param pSampler             Sampler to use for the image.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindImageIfUsed(
            GpuResource* pImageResourceToBind,
            std::string_view sShaderResourceName,
            VkDescriptorType descriptorType,
            VkImageLayout imageLayout,
            VkSampler pSampler);

        /**
         * Returns internal resources that this pipeline uses.
         *
         * @return Internal resources.
         */
        inline std::pair<std::recursive_mutex, InternalResources>* getInternalResources() {
            return &mtxInternalResources;
        }

    protected:
        /**
         * Releases all internal resources from this graphics pipeline and then recreates
         * them to reference new resources/parameters from the renderer.
         *
         * @warning Expects that the GPU is not processing any frames and the rendering is paused
         * (new frames are not submitted) while this function is being called.
         *
         * @remark This function is used when all graphics pipelines reference old render
         * resources/parameters to reference the new (changed) render resources/parameters. The typical
         * workflow goes like this: pause the rendering, change renderer's resource/parameter that all
         * graphics pipelines reference (like render target type (MSAA or not) or MSAA sample count), then
         * call this function (all graphics pipelines will now query up-to-date rendering
         * resources/parameters) and then you can continue rendering.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> recreateInternalResources() override;

    private:
        /**
         * Constructs uninitialized pipeline.
         *
         * @param pRenderer              Used renderer.
         * @param pPipelineManager       Pipeline manager that owns this pipeline.
         * @param pPipelineConfiguration Settings and usage details.
         */
        VulkanPipeline(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration);

        /**
         * (Re)generates Vulkan pipeline and pipeline layout.
         *
         * @warning If a shader of some type was already added it will be replaced with the new one.
         * When shader is replaced the old shader gets freed from the memory and
         * a new pipeline is immediately generated. Make sure the GPU is not using old shader/pipeline.
         *
         * @return Error if failed to generate pipeline.
         */
        [[nodiscard]] std::optional<Error> generateGraphicsPipeline();

        /**
         * (Re)generates Vulkan compute pipeline and pipeline layout for the specified shader.
         *
         * @warning If a shader of some type was already added it will be replaced with the new one.
         * When shader is replaced the old shader gets freed from the memory and
         * a new pipeline is immediately generated. Make sure the GPU is not using old shader/pipeline.
         *
         * @param sComputeShaderName Name of the compiled compute shader (see ShaderManager::compileShaders).
         *
         * @return Error if failed to generate pipeline.
         */
        [[nodiscard]] std::optional<Error>
        generateComputePipelineForShader(const std::string& sComputeShaderName);

        /**
         * Fully initializes @ref mtxInternalResources by creating a graphics pipeline for the specified
         * shaders.
         *
         * @param pVulkanRenderer   Vulkan renderer.
         * @param pVertexShader     Vertex shader to use in the pipeline.
         * @param pFragmentShader   Fragment shader to use in the pipeline. Specify `nullptr` if you want
         * to create depth only pipeline (used for z-prepass).
         * @param bUsePixelBlending Whether the pipeline should use blending or not (for transparency).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createGraphicsPipeline(
            VulkanRenderer* pVulkanRenderer,
            GlslShader* pVertexShader,
            GlslShader* pFragmentShader,
            bool bUsePixelBlending);

        /**
         * Fully initializes @ref mtxInternalResources by creating a compute pipeline for the specified
         * shader.
         *
         * @param pVulkanRenderer Vulkan renderer.
         * @param pComputeShader  Compute shader to use in the pipeline.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        createComputePipeline(VulkanRenderer* pVulkanRenderer, GlslShader* pComputeShader);

        /**
         * Binds descriptors that reference "frameData" shader resource to frame resources' uniform buffers.
         *
         * @remark Expects that descriptor sets in @ref mtxInternalResources are initialized.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindFrameDataDescriptors();

        /**
         * Releases all internal resources from this graphics pipeline.
         *
         * @warning Expects that the GPU is not processing any frames and the rendering is paused
         * (new frames are not submitted) while this function is being called.
         *
         * @remark This function is used in @ref recreateInternalResources and in the destructor
         * to delete/free used resources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> releaseInternalResources();

        /**
         * Initializes internal push constants manager and returns push constants description.
         *
         * @param pushConstantUintFieldOffsets Stores pairs of "name of field defined in GLSL push constants"
         * (all with `uint` type) and "offset from the beginning of the push constants struct
         * (in `uint`s not bytes)".
         * @param resourceBindings             Map of pairs "resource name" (from GLSL code) - "layout
         * binding index".
         *
         * @return Error if something went wrong, otherwise push constants range.
         */
        std::variant<VkPushConstantRange, Error> definePushConstants(
            const std::unordered_map<std::string, size_t>& pushConstantUintFieldOffsets,
            const std::unordered_map<std::string, uint32_t, StdStringHash, std::equal_to<>>&
                resourceBindings);

        /**
         * Internal resources.
         * Must be used with mutex when changing.
         */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;
    };
} // namespace ne
