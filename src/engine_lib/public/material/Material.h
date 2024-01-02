#pragma once

// Standard.
#include <string>
#include <unordered_map>

// Custom.
#include "render/general/pipeline/PipelineManager.h"
#include "io/Serializable.h"
#include "shader/general/ShaderMacro.h"
#include "math/GLMath.hpp"
#include "shader/general/resources/cpuwrite/ShaderCpuWriteResourceUniquePtr.h"
#include "shader/general/resources/texture/ShaderTextureResourceUniquePtr.h"
#include "shader/VulkanAlignmentConstants.hpp"
#include "material/TextureManager.h"

#include "Material.generated.h"

namespace ne RNAMESPACE() {
    class MeshNode;
    class GpuResource;

    /** Groups information about an index buffer of a mesh. */
    struct MeshIndexBufferInfo {
        /** Creates uninitialized object. */
        MeshIndexBufferInfo() = default;

        /**
         * Initializes the object.
         *
         * @param pIndexBuffer Index buffer that this material should display.
         * @param iIndexCount  The total number of indices stores in the specified index buffer.
         */
        MeshIndexBufferInfo(GpuResource* pIndexBuffer, unsigned int iIndexCount) {
            this->pIndexBuffer = pIndexBuffer;
            this->iIndexCount = iIndexCount;
        }

        /** A non-owning pointer to mesh's index buffer. */
        GpuResource* pIndexBuffer = nullptr;

        /** The total number of indices stores in @ref pIndexBuffer. */
        unsigned int iIndexCount = 0;
    };

    /** Groups mesh nodes by visibility. */
    struct MeshNodesThatUseThisMaterial {
        /**
         * Returns total number of visible and invisible mesh nodes that use this material.
         *
         * @return Total number of nodes that use this material.
         */
        size_t getTotalSize() const { return visibleMeshNodes.size() + invisibleMeshNodes.size(); }

        /**
         * Tells whether the specified node is already added to be considered.
         *
         * @param pMeshNode Mesh node to check.
         *
         * @return Whether the node is added or not.
         */
        bool isMeshNodeAdded(MeshNode* pMeshNode) {
            auto it = visibleMeshNodes.find(pMeshNode);
            if (it != visibleMeshNodes.end()) {
                return true;
            }

            it = invisibleMeshNodes.find(pMeshNode);
            if (it != invisibleMeshNodes.end()) {
                return true;
            }

            return false;
        }

        /** Stores pairs of "visible mesh node" - "index buffers of that mesh that use this material". */
        std::unordered_map<MeshNode*, std::vector<MeshIndexBufferInfo>> visibleMeshNodes;

        /** Stores pairs of "invisible mesh node" - "index buffers of that mesh that use this material". */
        std::unordered_map<MeshNode*, std::vector<MeshIndexBufferInfo>> invisibleMeshNodes;
    };

    /** Defines visual aspects of a mesh. */
    class RCLASS(Guid("a603fa3a-e9c2-4c38-bb4c-76384ef001f4")) Material : public Serializable {
        // Mesh node will notify the material when it's spawned/despawned.
        friend class MeshNode;

    public:
        /** Stores internal GPU resources. */
        struct GpuResources {
            /** Stores resources used by shaders. */
            struct ShaderResources {
                /** Shader single (non-array) resources with CPU write access. */
                std::unordered_map<std::string, ShaderCpuWriteResourceUniquePtr> shaderCpuWriteResources;

                /** Shader resources that reference textures. */
                std::unordered_map<std::string, ShaderTextureResourceUniquePtr> shaderTextureResources;
            };

            /** Shader GPU resources. */
            ShaderResources shaderResources;
        };

        /** Creates uninitialized material, only used for deserialization, instead use @ref create. */
        Material();

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        virtual ~Material() override;

        /**
         * Returns total amount of currently alive material objects.
         *
         * @return Total amount of alive materials.
         */
        static size_t getCurrentAliveMaterialCount();

        /**
         * Creates a new material that uses the specified shaders.
         *
         * @param sVertexShaderName Name of the compiled vertex shader
         * (see ShaderManager::compileShaders) to use.
         * @param sPixelShaderName  Name of the compiled pixel shader
         * (see ShaderManager::compileShaders) to use.
         * @param bUseTransparency  Whether this material should enable transparency after being created or
         * not (see @ref setEnableTransparency).
         * @param sMaterialName     Name of this material.
         *
         * @return Error if something went wrong, otherwise created material.
         */
        static std::variant<std::shared_ptr<Material>, Error> create(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUseTransparency,
            const std::string& sMaterialName = "Material");

