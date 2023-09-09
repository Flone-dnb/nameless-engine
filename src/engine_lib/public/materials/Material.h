#pragma once

// Standard.
#include <string>

// Custom.
#include "render/general/pipeline/PipelineManager.h"
#include "io/Serializable.h"
#include "materials/ShaderMacro.h"
#include "math/GLMath.hpp"
#include "materials/resources/ShaderCpuWriteResourceUniquePtr.h"
#include "materials/VulkanAlignmentConstants.hpp"

#include "Material.generated.h"

namespace ne RNAMESPACE() {
    class MeshNode;

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

        /** Visible nodes. */
        std::set<MeshNode*> visibleMeshNodes;

        /** Invisible nodes. */
        std::set<MeshNode*> invisibleMeshNodes;
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

                // TODO: vShaderResources

                // TODO: vShaderArrayResources
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
         * @param bUseTransparency  Whether this material will use transparency or not.
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
         * Sets material's fill color.
         *
         * @param diffuseColor Color in the RGB format.
         */
        void setDiffuseColor(const glm::vec3& diffuseColor);

        /**
         * Sets material's opacity.
         *
         * @remark Only works if the material has transparency enabled (see @ref create),
         * otherwise logs an error.
         *
         * @param opacity Value in range [0.0F; 1.0F].
         */
        void setOpacity(float opacity = 1.0F);

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

    private:
        /** Groups internal data. */
        struct InternalResources {
            /** Used pipeline, only valid when the mesh that is using this material is spawned. */
            PipelineSharedPtr pUsedPipeline;

            /**
             * Vertex shader macros that this material enables (i.e. DIFFUSE_TEXTURE if using a diffuse
             * texture).
             */
            std::set<ShaderMacro> materialVertexShaderMacros;

            /**
             * Pixel shader macros that this material enables (i.e. DIFFUSE_TEXTURE if using a diffuse
             * texture).
             */
            std::set<ShaderMacro> materialPixelShaderMacros;

            // TODO: texture resources go here
        };

        /**
         * Constants used by shaders.
         *
         * @remark Should be exactly the same as constant buffer in shaders.
         */
        struct MaterialShaderConstants {
            /** Fill color. */
            alignas(iVkVec3Alignment) glm::vec3 diffuseColor = glm::vec3(1.0F, 1.0F, 1.0F);

            /** Opacity (when material transparency is used). */
            alignas(iVkScalarAlignment) float opacity = 1.0F;

            // don't forget to add padding to 4 floats (if needed) for HLSL packing rules
        };

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
         * @param pMeshNode Mesh node that is currently being spawned.
         */
        void onMeshNodeSpawned(MeshNode* pMeshNode);

        /**
         * Called from MeshNode when a spawned mesh node changed its material and started using
         * this material now.
         *
         * @param pMeshNode Spawned mesh node that is using this material.
         */
        void onSpawnedMeshNodeStartedUsingMaterial(MeshNode* pMeshNode);

        /**
         * Called from MeshNode when a spawned mesh node changed its visibility.
         *
         * @param pMeshNode      Spawned mesh node that is using this material.
         * @param bOldVisibility Old visibility of the mesh.
         */
        void onSpawnedMeshNodeChangedVisibility(MeshNode* pMeshNode, bool bOldVisibility);

        /**
         * Called from MeshNode when a spawned mesh node changed its material and now no longer
         * using this material.
         *
         * @param pMeshNode Spawned mesh node that stopped using this material.
         */
        void onSpawnedMeshNodeStoppedUsingMaterial(MeshNode* pMeshNode);

        /**
         * Called from MeshNode when a spawned mesh node that uses this material is being despawned.
         *
         * @param pMeshNode Spawned mesh node that is being despawned.
         */
        void onMeshNodeDespawned(MeshNode* pMeshNode);

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
         * Called to copy data from @ref mtxShaderMaterialDataConstants.
         *
         * @return Pointer to data in @ref mtxShaderMaterialDataConstants.
         */
        void* onStartUpdatingShaderMeshConstants();

        /** Called after finished copying data from @ref mtxShaderMaterialDataConstants. */
        void onFinishedUpdatingShaderMeshConstants();

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

        /** Whether this material will use transparency or not. */
        RPROPERTY(Serialize)
        bool bUseTransparency = false;

        /** Whether @ref allocateShaderResources was called or not. */
        bool bIsShaderResourcesAllocated = false;

        /** Name of the constant buffer used to store material data in shaders. */
        static inline const auto sMaterialShaderConstantBufferName = "materialData";

        ne_Material_GENERATED
    };
} // namespace ne RNAMESPACE()

File_Material_GENERATED
