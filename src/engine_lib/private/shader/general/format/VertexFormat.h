#pragma once

// Standard.
#include <memory>

namespace ne {
    class Renderer;

    /** Stores available (that can be used) vertex format types. */
    enum class VertexFormat {
        MESH_NODE, //< Vertex format used by mesh nodes.
    };

    /** Base class to describe information about a vertex format. */
    class VertexFormatDescription {
    public:
        VertexFormatDescription() = default;
        virtual ~VertexFormatDescription() = default;

        /**
         * Creates vertex format description.
         *
         * @param type      Type of the vertex format.
         * @param pRenderer Used renderer.
         *
         * @return Vertex format description.
         */
        static std::unique_ptr<VertexFormatDescription>
        createDescription(VertexFormat type, Renderer* pRenderer);
    };
}