        /**
         * Enables/disables transparency.
         *
         * @param bEnable Whether to enable transparency or not.
         */
        void setEnableTransparency(bool bEnable);

        /**
         * Sets material's fill color.
         *
         * @param diffuseColor Color in the RGB format.
         */
        void setDiffuseColor(const glm::vec3& diffuseColor);

        /**
         * Sets material's diffuse texture.
         *
         * Example:
         * @code
         * // Specify path to a directory that stores DDS and KTX files with player's diffuse texture.
         * pMaterial->setDiffuseTexture("game/player/textures/diffuse");
         * @endcode
         *
         * @param sTextureResourcePathRelativeRes Path to a texture resource (file/directory) relative
         * to `res` directory that this material should now use. Specify empty string to clear the current
         * diffuse texture (if any is set) and don't use diffuse texture at all.
         */
        void setDiffuseTexture(const std::string& sTextureResourcePathRelativeRes);

        /**
         * Sets material's reflected color.
         *
         * @param specularColor Color in the RGB format.
         */
        void setSpecularColor(const glm::vec3& specularColor);

        /**
         * Sets factor that defines how much specular light will be reflected
         * (i.e. how rough or smooth the surface is).
         *
         * @param roughness Value in range [0.0F; 1.0F], will be clamped if outside of this range.
         */
        void setRoughness(float roughness);

        /**
         * Sets material's opacity.
         *
         * @remark Only works if the material has transparency enabled (see @ref create or
         * @ref setEnableTransparency).
         *
         * @param opacity Value in range [0.0F; 1.0F], will be clamped if outside of this range.
         */
        void setOpacity(float opacity = 1.0F);

        /**
         * Tells whether transparency on this material is enabled or not.
         *
         * @return `true` if enabled, `false` otherwise.
         */
        bool isTransparencyEnabled();

        /**
         * Returns fill color of this material.
         *
         * @return Color in the RGB format.
         */
        glm::vec3 getDiffuseColor() const;

        /**
         * Returns reflected color of this material.
         *
         * @return Color in the RGB format.
         */
        glm::vec3 getSpecularColor() const;

        /**
         * Returns path to a file/directory that stores currently used diffuse texture of this material.
         *
         * @return Empty if no diffuse texture is set, otherwise path to a file/directory relative to
         * the `res` directory.
         */
        std::string getPathToDiffuseTextureResource();

        /**
         * Returns roughness of this material.
         *
         * @return Value in range [0.0F; 1.0F].
         */
        float getRoughness() const;

        /**
         * Returns opacity of this material.
         *
         * @return Value in range [0.0F; 1.0F].
         */
        float getOpacity() const;

        /**
         * Returns array of mesh nodes that currently use this material.
         * Must be used with mutex.
         *
         * @return Array of mesh nodes.
         */
        std::pair<std::mutex, MeshNodesThatUseThisMaterial>* getSpawnedMeshNodesThatUseThisMaterial();

        /**
         * Returns material name.
         *
         * @return Material name.
         */
        std::string getMaterialName() const;

        /**
         * Tells whether this material uses transparency or not.
         *
         * @return Whether this material uses transparency or not.
         */
        bool isUsingTransparency() const;

        /**
         * Returns pipeline that this material uses.
         *
         * @warning Do not delete returned pointer.
         *
         * @return `nullptr` if pipeline was not initialized yet, otherwise used pipeline.
         */
        Pipeline* getUsedPipeline() const;

        /**
         * Returns pipeline that only has vertex shader (used for depth only passes).
         *
         * @warning Do not delete returned pointer.
         *
         * @return `nullptr` if pipeline was not initialized yet or if @ref isUsingTransparency is enabled,
         * otherwise used pipeline.
         */
        Pipeline* getDepthOnlyPipeline() const;

        /**
         * Returns pipeline that only has vertex shader and depth bias enabled (used for shadow passes).
         *
         * @warning Do not delete returned pointer.
         *
         * @return `nullptr` if pipeline was not initialized yet or if @ref isUsingTransparency is enabled,
         * otherwise used pipeline.
         */
        Pipeline* getShadowMappingPipeline() const;

        /**
         * Returns GPU resources that this material uses.
         *
         * @return GPU resources.
         */
        inline std::pair<std::recursive_mutex, GpuResources>* getMaterialGpuResources() {
            return &mtxGpuResources;
        }

