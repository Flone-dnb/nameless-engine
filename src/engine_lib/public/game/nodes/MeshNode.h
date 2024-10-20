#pragma once

// Standard.
#include <atomic>

// Custom.
#include "game/nodes/SpatialNode.h"
#include "math/GLMath.hpp"
#include "render/general/resource/GpuResource.h"
#include "shader/general/resource/binding/ShaderResourceBinding.h"
#include "shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingUniquePtr.h"
#include "shader/general/resource/binding/texture/ShaderTextureResourceBindingUniquePtr.h"
#include "shader/VulkanAlignmentConstants.hpp"
#include "misc/shapes/AABB.h"
#include "material/Material.h"
#include "render/general/resource/MeshData.h"

#include "MeshNode.generated.h"

namespace ne RNAMESPACE() {
    class UploadBuffer;

    /**
     * Represents a node that can have 3D geometry to display (mesh).
     *
     * @remark Used for GPU optimized geometry - geometry that rarely (if ever) changes from the CPU side.
     */
    class RCLASS(Guid("d5407ca4-3c2e-4a5a-9ff3-1262b6a4d264")) MeshNode : public SpatialNode {
        // Material notifies us when it changes its pipeline.
        friend class Material;

    public:
        /**
         * Constants used by shaders.
         *
         * @remark Should be exactly the same as constant buffer in shaders.
         */
        struct MeshShaderConstants {
            MeshShaderConstants() = default;

            /** Matrix to transform positions from model space to worldMatrix space. */
            alignas(iVkMat4Alignment) glm::mat4x4 worldMatrix = glm::identity<glm::mat4x4>();

            /**
             * 3x3 matrix for transforming normals from model space to world space.
             *
             * @remark Using 4x4 matrix for shader alignment/packing simplicity.
             */
            alignas(iVkMat4Alignment) glm::mat4x4 normalMatrix = glm::identity<glm::mat4x4>();
        };

        /** Stores internal GPU resources. */
        struct GpuResources {
            GpuResources() = default;

            /** Stores mesh GPU resources. */
            struct Mesh {
                Mesh() = default;

                /** Stores mesh vertices. */
                std::unique_ptr<GpuResource> pVertexBuffer;

                /** Stores one index buffer per material slot. */
                std::vector<std::unique_ptr<GpuResource>> vIndexBuffers;
            };

            /** Stores resources used by shaders. */
            struct ShaderResources {
                ShaderResources() = default;

                /** Single (non-array) shader resource bindings with CPU write access. */
                std::unordered_map<std::string, ShaderCpuWriteResourceBindingUniquePtr>
                    shaderCpuWriteResourceBindings;

                /** Shader resource bindings that reference textures. */
                std::unordered_map<std::string, ShaderTextureResourceBindingUniquePtr> shaderTextureResources;
            };

            /** Mesh GPU resources. */
            Mesh mesh;

            /** Shader GPU resources. */
            ShaderResources shaderResources;
        };

        /**
         * Returns name of the constant buffer that stores mesh constants.
         *
         * @return Name of shader resource.
         */
        static inline const char* getMeshShaderConstantBufferName() { return sMeshShaderConstantBufferName; }

        MeshNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        MeshNode(const std::string& sNodeName);

        virtual ~MeshNode() override = default;

        /**
         * Sets material to use.
         *
         * @remark Replaces old (previously used) material.
         *
         * @remark Logs an error if the specified material slot does not exist (see
         * @ref getAvailableMaterialSlotCount).
         *
         * @param pMaterial     Material to use.
         * @param iMaterialSlot Index of the material slot to set this new material to.
         * By default all meshes have 1 material at slot 0 and mesh's geometry uses only this slot.
         */
        void setMaterial(std::unique_ptr<Material> pMaterial, size_t iMaterialSlot = 0);

        /**
         * Sets mesh data (geometry) to use.
         *
         * @remark The number of available material slots will be changed according to the mesh data.
         *
         * @param meshData Mesh geometry.
         */
        void setMeshData(const MeshData& meshData);

        /**
         * Sets mesh data (geometry) to use.
         *
         * @remark The number of available material slots will be changed according to the mesh data.
         *
         * @param meshData Mesh geometry.
         */
        void setMeshData(MeshData&& meshData);

        /** Must be called after mesh data was modified to (re)create internal CPU/GPU resources. */
        void onMeshDataChanged();

        /**
         * Sets whether this mesh is visible or not.
         *
         * @param bVisible Whether this mesh is visible or not.
         */
        void setIsVisible(bool bVisible);

