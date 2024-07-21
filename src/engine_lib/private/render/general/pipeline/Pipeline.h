#pragma once

// Standard.
#include <set>
#include <unordered_set>

// Custom.
#include "shader/general/ShaderUser.h"
#include "render/general/pipeline/PipelineConfiguration.h"
#include "render/general/pipeline/PipelineShaderConstantsManager.hpp"

namespace ne {
    class Renderer;
    class PipelineManager;
    class Material;
    class ComputeShaderInterface;
    class Pipeline;

    /** Base class for render specific pipeline objects. */
    class Pipeline : public ShaderUser {
        // Only Pipeline manager should be able to create Pipeline.
        friend class PipelineManager;

        // Will notify when material is no longer referencing a Pipeline.
        friend class PipelineSharedPtr;

    public:
        Pipeline() = delete;
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        virtual ~Pipeline() override = default;

        /** Groups information related to push/root constants. */
        struct ShaderConstantsData {
            /**
             * Looks for and index of the specified shader constant in the specified pipeline
             * and copies the specified value into the constant's slot.
             *
             * @warning In debug builds shows an error and throws an exception if the specified constant is
             * not used in the pipeline, otherwise in release builds does not check this and will probably
             * crash if the specified constant is not used.
             *
             * @remark Named "special" because this function is generally used for special shader constants
             * (not used-defined).
             *
             * @param pPipeline     Pipeline to look for offset of the constant.
             * @param pConstantName Name of the constant from shader code.
             * @param iValueToCopy  Value to copy as constant.
             */
            void findOffsetAndCopySpecialValueToConstant(
                Pipeline* pPipeline, const char* pConstantName, unsigned int iValueToCopy);

            /** Stores push constants. `nullptr` if push/root constants are not used. */
            std::unique_ptr<PipelineShaderConstantsManager> pConstantsManager;

            /**
             * Stores names of fields defined in GLSL as push constants or as root constants in HLSL (all with
             * `uint` type) and offset of the constant from the beginning of the layout/struct.
             */
            std::unordered_map<std::string, size_t> uintConstantsOffsets;
        };

        /**
         * Combines shader names into a one string.
         *
         * @remark This function exists to avoid duplicating the shader name combination
         * formatting.
         *
         * @param sVertexShaderName  Name of the vertex shader that pipeline is using.
         * @param sPixelShaderName   Name of the pixel shader that pipeline is using. If empty only
         * vertex shader name will be returned.
         * @param sComputeShaderName Name of the compute shader that pipeline is using. Can be empty.
         *
         * @return A (not unique) pipeline identifier.
         */
        static std::string combineShaderNames(
            std::string_view sVertexShaderName,
            std::string_view sPixelShaderName,
            std::string_view sComputeShaderName = "");

        /**
         * Returns a shader configuration of the currently used shader.
         *
         * @param shaderType Shader type to get configuration of.
         *
         * @return Empty if a shader of this type is not used by this Pipeline, otherwise shader
         * configuration.
         */
        std::optional<std::set<ShaderMacro>> getCurrentShaderConfiguration(ShaderType shaderType);

        /**
         * Returns an array of materials that currently reference this Pipeline.
         * Must be used with mutex.
         *
         * @return Array of materials that currently reference this Pipeline.
         */
        std::pair<std::mutex, std::unordered_set<Material*>>* getMaterialsThatUseThisPipeline();

        /**
         * Constructs and returns a non unique identifier of this pipeline that contains used shader names.
         *
         * @return A (not unique) pipeline identifier.
         */
        std::string getPipelineIdentifier() const;

        /**
         * Returns renderer that owns this Pipeline.
         *
         * @return Renderer.
         */
        Renderer* getRenderer() const;

        /**
         * Returns pipeline's settings and usage details.
         *
         * @warning Do not delete (free) returned pointer. It's guaranteed to live while the object is alive.
         *
         * @return Settings.
         */
        const PipelineConfiguration* getConfiguration() const;

        /**
         * Returns push/root constants used in the pipeline (if were specified in the shaders).
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Optional constants.
         */
        std::pair<std::mutex, std::optional<ShaderConstantsData>>* getShaderConstants();

    protected:
        /**
         * Creates a new empty (no internal GPU resource is created) pipeline.
         *
         * @param pRenderer              Current renderer.
         * @param pPipelineManager       Pipeline manager that owns this pipeline.
         * @param pPipelineConfiguration Settings and usage details.
         */
        explicit Pipeline(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration);

        /**
         * Saves shader configuration of the currently used shader.
         *
         * @remark This should be called by derived classes when they start to use some shader.
         *
         * @param shaderType        Type of the shader being currently used.
         * @param fullConfiguration Shader's full (might include renderer's configuration) configuration.
         */
        void saveUsedShaderConfiguration(ShaderType shaderType, std::set<ShaderMacro>&& fullConfiguration);

