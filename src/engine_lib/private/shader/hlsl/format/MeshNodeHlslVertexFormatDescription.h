#pragma once

// Standard.
#include <vector>

// Custom.
#include "HlslVertexFormatDescription.h"

namespace ne {
    /** Describes vertex format used by MeshNode. */
    class MeshNodeHlslVertexFormatDescription : public HlslVertexFormatDescription {
    public:
        MeshNodeHlslVertexFormatDescription() = default;
        virtual ~MeshNodeHlslVertexFormatDescription() override = default;

        /**
         * Returns information about vertex semantics used by shaders.
         *
         * @return Array of semantics where index (in the array) describes semantic location.
         */
        virtual std::vector<std::string> getVertexSemanticLocations() override;

        /**
         * Returns shader input layout description (vertex attribute description).
         *
         * @return Input layout description.
         */
        virtual std::vector<D3D12_INPUT_ELEMENT_DESC> getShaderInputElementDescription() override;
    };
}