        /**
         * Returns material used by this name.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @param iMaterialSlot Slot from which requested material is taken from.
         *
         * @return `nullptr` if the specified slot does not exist (see @ref getAvailableMaterialSlotCount),
         * otherwise used material.
         */
        Material* getMaterial(size_t iMaterialSlot = 0);

        /**
         * Returns the total number of available material slots that the current mesh data (see
         * @ref setMeshData) provides.
         *
         * @return The number of available material slots.
         */
        size_t getAvailableMaterialSlotCount();

        /**
         * Returns mesh geometry for read/write operations.
         *
         * @warning Must be used with mutex.
         *
         * @warning If you're changing mesh data you must call @ref onMeshDataChanged after you
         * finished modifying the mesh data to update internal CPU/GPU resources and see updated geometry
         * on the screen, otherwise this might cause the object to be displayed incorrectly.
         *
         * @remark Note that changing mesh data using @ref onMeshDataChanged will have the same performance as
         * if you used @ref setMeshData because they both do the same work. The main reason why this getter
         * exists is to avoid copying MeshData when somebody wants to read/modify existing mesh data.
         *
         * @return Mesh data.
         */
        inline std::pair<std::recursive_mutex*, MeshData*> getMeshData() {
            return std::make_pair(&mtxMeshData, &meshData);
        }

        /**
         * Returns GPU resources that this mesh node uses.
         *
         * @return GPU resources.
         */
        inline std::pair<std::recursive_mutex, GpuResources>* getMeshGpuResources() {
            return &mtxGpuResources;
        }

        /**
         * Returns shader constants (copied to GPU).
         *
         * @remark Changes made this returned object will not be copied to the GPU, this getter
         * only exists read access.
         *
         * @return Mesh constants.
         */
        inline std::pair<std::recursive_mutex, MeshShaderConstants>* getMeshShaderConstants() {
            return &mtxShaderMeshDataConstants;
        }

        /**
         * Returns pointer to axis-aligned bounding box of this mesh.
         *
         * @return Axis-aligned bounding box.
         */
        inline AABB* getAABB() { return &aabb; }

        /**
         * Tells whether this mesh is currently visible or not.
         *
         * @return Whether the mesh is visible or not.
         */
        bool isVisible() const;

    protected:
        /**
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() override;

        /**
         * Called when this node was not spawned previously and it was either attached to a parent node
         * that is spawned or set as world's root node to execute custom spawn logic.
         *
         * @remark This node will be marked as spawned before this function is called.
         * @remark This function is called before any of the child nodes are spawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onSpawning() override;

        /**
         * Called before this node is despawned from the world to execute custom despawn logic.
         *
         * @remark This node will be marked as despawned after this function is called.
         * @remark This function is called after all child nodes were despawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onDespawning() override;

        /**
         * Called after node's world location/rotation/scale was changed.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onWorldLocationRotationScaleChanged() override;

        /**
         * Setups callbacks for a shader resource (buffer or a texture from the shader code) with CPU write
         * access to copy the data from the CPU to the GPU to be used by the shaders.
         *
         * @remark Call this function in your @ref onSpawning function to bind to shader resources, all
         * bindings will be automatically removed in @ref onDespawning. An error will be shown if this
         * function is called when the node is not spawned.
         *
         * @remark When the data of a resource that you registered was updated on the CPU side you need
         * to call @ref markShaderCpuWriteResourceToBeCopiedToGpu so that the specified update callbacks will
         * be called and the updated data will be copied to the GPU to be used by the shaders. Note that you
         * don't need to call @ref markShaderCpuWriteResourceToBeCopiedToGpu for resources you have not
         * registered yourself. Also note that all registered resources are marked as "needs an update" by
         * default so you don't have to call @ref markShaderCpuWriteResourceToBeCopiedToGpu right after
         * calling this function.
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
        void setShaderCpuWriteResourceBinding(
            const std::string& sShaderResourceName,
            size_t iResourceSizeInBytes,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Setups a shader resource binding that references a texture from the shader code that is used when
         * this mesh is rendered.
         *
         * @remark Call this function in @ref allocateShaderResources to bind to shader resources, all
         * bindings will be automatically removed in @ref deallocateShaderResources.
         *
         * @param sShaderResourceName               Name of the resource we are referencing (should be exactly
         * the same as the resource name written in the shader file we are referencing).
         * @param sPathToTextureResourceRelativeRes Path to the directory with texture resource to use.
         */
        void setShaderTextureResourceBinding(
            const std::string& sShaderResourceName, const std::string& sPathToTextureResourceRelativeRes);