        /**
         * Releases internal resources such as root signature, internal Pipeline, etc.
         *
         * @warning Expects that the GPU is not referencing this Pipeline (command queue is empty) and
         * that no drawing will occur until @ref restoreInternalResources is called.
         *
         * @remark Typically used before changing something (for ex. shader configuration, texture
         * filtering), so that no Pipeline will reference old resources,
         * to later call @ref restoreInternalResources that will recreate internal resources with new
         * render settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> releaseInternalResources() = 0;

        /**
         * Creates internal resources using the current shader configuration.
         *
         * @remark Called after @ref releaseInternalResources to create resources that will now reference
         * changed (new) resources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> restoreInternalResources() = 0;

        /**
         * Sets new push/root constants that will were found in the shaders of the pipeline.
         *
         * @param uintConstantsOffsets Empty if shader constants should not be used, otherwise pairs of:
         * names of fields defined in GLSL as push constants or as root constants in HLSL (all with
         * `uint` type) and offset of the constant from the beginning of the layout/struct.
         */
        void setShaderConstants(const std::unordered_map<std::string, size_t>& uintConstantsOffsets);

    private:
        /**
         * Assigns vertex and pixel shaders to create a render specific graphics pipeline
         * (for usual rendering).
         *
         * @param pRenderer              Parent renderer that owns this pipeline.
         * @param pPipelineManager       Pipeline manager that owns this pipeline.
         * @param pPipelineConfiguration Settings that determine pipeline usage and usage details.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate Pipeline,
         * otherwise created Pipeline.
         */
        static std::variant<std::shared_ptr<Pipeline>, Error> createGraphicsPipeline(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration);

        /**
         * Assigns compute shader to create a render specific compute pipeline.
         *
         * @param pRenderer            Parent renderer that owns this pipeline.
         * @param pPipelineManager     Pipeline manager that owns this pipeline.
         * @param sComputeShaderName   Name of the compiled compute shader (see
         * ShaderManager::compileShaders).
         *
         * @return Error if shader was not found in ShaderManager or if failed to generate Pipeline,
         * otherwise created Pipeline.
         */
        static std::variant<std::shared_ptr<Pipeline>, Error> createComputePipeline(
            Renderer* pRenderer, PipelineManager* pPipelineManager, const std::string& sComputeShaderName);

        /**
         * Called to notify this pipeline that a material started storing
         * a shared pointer to this pipeline.
         *
         * @remark When a material is no longer references the Pipeline use
         * @ref onMaterialNoLongerUsingPipeline.
         *
         * @param pMaterial Material that started using this pipeline.
         */
        void onMaterialUsingPipeline(Material* pMaterial);

        /**
         * Called to notify this Pipeline that the shared pointer to this pipeline
         * (that Material stores) is now `nullptr`.
         *
         * @warning Call this function after clearing (setting to `nullptr`) the shared pointer,
         * not before.
         *
         * @param pMaterial Material that stopped using this pipeline.
         */
        void onMaterialNoLongerUsingPipeline(Material* pMaterial);

        /**
         * Called to notify this pipeline that a compute shader interface started storing
         * a shared pointer to this pipeline.
         *
         * @remark When a compute interface is no longer references the pipeline use
         * @ref onComputeShaderNoLongerUsingPipeline.
         *
         * @param pComputeShaderInterface Compute shader interface that started using this pipeline.
         */
        void onComputeShaderUsingPipeline(ComputeShaderInterface* pComputeShaderInterface);

        /**
         * Called to notify this pipeline that the shared pointer to this pipeline
         * (that compute shader interface stores) is now `nullptr`.
         *
         * @warning Call this function after clearing (setting to `nullptr`) the shared pointer,
         * not before.
         *
         * @param pComputeShaderInterface Compute shader interface that stopped using this pipeline.
         */
        void onComputeShaderNoLongerUsingPipeline(ComputeShaderInterface* pComputeShaderInterface);

        /**
         * Array of materials that currently reference this graphics pipeline.
         *
         * @remark Must be used with mutex.
         */
        std::pair<std::mutex, std::unordered_set<Material*>> mtxMaterialsThatUseThisPipeline;

        /**
         * Array of compute shader interfaces that currently reference this compute pipeline.
         *
         * @remark Must be used with mutex.
         */
        std::pair<std::mutex, std::unordered_set<ComputeShaderInterface*>>
            mtxComputeShadersThatUseThisPipeline;

        /** Not empty if push/root constants are used. */
        std::pair<std::mutex, std::optional<ShaderConstantsData>> mtxShaderConstantsData;

        /**
         * Full shader configuration (might include renderer's configuration) of a currently used shader(s).
         */
        std::unordered_map<ShaderType, std::set<ShaderMacro>> usedShaderConfiguration;

        /** Usage details of this pipeline. */
        const std::unique_ptr<PipelineConfiguration> pPipelineConfiguration;

        /** Do not delete (free) this pointer. Pipeline manager that owns this Pipeline. */
        PipelineManager* const pPipelineManager = nullptr;

        /** Do not delete (free) this pointer. Current renderer. */
        Renderer* const pRenderer = nullptr;
    };
} // namespace ne