        /**
         * Returns name of the vertex shader that this material uses.
         *
         * @return Vertex shader name.
         */
        std::string getVertexShaderName() const;

        /**
         * Returns name of the pixel shader that this material uses.
         *
         * @return Pixel shader name.
         */
        std::string getPixelShaderName() const;

    protected:
        /**
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() override;

    private:
        /** Groups internal data. */
        struct InternalResources {
            /**
             * Used pipeline with pixel/fragment shader enabled.
             *
             * @remark This pipeline is considered to be the main pipeline while others might be optional.
             * @remark Only valid when the mesh that is using this material is spawned.
             */
            PipelineSharedPtr pColorPipeline;

            /**
             * @ref pColorPipeline but with only vertex shader (used for depth only passes).
             *
             * @remark Only valid when the mesh that is using this material is spawned.
             * @remark Only valid when material does not use transparency.
             */
            PipelineSharedPtr pDepthOnlyPipeline;

            /**
             * @ref pColorPipeline but with only vertex shader and depth bias enabled (used for shadow
             * passes).
             *
             * @remark Only valid when the mesh that is using this material is spawned.
             * @remark Only valid when material does not use transparency.
             */
            PipelineSharedPtr pShadowMappingPipeline;
        };

        /**
         * Constants used by shaders.
         *
         * @remark Should be exactly the same as constant buffer in shaders.
         */
        struct MaterialShaderConstants {
            /** Fill color. 4th component stores opacity when transparency is used. */
            alignas(iVkVec4Alignment) glm::vec4 diffuseColor = glm::vec4(1.0F, 1.0F, 1.0F, 1.0F);

            /** Reflected color. 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 specularColor = glm::vec4(1.0F, 1.0F, 1.0F, 1.0F);

            /** Defines how much specular light will be reflected. */
            alignas(iVkScalarAlignment) float roughness = 0.0F;
        };

        /**
         * Returns pipeline manager.
         *
         * @remark Generally called before creating a new material to get pipeline manager and
         * also check that selected shader names indeed exist.
         *
         * @param sVertexShaderName Name of the vertex shader that the material is using.
         * @param sPixelShaderName  Name of the pixel shader that the material is using.
         *
         * @return Error if something went wrong, otherwise pipeline manager.
         */
        static std::variant<PipelineManager*, Error> getPipelineManagerForNewMaterial(
            const std::string& sVertexShaderName, const std::string& sPixelShaderName);

        /**
         * Creates a new material with the specified name.
         *
         * @remark This constructor should only be used internally (only by this class), use @ref
         * create instead.
         *
         * @param sVertexShaderName Name of the vertex shader that this material is using.
         * @param sPixelShaderName  Name of the pixel shader that this material is using.
         * @param bUseTransparency  Whether this material will use transparency or not.
         * @param pPipelineManager  Pipeline manager that the renderer owns.
         * @param sMaterialName     Name of this material.
         */
        Material(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUseTransparency,
            PipelineManager* pPipelineManager,
            const std::string& sMaterialName = "Material");

        /**
         * Called from MeshNode when a mesh node that uses this material is being spawned.
         *
         * @warning Expects that the mesh will not change its visibility while calling this function.
         *
         * @param pMeshNode            Mesh node that is currently being spawned.
         * @param indexBufferToDisplay Index buffer that this material should display.
         */
        void onMeshNodeSpawning(
            MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferToDisplay);

        /**
         * Called from MeshNode when a spawned mesh node changed its material and started using
         * this material now.
         *
         * @warning Expects that the mesh will not change its visibility while calling this function.
         *
         * @param pMeshNode            Spawned mesh node that is using this material.
         * @param indexBufferToDisplay Index buffer that this material should display.
         */
        void onSpawnedMeshNodeStartedUsingMaterial(
            MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferToDisplay);

        /**
         * Called from MeshNode when a spawned mesh node re-created some index buffer
         * and now wants to notify the material about it.
         *
         * @warning Expects that the mesh will not change its visibility while calling this function.
         *
         * @param pMeshNode          Spawned mesh node that is using this material.
         * @param deletedIndexBuffer Index buffer that was deleted.
         * @param newIndexBuffer     Index buffer that this material should display now.
         */
        void onSpawnedMeshNodeRecreatedIndexBuffer(
            MeshNode* pMeshNode,
            const std::pair<GpuResource*, unsigned int>& deletedIndexBuffer,
            const std::pair<GpuResource*, unsigned int>& newIndexBuffer);

