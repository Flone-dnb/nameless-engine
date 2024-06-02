#pragma once

// Standard.
#include <vector>
#include <string>

// Custom.
#include "shader/general/formats/VertexFormat.h"

// External.
#include "directx/d3dx12.h"

namespace ne {
    /** Describes a vertex format for DirectX/HLSL. */
    class HlslVertexFormatDescription : public VertexFormatDescription {
    public:
        HlslVertexFormatDescription() = default;
        virtual ~HlslVertexFormatDescription() override = default;

        /**
         * Creates vertex format description.
         *
         * @param type Type of the vertex format.
         *
         * @return Vertex format description.
         */
        static std::unique_ptr<HlslVertexFormatDescription> createDescription(VertexFormat type);

        /**
         * Returns information about vertex semantics used by shaders.
         *
         * @return Array of semantics where index (in the array) describes semantic location.
         */
        virtual std::vector<std::string> getVertexSemanticLocations() = 0;

        /**
         * Returns shader input layout description (vertex attribute description).
         *
         * @return Input layout description.
         */
        virtual std::vector<D3D12_INPUT_ELEMENT_DESC> getShaderInputElementDescription() = 0;
    };
}
