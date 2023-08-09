#pragma once

// Standard.
#include <memory>
#include <unordered_set>

// Custom.
#include "render/general/pipeline/Pipeline.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "render/vulkan/pipeline/VulkanPushConstantsManager.hpp"

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
            /** Created pipeline layout. */
            VkPipelineLayout pPipelineLayout = nullptr;

            /** Created pipeline. */
            VkPipeline pPipeline = nullptr;

            /** Created descriptor set layout. */
            VkDescriptorSetLayout pDescriptorSetLayout = nullptr;

            /** Created descriptor pool. */
            VkDescriptorPool pDescriptorPool = nullptr;

            /** Created descriptor set per each frame resource. */
            std::array<VkDescriptorSet, FrameResourcesManager::getFrameResourcesCount()> vDescriptorSets;

            /**
             * Map of pairs "resource name" (from GLSL code) - "layout binding index".
             *
             * @remark Binding index in the map reference descriptor sets from @ref vDescriptorSets.
             *
             * @remark Generally used to bind/update data of some GLSL resource to a specific
             * descriptor in a descriptor set.
             */
            std::unordered_map<std::string, uint32_t> resourceBindings;

            /**
             * Not empty if push constants are used.
             * Stores names of fields defined in GLSL as push constants (all with `uint` type).
             *
             * @remark If a non `uint` fields is found an error is returned instead.
             */
            std::optional<std::unordered_set<std::string>> pushConstantUintFieldNames;

            /**
             * Smallest `binding` value of a shader resource (from GLSL code) that is referenced
             * in push constants.
             *
             * @remark Used by shader resources (from C++ code) to calculate the correct index into
             * @ref pPushConstantsManager to update push constants.
             *
             * @remark Not empty if @ref pPushConstantsManager is valid.
             */
            std::optional<unsigned int> smallestBindingIndexReferencedByPushConstants;

            /** Stores push constants. `nullptr` if push constants are not used. */
            std::unique_ptr<VulkanPushConstantsManager> pPushConstantsManager;

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
         * @param sVertexShaderName      Name of the compiled vertex shader (see
         * ShaderManager::compileShaders).
         * @param sFragmentShaderName    Name of the compiled fragment shader (see
         * ShaderManager::compileShaders).
         * @param bUsePixelBlending      Whether the pixels of the mesh that uses this pipeline should
         * blend with existing pixels on back buffer or not (for transparency).
         * @param additionalVertexShaderMacros   Additional macros to enable for vertex shader configuration.
         * @param additionalFragmentShaderMacros Additional macros to enable for fragment shader
         * configuration.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate pipeline,
         * otherwise created pipeline.
         */
        static std::variant<std::shared_ptr<VulkanPipeline>, Error> createGraphicsPipeline(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            const std::string& sVertexShaderName,
            const std::string& sFragmentShaderName,
            bool bUsePixelBlending,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::set<ShaderMacro>& additionalFragmentShaderMacros);

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
         * Releases internal resources such as pipeline layout, internal pipeline, descriptor layout, etc.
         *
         * @warning Expects that the GPU is not referencing this pipeline and
         * that no drawing will occur until @ref restoreInternalResources is called.
         *
         * @remark Typically used before changing something (for ex. shader configuration), so that no
         * pipeline will reference old resources, to later call @ref restoreInternalResources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> releaseInternalResources() override;

        /**
         * Creates internal resources using the current shader configuration.
         *
         * @remark Called after @ref releaseInternalResources to create resources that will now reference
         * changed (new) resources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> restoreInternalResources() override;

    private:
        /**
         * Constructs uninitialized pipeline.
         *
         * @param pRenderer           Used renderer.
         * @param pPipelineManager    Pipeline manager that owns this pipeline.
         * @param sVertexShaderName   Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sFragmentShaderName Name of the compiled fragment shader (see
         * ShaderManager::compileShaders).
         * @param bUsePixelBlending   Whether the pixels of the mesh that uses this pipeline should blend with
         * existing pixels on back buffer or not (for transparency).
         */
        VulkanPipeline(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            const std::string& sVertexShaderName,
            const std::string& sFragmentShaderName,
            bool bUsePixelBlending);

        /**
         * (Re)generates Vulkan pipeline and pipeline layout for the specified shaders.
         *
         * @warning If a shader of some type was already added it will be replaced with the new one.
         * When shader is replaced the old shader gets freed from the memory and
         * a new pipeline is immediately generated. Make sure the GPU is not using old shader/pipeline.
         *
         * @param sVertexShaderName      Name of the compiled vertex shader (see
         * ShaderManager::compileShaders).
         * @param sFragmentShaderName    Name of the compiled fragment shader (see
         * ShaderManager::compileShaders).
         * @param bUsePixelBlending      Whether the pipeline should use blending or not (for transparency).
         * @param additionalVertexShaderMacros   Additional macros to enable for vertex shader.
         * @param additionalFragmentShaderMacros Additional macros to enable for fragment shader.
         *
         * @return Error if failed to generate pipeline.
         */
        [[nodiscard]] std::optional<Error> generateGraphicsPipelineForShaders(
            const std::string& sVertexShaderName,
            const std::string& sFragmentShaderName,
            bool bUsePixelBlending,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::set<ShaderMacro>& additionalFragmentShaderMacros);

        /**
         * Fully initializes @ref mtxInternalResources by creating a graphics pipeline for the specified
         * shaders.
         *
         * @param pVulkanRenderer   Vulkan renderer.
         * @param pVertexShader     Vertex shader to use in the pipeline.
         * @param pFragmentShader   Fragment shader to use in the pipeline.
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
         * Binds descriptors that reference "frameData" shader resource to frame resources' uniform buffers.
         *
         * @remark Expects that descriptor sets in @ref mtxInternalResources are initialized.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindFrameDataDescriptors();

        /**
         * Initializes internal push constants manager and returns push constants description.
         *
         * @param pushConstantUintFieldNames Stores names of fields defined in GLSL as push constants
         * (all with `uint` type).
         * @param resourceBindings           Map of pairs "resource name" (from GLSL code) - "layout
         * binding index".
         *
         * @return Error if something went wrong, otherwise push constants range.
         */
        std::variant<VkPushConstantRange, Error> definePushConstants(
            const std::unordered_set<std::string>& pushConstantUintFieldNames,
            const std::unordered_map<std::string, uint32_t>& resourceBindings);

        /**
         * Internal resources.
         * Must be used with mutex when changing.
         */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;
    };
} // namespace ne