        /**
         * Called from MeshNode when a spawned mesh node changed its visibility.
         *
         * @warning Expects that the mesh will not change its visibility while calling this function.
         *
         * @param pMeshNode      Spawned mesh node that is using this material.
         * @param bOldVisibility Old visibility of the mesh.
         */
        void onSpawnedMeshNodeChangedVisibility(MeshNode* pMeshNode, bool bOldVisibility);

        /**
         * Called from MeshNode when a spawned mesh node changed its material and now no longer
         * using this material.
         *
         * @warning Expects that the mesh will not change its visibility while calling this function.
         *
         * @param pMeshNode            Spawned mesh node that stopped using this material.
         * @param indexBufferDisplayed Index buffer that this material was displaying.
         */
        void onSpawnedMeshNodeStoppedUsingMaterial(
            MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferDisplayed);

        /**
         * Called from MeshNode when a spawned mesh node that uses this material is being despawned.
         *
         * @warning Expects that the mesh will not change its visibility while calling this function.
         *
         * @param pMeshNode            Spawned mesh node that is being despawned.
         * @param indexBufferDisplayed Index buffer that this material was displaying.
         */
        void onMeshNodeDespawning(
            MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferDisplayed);

        /**
         * Initializes pipelines that the material needs.
         *
         * @remark Expects that pipelines are not initialized.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> initializePipelines();

        /** Clears (sets to `nullptr`) all used pipelines. */
        void resetPipelines();

        /**
         * Creates shader resources such as material's constant buffer.
         *
         * @remark Should be called after pipeline was initialized.
         */
        void allocateShaderResources();

        /**
         * Deallocates shader resources after @ref allocateShaderResources was called.
         *
         * @remark Should be called before pipeline is cleared.
         */
        void deallocateShaderResources();

