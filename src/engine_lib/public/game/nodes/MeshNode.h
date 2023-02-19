#pragma once

// Standard.
#include <atomic>

// Custom.
#include "game/nodes/SpatialNode.h"
#include "math/GLMath.hpp"
#include "render/general/resources/GpuResource.h"
#include "render/general/resources/UploadBuffer.h"

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

        /**
         * Position of the vertex in a 3D space.
         *
         * @remark Using `vec4` instead of `vec3` because of the enabled GLM type alignment, so
         * `vec3` would take the same amount of space as `vec4` but with `vec4` we allow using
         * an extra `float` for custom purposes.
         */
        glm::vec4 position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

        /**
         * UV coordinates of the vertex.
         *
         * @remark Using `vec4` instead of `vec2` for the same reason as @ref position.
         */
        glm::vec4 uv = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

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

    /** Represents a node that can have 3D geometry to display (mesh). */
    class RCLASS(Guid("d5407ca4-3c2e-4a5a-9ff3-1262b6a4d264")) MeshNode : public SpatialNode {
        // Renderer will call `draw` on this node.
        friend class Renderer;

    public:
        /** Groups GPU buffers that store mesh data. */
        struct GeometryBuffers {
            /** Stores mesh vertices. */
            std::unique_ptr<GpuResource> pVertexBuffer;

            /** Stores mesh indices. */
            std::unique_ptr<GpuResource> pIndexBuffer;
        };

        /** Stores GPU resources used by shaders. */
        struct ShaderConstants {
            /**
             * Buffer with CPU access that contains N elements with object-specific data
             * (@ref MeshShaderConstants). Amount of elements in this buffer is defined by the amount
             * of frame resources.
             */
            std::unique_ptr<UploadBuffer> pConstantBuffers;

            /** Number of elements in @ref pConstantBuffers that needs to be updated with new data. */
            std::atomic<unsigned int> iFrameResourceCountToUpdate{0};
        };

        /** Constants used by shaders. */
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
         * Returns mesh geometry.
         *
         * @warning Must be used with mutex.
         *
         * @warning If you're changing mesh data you must call @ref onMeshDataChanged after you
         * finished modifying the mesh data to update internal CPU/GPU resources and see updated geometry
         * on the screen, otherwise this might cause the object to be displayed incorrectly.
         *
         * @return Mesh data.
         */
        std::pair<std::recursive_mutex*, MeshData*> getMeshData();

        /**
         * Returns GPU resources that store mesh geometry.
         *
         * @return Geometry buffers.
         */
        inline std::pair<std::mutex, GeometryBuffers>* getGeometryBuffers() { return &mtxGeometryBuffers; }

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
        virtual void onSpawn() override;

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
        virtual void onDespawn() override;

        /**
         * Called after node's world location/rotation/scale was changed.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onWorldLocationRotationScaleChanged() override;

    private:
        /** Allocates @ref mtxShaderConstantBuffers to be used by shaders. */
        void allocateShaderConstantBuffers();

        /** Allocates @ref mtxGeometryBuffers to be used by renderer. */
        void allocateGeometryBuffers();

        /** Deallocates @ref mtxShaderConstantBuffers. */
        void deallocateShaderConstantBuffers();

        /** Deallocates @ref mtxGeometryBuffers. */
        void deallocateGeometryBuffers();

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

        /** Mutex to use with @ref meshData. */
        std::recursive_mutex mtxMeshData;

        /** Stores @ref meshData in GPU memory.  */
        std::pair<std::mutex, GeometryBuffers> mtxGeometryBuffers;

        /** Stores GPU resources used by shaders. */
        std::pair<std::mutex, ShaderConstants> mtxShaderConstantBuffers;

        /** Whether mesh is visible or not. */
        bool bIsVisible = true;

        /** Name of the category used for logging. */
        static inline const auto sMeshNodeLogCategory = "Mesh Node";

        ne_MeshNode_GENERATED
    };
} // namespace ne RNAMESPACE()

File_MeshNode_GENERATED
