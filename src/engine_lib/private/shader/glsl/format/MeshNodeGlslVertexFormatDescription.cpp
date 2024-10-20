#include "MeshNodeGlslVertexFormatDescription.h"

// Custom.
#include "game/nodes/MeshNode.h"

namespace ne {

    std::vector<std::string> MeshNodeGlslVertexFormatDescription::getVertexLayoutBindingIndexMacros() {
        // Make sure vertex attributes don't need an update.
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "vertex binding indices needs to be updated");

        std::vector<std::string> vMacros;

        static_assert(offsetof(MeshVertex, position) == 0, "update push_back order");
        vMacros.push_back("VERTEX_LAYOUT_POS_BINDING_INDEX");

        static_assert(offsetof(MeshVertex, normal) == 12, "update push_back order");
        vMacros.push_back("VERTEX_LAYOUT_NORMAL_BINDING_INDEX");

        static_assert(offsetof(MeshVertex, uv) == 24, "update push_back order");
        vMacros.push_back("VERTEX_LAYOUT_UV_BINDING_INDEX");

        return vMacros;
    }

    VkVertexInputBindingDescription MeshNodeGlslVertexFormatDescription::getVertexBindingDescription() {
        // Make sure vertex attributes don't need an update.
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "vertex bindings need to be updated");

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = getVertexBindingIndex();
        bindingDescription.stride = sizeof(MeshVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    std::vector<VkVertexInputAttributeDescription>
    MeshNodeGlslVertexFormatDescription::getVertexAttributeDescriptions() {
        // Prepare some constants.
        constexpr VkFormat vec3Format = VK_FORMAT_R32G32B32_SFLOAT;
        constexpr VkFormat vec2Format = VK_FORMAT_R32G32_SFLOAT;

        static_assert(offsetof(MeshVertex, position) == 0, "update attribute offset");
        constexpr uint32_t iPositionAttributeOffset = 0;

        static_assert(offsetof(MeshVertex, normal) == 12, "update attribute offset");
        constexpr uint32_t iNormalAttributeOffset = 1;

        static_assert(offsetof(MeshVertex, uv) == 24, "update attribute offset");
        constexpr uint32_t iUvAttributeOffset = 2;

        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "vertex attribute count need to be updated");
        std::vector<VkVertexInputAttributeDescription> vAttributeDescriptions(3);

        // Describe position attribute.
        auto& positionAttribute = vAttributeDescriptions[iPositionAttributeOffset];
        positionAttribute.binding = getVertexBindingIndex();
        positionAttribute.location = iPositionAttributeOffset;
        positionAttribute.format = vec3Format;
        positionAttribute.offset = offsetof(MeshVertex, position);

        // Describe normal attribute.
        auto& normalAttribute = vAttributeDescriptions[iNormalAttributeOffset];
        normalAttribute.binding = getVertexBindingIndex();
        normalAttribute.location = iNormalAttributeOffset;
        normalAttribute.format = vec3Format;
        normalAttribute.offset = offsetof(MeshVertex, normal);

        // Describe UV attribute.
        auto& uvAttribute = vAttributeDescriptions[iUvAttributeOffset];
        uvAttribute.binding = getVertexBindingIndex();
        uvAttribute.location = iUvAttributeOffset;
        uvAttribute.format = vec2Format;
        uvAttribute.offset = offsetof(MeshVertex, uv);

        return vAttributeDescriptions;
    }

}
