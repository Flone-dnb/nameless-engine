#pragma once

// Standard.
#include <set>

// Custom.
#include "materials/ShaderUser.h"

namespace ne {
    class Renderer;
    class PipelineManager;
    class Material;

    enum class PipelineType : size_t {
        // !!! order of entries in this enum defines draw order !!!

        PT_OPAQUE = 0,  // OPAQUE is a Windows macro, thus adding a prefix
        PT_TRANSPARENT, // TRANSPARENT is a Windows macro, thus adding a prefix

        // !!! new Pipeline types go here !!!

        // !!! order of entries in this enum defines draw order !!!

        SIZE ///< marks the size of this enum, should be the last entry
    };

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

        /**
         * Constructs a unique Pipeline identifier.
         *
         * @param sVertexShaderName  Name of the vertex shader that Pipeline is using.
         * @param sPixelShaderName   Name of the pixel shader that Pipeline is using.
         * @param bUsePixelBlending  Whether the pixels of the mesh that uses the Pipeline should blend
         * with existing pixels on back buffer or not (for transparency).
         *
         * @return Unique Pipeline identifier.
         */
        static std::string constructUniquePipelineIdentifier(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending);

        /**
         * Returns name of the vertex shader that this Pipeline is using.
         *
         * @return Empty if not using vertex shader, otherwise name of the used vertex shader.
         */
        std::string getVertexShaderName();

        /**
         * Returns name of the pixel shader that this Pipeline is using.
         *
         * @return Empty if not using pixel shader, otherwise name of the used pixel shader.
         */
        std::string getPixelShaderName();

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
         * Tells whether this Pipeline is using pixel blending or not.
         *
         * @return Whether this Pipeline is using pixel blending or not.
         */
        bool isUsingPixelBlending() const;

        /**
         * Returns an array of materials that currently reference this Pipeline.
         * Must be used with mutex.
         *
         * @return Array of materials that currently reference this Pipeline.
         */
        std::pair<std::mutex, std::set<Material*>>* getMaterialsThatUseThisPipeline();

        /**
         * Returns renderer that owns this Pipeline.
         *
         * @return Renderer.
         */
        Renderer* getRenderer() const;

    protected:
        /**
         * Creates a new uninitialized Pipeline.
         *
         * @param pRenderer         Current renderer.
         * @param pPipelineManager  Pipeline manager that owns this Pipeline.
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the pixels of the mesh that uses this Pipeline should blend
         * with existing pixels on back buffer or not (for transparency).
         */
        Pipeline(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending);

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
         * Returns unique identifier for this Pipeline.
         *
         * @return Unique Pipeline identifier.
         */
        std::string getUniquePipelineIdentifier() const;

        /**
         * Returns additional macros that were specified during pipeline creation to be enabled for
         * vertex shader configuration.
         *
         * @return Additional macros to enable for vertex shader.
         */
        std::set<ShaderMacro> getAdditionalVertexShaderMacros() const;

        /**
         * Returns additional macros that were specified during pipeline creation to be enabled for
         * pixel/fragment shader configuration.
         *
         * @return Additional macros to enable for pixel/fragment shader.
         */
        std::set<ShaderMacro> getAdditionalPixelShaderMacros() const;

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

    private:
        /**
         * Assigns vertex and pixel shaders to create a render specific graphics pipeline
         * (for usual rendering).
         *
         * @param pRenderer            Parent renderer that owns this pipeline.
         * @param pPipelineManager     Pipeline manager that owns this pipeline.
         * @param sVertexShaderName    Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName     Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending    Whether the pixels of the mesh that uses this Pipeline should
         * blend with existing pixels on back buffer or not (for transparency).
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         * @param additionalPixelShaderMacros  Additional macros to enable for pixel shader configuration.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate Pipeline,
         * otherwise created Pipeline.
         */
        static std::variant<std::shared_ptr<Pipeline>, Error> createGraphicsPipeline(
            Renderer* pRenderer,
            PipelineManager* pPipelineManager,
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::set<ShaderMacro>& additionalPixelShaderMacros);

        /**
         * Called to notify this Pipeline that a material started storing
         * a shared pointer to this Pipeline.
         *
         * @param pMaterial Material that started using this Pipeline.
         *
         * @remark When a material is no longer references the Pipeline use
         * @ref onMaterialNoLongerUsingPipeline.
         */
        void onMaterialUsingPipeline(Material* pMaterial);

        /**
         * Called to notify this Pipeline that the shared pointer to this Pipeline
         * (that Material stores) is now `nullptr`.
         *
         * @param pMaterial Material that stopped using this Pipeline.
         *
         * @warning Call this function after clearing (setting to `nullptr`) the shared pointer,
         * not before.
         */
        void onMaterialNoLongerUsingPipeline(Material* pMaterial);

        /**
         * Array of materials that currently reference this Pipeline.
         * Must be used with mutex.
         */
        std::pair<std::mutex, std::set<Material*>> mtxMaterialsThatUseThisPipeline;

        /**
         * Additional macros that were specified during pipeline creation to be enabled for
         * vertex shader configuration.
         *
         * @remark Generally used in @ref restoreInternalResources.
         */
        std::set<ShaderMacro> additionalVertexShaderMacros;

        /**
         * Additional macros that were specified during pipeline creation to be enabled for
         * pixel/fragment shader configuration.
         *
         * @remark Generally used in @ref restoreInternalResources.
         */
        std::set<ShaderMacro> additionalPixelShaderMacros;

        /** Full shader configuration (might include renderer's configuration) of a currently used shader. */
        std::unordered_map<ShaderType, std::set<ShaderMacro>> usedShaderConfiguration;

        /** Do not delete (free) this pointer. Pipeline manager that owns this Pipeline. */
        PipelineManager* pPipelineManager = nullptr;

        /** Do not delete (free) this pointer. Current renderer. */
        Renderer* pRenderer = nullptr;

        /**
         * Contains combines used shader names, transparency setting and etc. that
         * uniquely identifies the Pipeline.
         */
        std::string sUniquePipelineIdentifier;

        /** Name of the compiled vertex shader (see ShaderManager::compileShaders) that this Pipeline uses. */
        std::string sVertexShaderName;

        /** Name of the compiled pixel shader (see ShaderManager::compileShaders) that this Pipeline uses. */
        std::string sPixelShaderName;

        /** Whether this Pipeline is using pixel blending or not. */
        bool bIsUsingPixelBlending = false;
    };
} // namespace ne