        /**
         * Setups callbacks for shader resource with CPU write access to copy data from CPU to
         * the GPU to be used by shaders.
         *
         * @remark Call this function in @ref allocateShaderResources to bind to shader resources, all
         * bindings will be automatically removed in @ref deallocateShaderResources.
         *
         * @remark When data of a resource that you registered was updated on the CPU side you need
         * to call @ref markShaderCpuWriteResourceAsNeedsUpdate so that update callbacks will be
         * called and updated data will be copied to the GPU to be used by shaders.
         * Note that you don't need to call @ref markShaderCpuWriteResourceAsNeedsUpdate for
         * resources you have not registered yourself. Also note that all registered resources
         * are marked as "need update" by default so you don't have to call
         * @ref markShaderCpuWriteResourceAsNeedsUpdate right after calling this function.
         *
         * @param sShaderResourceName        Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param iResourceSizeInBytes       Size of the data that this resource will contain. Note that
         * the specified size will most likely be padded (changed) to be a multiple of 256 because of
         * the hardware requirement for shader constant buffers.
         * @param onStartedUpdatingResource  Function that will be called when the engine has started
         * copying data to the GPU. Function returns pointer to new (updated) data
         * of the specified resource that will be copied to the GPU.
         * @param onFinishedUpdatingResource Function that will be called when the engine has finished
         * copying resource data to the GPU (usually used for unlocking mutexes).
         */
        void setShaderCpuWriteResourceBindingData(
            const std::string& sShaderResourceName,
            size_t iResourceSizeInBytes,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Setups a shader resource that references a texture  that will be used in shaders when
         * this material is rendered.
         *
         * @remark Call this function in @ref allocateShaderResources to bind to shader resources, all
         * bindings will be automatically removed in @ref deallocateShaderResources.
         *
         * @param sShaderResourceName               Name of the resource we are referencing (should be exactly
         * the same as the resource name written in the shader file we are referencing).
         * @param sPathToTextureResourceRelativeRes Path to file/directory with texture resource to use.
         */
        void setShaderTextureResourceBindingData(
            const std::string& sShaderResourceName, const std::string& sPathToTextureResourceRelativeRes);

        /**
         * Looks for binding created using @ref setShaderCpuWriteResourceBindingData and
         * notifies the engine that there is new (updated) data for shader CPU write resource to copy
         * to the GPU to be used by shaders.
         *
         * @remark You don't need to check if the pipeline is initialized or not before calling this function,
         * if the binding does not exist or some other condition is not met this call will be
         * silently ignored without any errors.
         *
         * @remark Note that the callbacks that you have specified in
         * @ref setShaderCpuWriteResourceBindingData will not be called inside of this function,
         * moreover they are most likely to be called in the next frame(s) (most likely multiple times)
         * when the engine is ready to copy the data to the GPU, so if the resource's data is used by
         * multiple threads in your code, make sure to use mutex or other synchronization primitive
         * in your callbacks.
         *
         * @param sShaderResourceName Name of the shader CPU write resource (should be exactly the same
         * as the resource name written in the shader file we are referencing).
         */
        void markShaderCpuWriteResourceAsNeedsUpdate(const std::string& sShaderResourceName);

        /**
         * Releases all shader resources, requests a new pipeline according to currently defined
         * shader macros, allocates all shader resources and notifies all mesh nodes that use this material.
         */
        void updateToNewPipeline();

        /**
         * Called to copy data from @ref mtxShaderMaterialDataConstants.
         *
         * @return Pointer to data in @ref mtxShaderMaterialDataConstants.
         */
        void* onStartUpdatingShaderMeshConstants();

        /** Called after finished copying data from @ref mtxShaderMaterialDataConstants. */
        void onFinishedUpdatingShaderMeshConstants();

        /**
         * Analyzes @ref mtxInternalResources and returns vertex shader macros that should
         * be enabled to support the material's features.
         *
         * @return Macros.
         */
        std::set<ShaderMacro> getVertexShaderMacrosForCurrentState();

        /**
         * Analyzes @ref mtxInternalResources and returns pixel/fragment shader macros that should
         * be enabled to support the material's features.
         *
         * @return Macros.
         */
        std::set<ShaderMacro> getPixelShaderMacrosForCurrentState();

        /**
         * Array of spawned mesh nodes that use this material.
         * Must be used with mutex.
         */
        std::pair<std::mutex, MeshNodesThatUseThisMaterial> mtxSpawnedMeshNodesThatUseThisMaterial;

        /** Internal data. */
        std::pair<std::recursive_mutex, InternalResources> mtxInternalResources;

        /** Stores GPU resources used by this material. */
        std::pair<std::recursive_mutex, GpuResources> mtxGpuResources;

        /** Stores data for constant buffer used by shaders. */
        std::pair<std::recursive_mutex, MaterialShaderConstants> mtxShaderMaterialDataConstants;

        /** Do not delete (free) this pointer. Pipeline manager that the renderer owns. */
        PipelineManager* pPipelineManager = nullptr;

        /** Name of the vertex shader that this material is using. */
        RPROPERTY(Serialize)
        std::string sVertexShaderName;

        /** Name of the pixel shader that this material is using. */
        RPROPERTY(Serialize)
        std::string sPixelShaderName;

        /** Name of this material. */
        RPROPERTY(Serialize)
        std::string sMaterialName;

        /**
         * Empty if diffuse texture is not used, otherwise path to used diffuse texture relative
         * to `res` directory.
         */
        RPROPERTY(Serialize)
        std::string sDiffuseTexturePathRelativeRes;

        /** Fill color. */
        RPROPERTY(Serialize)
        glm::vec3 diffuseColor = glm::vec3(1.0F, 1.0F, 1.0F);

        /** Reflected color. */
        RPROPERTY(Serialize)
        glm::vec3 specularColor = glm::vec3(1.0F, 1.0F, 1.0F);

        /** Defines how much specular light will be reflected. Value in range [0.0F; 1.0F]. */
        RPROPERTY(Serialize)
        float roughness = 0.7F;

        /**
         * Opacity in range [0.0; 1.0].
         *
         * @remark Only used when @ref bUseTransparency is enabled.
         */
        RPROPERTY(Serialize)
        float opacity = 0.6F;

        /** Whether this material will use transparency or not. */
        RPROPERTY(Serialize)
        bool bUseTransparency = false;

        /** Whether @ref allocateShaderResources was called or not. */
        bool bIsShaderResourcesAllocated = false;

        /** Name of the constant buffer used to store material data in shaders. */
        static inline const auto sMaterialShaderConstantBufferName = "materialData";

        /** Name of the resource used to store diffuse texture in shaders. */
        static inline const auto sMaterialShaderDiffuseTextureName = "diffuseTexture";

        ne_Material_GENERATED
    };
} // namespace ne RNAMESPACE()

File_Material_GENERATED
