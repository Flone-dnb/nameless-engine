#pragma once

// Custom.
#include "game/nodes/MeshNode.h"

namespace ne {
    /** Provides static functions */
    class PrimitiveMeshGenerator {
    public:
        PrimitiveMeshGenerator() = delete;

        /**
         * Creates a cube mesh.
         *
         * @param size Size of the cube.
         *
         * @return Cube mesh.
         */
        static MeshData createCube(float size);
    };
} // namespace ne
