#pragma once

// Standard.
#include <atomic>

// Custom.
#include "game/nodes/SpatialNode.h"
#include "math/GLMath.hpp"
#include "render/general/resources/GpuResource.h"
#include "materials/ShaderResource.h"
#include "materials/ShaderReadWriteResourceUniquePtr.h"

#include "MeshNode.generated.h"

namespace ne RNAMESPACE() {
    class Material;
    class UploadBuffer;

    /**
     * Vertex of a mesh.
     *
     * @remark Equal to the vertex struct we use in shaders.
     */
    struct MeshVertex { // not using inheritance to avoid extra fields that are not related to vertex
        MeshVertex() = default;

        /** Copy constructor. */
        MeshVertex(const MeshVertex&) = default;

        /**
         * Copy assignment.
         *
         * @return This.
         */
        MeshVertex& operator=(const MeshVertex&) = default;

        /** Move constructor. */
        MeshVertex(MeshVertex&&) noexcept = default;

        /**
         * Move assignment.
         *
         * @return This.
         */
        MeshVertex& operator=(MeshVertex&&) noexcept = default;

        /**
         * Equality operator.
         *
         * @param other Other object.
         *
         * @return Whether objects are equal or not.
         */
        bool operator==(const MeshVertex& other) const;

        /**
         * Serializes array of vertices into the specified section.
         *
         * @param pFrom         Array of vertices to serialize.
         * @param pToml         TOML value to serialize to.
         * @param sSectionName  Name of the section to serialize to.
         */
        static void
        serializeVec(std::vector<MeshVertex>* pFrom, toml::value* pToml, const std::string& sSectionName);

        /**
         * Deserializes TOML value to array of vertices.
         *
         * @param pTo     Array of vertices to deserialize to.
         * @param pToml   TOML value to deserialize.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> deserializeVec(std::vector<MeshVertex>* pTo, const toml::value* pToml);

        // --------------------------------------------------------------------------------------

        /** Position of the vertex in a 3D space. */
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);

        /** Normal vector of the vertex. */
        glm::vec3 normal = glm::vec3(0.0f, 0.0f, 0.0f);

        /** UV coordinates of the vertex. */
        glm::vec2 uv = glm::vec2(0.0f, 0.0f);

        // --------------------------------------------------------------------------------------

        // ! only vertex related fields (same as in shader) can be added here !
        // ! add new fields to `serializeVec`, `deserializeVec` and `operator==` !
        // ! and to unit test !
        // (not deriving from `Serializable` to avoid extra fields that are not related to vertex)

