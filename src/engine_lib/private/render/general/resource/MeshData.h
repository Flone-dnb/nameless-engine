#pragma once

// Custom.
#include "math/GLMath.hpp"
#include "io/Serializable.h"

#include "MeshData.generated.h"

namespace ne RNAMESPACE() {
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

    /**
     * Vertex of a mesh.
     *
     * @remark Equal to the vertex struct we use in shaders.
     */
    struct MeshVertex { // not using inheritance to avoid extra fields that are not related to vertex
        MeshVertex() = default;
        ~MeshVertex() = default;

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

        // --------------------------------------------------------------------------------------

        /** Position of the vertex in a 3D space. */
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);

        /** Normal vector of the vertex. */
        glm::vec3 normal = glm::vec3(0.0f, 0.0f, 0.0f);

        /** UV coordinates of the vertex. */
        glm::vec2 uv = glm::vec2(0.0f, 0.0f);

        // --------------------------------------------------------------------------------------

        // ! only vertex related fields (same as in shader) can be added here !
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
         * Returns array of mesh indices per material slot so first element in the array
         * stores indices of the mesh that use material slot 0, then indices that use material
         * slot 1 and so on.
         *
         * @return Mesh indices.
         */
        std::vector<std::vector<meshindex_t>>* getIndices();

    private:
        /** Mesh vertices. */
        RPROPERTY(Serialize)
        std::vector<MeshVertex> vVertices;

        /**
         * Stores array of mesh indices per material slot so first element in the array
         * stores indices of the mesh that use material slot 0, then indices that use material
         * slot 1 and so on.
         *
         * @remark This array defines how much material slots will be available.
         */
        RPROPERTY(Serialize)
        std::vector<std::vector<meshindex_t>> vIndices;

        ne_MeshData_GENERATED
    };
}

File_MeshData_GENERATED
