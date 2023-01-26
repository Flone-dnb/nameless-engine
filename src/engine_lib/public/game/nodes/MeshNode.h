#pragma once

// Standard.
#include <atomic>

// Custom.
#include "game/nodes/SpatialNode.h"
#include "math/GLMath.hpp"
#include "render/general/resources/UploadBuffer.h"

#include "MeshNode.generated.h"

namespace ne RNAMESPACE() {
    class Material;
    class GpuResource;
    class UploadBuffer;
    class GpuCommandList;

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

        /** Position of the vertex in a 3D space. */
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);

        /** UV coordinates of the vertex. */
        glm::vec2 uv = glm::vec2(0.0f, 0.0f);

        // ! only vertex related fields (same as in shader) can be added here !
        // ! add new fields to `serializeVec`, `deserializeVec` and `operator==` !
        // ! and to unit test !
        // (not deriving from `Serializable` to avoid extra fields that are not related to vertex)
    };

    /** Stores mesh geometry (vertices and indices). */
    class RCLASS(Guid("b60e4b47-b1e6-4001-87a8-b7885b4e8383")) MeshData : public Serializable {
    public:
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
        std::vector<unsigned int>* getIndices();

    private:
        /** Mesh vertices. */
        RPROPERTY(Serialize)
        std::vector<MeshVertex> vVertices;

        /** Stores mesh indices. */
        RPROPERTY(Serialize)
        std::vector<unsigned int> vIndices;

        ne_MeshData_GENERATED
    };

    /** Represents a node that can have 3D geometry to display (mesh). */
    class RCLASS(Guid("d5407ca4-3c2e-4a5a-9ff3-1262b6a4d264")) MeshNode : public SpatialNode {
    public:
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

        /**
         * Returns material used by this name.
         *
         * @return Material.
         */
        std::shared_ptr<Material> getMaterial() const;

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
         * Must be called after mesh data was modified to (re)create internal CPU/GPU resources.
         */
        void onMeshDataChanged();

    protected:
        // Renderer will call `draw` on this node.
        friend class Renderer;

        /** Constants used by shaders. */
        struct MeshShaderConstants {
            /** World matrix. */
            glm::mat4x4 world = glm::identity<glm::mat4x4>();

            // remember to add padding to 4 floats
        };

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

        /**
         * Called by renderer to add draw commands (that will draw this mesh) to the command list.
         *
         * @param pCommandList               Command list to add draw commands to.
         * @param iCurrentFrameResourceIndex Index of currently used frame resource.
         */
        void draw(GpuCommandList* pCommandList, unsigned int iCurrentFrameResourceIndex);

    private:
        /** Used material. Always contains a valid pointer. */
        RPROPERTY(Serialize)
        std::shared_ptr<Material> pMaterial;

        /**
         * Mesh geometry.
         *
         * @warning Use with @ref mtxMeshData;
         */
        RPROPERTY(SerializeAsExternal) // allow VCSs to treat this file in a special way
        MeshData meshData; // don't change this field's name (no backwards compatibility in deserialization)

        /** Mutex to use with @ref meshData. */
        std::recursive_mutex mtxMeshData;

        /**
         * Buffer with CPU access that contains N elements with object-specific data
         * (@ref MeshShaderConstants). Amount of elements in this buffer is defined by the amount
         * of frame resources.
         */
        std::unique_ptr<UploadBuffer> pShaderConstantBuffers;

        /** Number of elements in @ref pShaderConstantBuffers that needs to be updated with new data. */
        std::atomic<unsigned int> iFrameResourceCountToUpdate{0};

        ne_MeshNode_GENERATED
    };
} // namespace ne RNAMESPACE()

File_MeshNode_GENERATED