        /**
         * Looks for binding created using @ref setShaderCpuWriteResourceBinding and
         * notifies the engine that there is new (updated) data for shader CPU write resource to copy
         * to the GPU to be used by shaders.
         *
         * @remark You don't need to check if the node is spawned or not before calling this function,
         * if the binding does not exist or some other condition is not met this call will be
         * silently ignored without any errors.
         *
         * @remark Note that the callbacks that you have specified in
         * @ref setShaderCpuWriteResourceBinding will not be called inside of this function,
         * moreover they are most likely to be called in the next frame(s) (most likely multiple times)
         * when the engine is ready to copy the data to the GPU, so if the resource's data is used by
         * multiple threads in your code, make sure to use mutex or other synchronization primitive
         * in your callbacks.
         *
         * @param sShaderResourceName Name of the shader CPU write resource (should be exactly the same
         * as the resource name written in the shader file we are referencing).
         */
        void markShaderCpuWriteResourceToBeCopiedToGpu(const std::string& sShaderResourceName);

    private:
        /**
         * Returns default mesh node material.
         *
         * @return Default material for mesh node.
         */
        static std::unique_ptr<Material> getDefaultMaterial();

        /**
         * Allocates shader resources (see @ref mtxGpuResources).
         *
         * @warning Expects that @ref vMaterials to have initialized PSOs.
         */
        void allocateShaderResources();

        /** Allocates geometry resources (see @ref mtxGpuResources). */
        void allocateGeometryBuffers();

        /** Deallocates shader resources (see @ref mtxGpuResources). */
        void deallocateShaderResources();

        /** Deallocates geometry resources (see @ref mtxGpuResources). */
        void deallocateGeometryBuffers();

        /**
         * Called to copy data from @ref mtxShaderMeshDataConstants.
         *
         * @return Pointer to data in @ref mtxShaderMeshDataConstants.
         */
        void* onStartedUpdatingShaderMeshConstants();

        /** Called after finished copying data from @ref mtxShaderMeshDataConstants. */
        void onFinishedUpdatingShaderMeshConstants();

        /**
         * Called after @ref vMaterials is changed (some slots are added/removed) to notify
         * all shader resources.
         *
         * @warning Expects that the caller is using some mutex to protect this shader resource
         * from being used in the `draw` function while this function is not finished
         * (i.e. make sure the CPU will not queue a new frame while this function is not finished).
         *
         * @warning Expects that the node is spawned and all @ref vMaterials have pipelines initialized.
         */
        void updateShaderResourcesToUseChangedMaterialPipelines();

        /**
         * Returns information about an index buffer for the specified material slot.
         *
         * @warning Shows an error and throws an exception if the index buffer at the specified
         * index cannon be found.
         *
         * @param iMaterialSlot Material slot that corresponds with an index buffer to look for.
         *
         * @return Information about an index buffer at the specified material slot.
         */
        std::pair<GpuResource*, unsigned int> getIndexBufferInfoForMaterialSlot(size_t iMaterialSlot);

        /**
         * Stores materials of the mesh. Material at index 0 used by index buffer 0, material
         * at index 1 used by index buffer 1 and so on (@ref meshData defines available slots).
         *
         * @remark Always contains valid pointers.
         */
        RPROPERTY(Serialize)
        std::vector<std::unique_ptr<Material>> vMaterials;

        /**
         * Mesh geometry.
         *
         * @warning Use with @ref mtxMeshData.
         */
        RPROPERTY(Serialize(FST_AS_EXTERNAL_BINARY_FILE)) // allow VCSs to treat this file in a special way
        MeshData meshData;

        /** Axis-aligned bounding box for @ref meshData. */
        AABB aabb;

        /** Mutex for @ref meshData. */
        std::recursive_mutex mtxMeshData;

        /** Stores GPU resources used by this mesh node.  */
        std::pair<std::recursive_mutex, GpuResources> mtxGpuResources;

        /** Stores data for constant buffer used by shaders. */
        std::pair<std::recursive_mutex, MeshShaderConstants> mtxShaderMeshDataConstants;

        /** Whether mesh is visible or not. */
        bool bIsVisible = true;

        /** Name of the constant buffer used to store general mesh data in shaders. */
        static inline const auto sMeshShaderConstantBufferName = "meshData";

        ne_MeshNode_GENERATED
    };
}

File_MeshNode_GENERATED