        // --------------------------------------------------------------------------------------
    };

    /** Stores mesh geometry (vertices and indices). */
    class RCLASS(Guid("b60e4b47-b1e6-4001-87a8-b7885b4e8383")) MeshData : public Serializable {
    public:
        /** Type of mesh index. */
        using meshindex_t = unsigned int; // if making this dynamic (changes depending on the number of
                                          // indices) change hardcoded FORMAT in the renderer

        MeshData();
        virtual ~MeshData() override = default;

        /** Copy constructor. */
        MeshData(const MeshData&) = default;

        /**
         * Copy assignment.
         *
         * @return This.
         */
        MeshData& operator=(const MeshData&) = default;

        /** Move constructor. */
        MeshData(MeshData&&) noexcept = default;

        /**
         * Move assignment.
         *
         * @return This.
         */
        MeshData& operator=(MeshData&&) noexcept = default;

        /**
         * Returns mesh vertices.
         *
         * @return Mesh vertices.
         */
        std::vector<MeshVertex>* getVertices();

        /**
         * Returns mesh indices.
         *
         * @return Mesh indices.
         */
        std::vector<meshindex_t>* getIndices();

    private:
        /** Mesh vertices. */
        RPROPERTY(Serialize)
        std::vector<MeshVertex> vVertices;

        /** Stores mesh indices. */
        RPROPERTY(Serialize)
        std::vector<meshindex_t> vIndices;

        ne_MeshData_GENERATED
    };

    /**
     * Represents a node that can have 3D geometry to display (mesh).
     *
     * @remark Used for GPU optimized geometry - geometry that rarely (if ever) changes from the CPU side.
     */
    class RCLASS(Guid("d5407ca4-3c2e-4a5a-9ff3-1262b6a4d264")) MeshNode : public SpatialNode {
        // Renderer will call `draw` on this node.
        friend class Renderer;

    public:
        /** Stores internal GPU resources. */
        struct GpuResources {

            /** Stores mesh GPU resources. */
            struct Mesh {
                /** Stores mesh vertices. */
                std::unique_ptr<GpuResource> pVertexBuffer;

                /** Stores mesh indices. */
                std::unique_ptr<GpuResource> pIndexBuffer;
            };

            /** Stores resources used by shaders. */
            struct ShaderResources {
                /** Shader single (non-array) resources with CPU access. */
                std::unordered_map<std::string, ShaderCpuReadWriteResourceUniquePtr>
                    shaderCpuReadWriteResources;

                // TODO: vShaderCpuReadOnlyResources

                // TODO: vShaderCpuReadOnlyArrayResources
            };

            /** Mesh GPU resources. */
            Mesh mesh;

            /** Shader GPU resources. */
            ShaderResources shaderResources;
        };

        /**
         * Constants used by shaders.
         *
         * @remark Should be exactly the same as constant buffer in shaders.
         */
        struct MeshShaderConstants {
            /** World matrix. */
            glm::mat4x4 world = glm::identity<glm::mat4x4>();

            // remember to add padding to 4 floats (if needed)
        };

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
         * @param pMaterial Material to use.
         */
        void setMaterial(std::shared_ptr<Material> pMaterial);

        /**
         * Sets mesh data (geometry) to use.
         *
         * @param meshData Mesh geometry.
         */
        void setMeshData(const MeshData& meshData);

        /**
         * Sets mesh data (geometry) to use.
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
        void setVisibility(bool bVisible);

        /**
         * Returns material used by this name.
         *
         * @return Material.
         */
        std::shared_ptr<Material> getMaterial();

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
         * Returns GPU resources that mesh node uses.
         *
         * @return GPU resources.
         */
        inline std::pair<std::recursive_mutex, GpuResources>* getMeshGpuResources() {
            return &mtxGpuResources;
        }

        /**
         * Tells whether this mesh is currently visible or not.
         *
         * @return Whether the mesh is visible or not.
         */
        bool isVisible() const;

    protected:
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
         * @remark If node's destructor is called but node is still spawned it will be despawned.
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
         * Prepares data for binding to shader resource with CPU read/write access to copy data from CPU to
         * shader.
         *
         * @remark Call this function in your onSpawn function to bind to shader resources, all bindings
         * will be automatically removed in onDespawn.
         *
         * @param sShaderResourceName        Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param iResourceSizeInBytes       Size of the data that this resource will contain. Note that
         * this size will most likely be padded to be a multiple of 256 because of the hardware requirement
         * for shader constant buffers.
         * @param onStartedUpdatingResource  Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         */
        void prepareDataForBindingToShaderCpuReadWriteResource(
            const std::string& sShaderResourceName,
            size_t iResourceSizeInBytes,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Looks for binding created using @ref prepareDataForBindingToShaderCpuReadWriteResource and
         * notifies the engine that there is new (updated) data for shader CPU read/write resource to copy.
         *
         * @remark You don't need to check if the node is spawned or not before calling this function,
         * if the binding does not exist this call will be ignored.
         *
         * @param sShaderResourceName Name of the shader CPU read/write resource (should be exactly the same
         * as the resource name written in the shader file we are referencing).
         */
        void markShaderCpuReadWriteResourceAsNeedsUpdate(const std::string& sShaderResourceName);

    private:
        /**
         * Allocates shader resources (see @ref mtxGpuResources).
         *
         * @warning Expects @ref pMaterial to have initialized PSO.
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
        void* onStartUpdatingShaderMeshConstants();

        /** Called after finished copying data from @ref mtxShaderMeshDataConstants. */
        void onFinishedUpdatingShaderMeshConstants();

        /** Used material. Always contains a valid pointer. */
        RPROPERTY(Serialize)
        std::shared_ptr<Material> pMaterial;

        /**
         * Mesh geometry.
         *
         * @warning Use with @ref mtxMeshData;
         */
        RPROPERTY(Serialize(
            FieldSerializationType::AS_EXTERNAL_FILE)) // allow VCSs to treat this file in a special way
        MeshData meshData; // don't change this field's name (no backwards compatibility in deserialization)

        /** Mutex for @ref meshData. */
        std::recursive_mutex mtxMeshData;

        /** Stores @ref meshData in GPU memory.  */
        std::pair<std::recursive_mutex, GpuResources> mtxGpuResources;

        /** Stores data for constant buffer used by shaders. */
        std::pair<std::recursive_mutex, MeshShaderConstants> mtxShaderMeshDataConstants;

        /** Whether mesh is visible or not. */
        bool bIsVisible = true;

        /** Name of the constant buffer used to store general mesh data in shaders. */
        static inline const auto sMeshShaderConstantBufferName = "meshData";

        /** Name of the category used for logging. */
        static inline const auto sMeshNodeLogCategory = "Mesh Node";

        ne_MeshNode_GENERATED
    };
} // namespace ne RNAMESPACE()

File_MeshNode_GENERATED
