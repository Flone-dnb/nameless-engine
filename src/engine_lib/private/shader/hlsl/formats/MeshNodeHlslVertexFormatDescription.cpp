#include "MeshNodeHlslVertexFormatDescription.h"

// Custom.
#include "game/nodes/MeshNode.h"

namespace ne {

    std::vector<std::string> MeshNodeHlslVertexFormatDescription::getVertexSemanticLocations() {
        // Check if vertex description needs to be updated.
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "vertex semantic locations needs to be updated");

        std::vector<std::string> vVertexSemantics;

        static_assert(offsetof(MeshVertex, position) == 0, "update semantic order (index in array)");
        vVertexSemantics.push_back("POSITION");

        static_assert(offsetof(MeshVertex, normal) == 12, "update semantic order (index in array)");
        vVertexSemantics.push_back("NORMAL");

        static_assert(offsetof(MeshVertex, uv) == 24, "update semantic order (index in array)");
        vVertexSemantics.push_back("UV");

        return vVertexSemantics;
    }

    std::vector<D3D12_INPUT_ELEMENT_DESC> MeshNodeHlslVertexFormatDescription::getShaderInputElementDescription() {
        // Check if vertex description needs to be updated.
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "vertex layout needs to be updated");

        return {
            {"POSITION",
             0,
             DXGI_FORMAT_R32G32B32_FLOAT,
             0,
             offsetof(MeshVertex, position),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
             0},
            {"NORMAL",
             0,
             DXGI_FORMAT_R32G32B32_FLOAT,
             0,
             offsetof(MeshVertex, normal),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
             0},
            {"UV",
             0,
             DXGI_FORMAT_R32G32_FLOAT,
             0,
             offsetof(MeshVertex, uv),
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
             0}};
    }

}
