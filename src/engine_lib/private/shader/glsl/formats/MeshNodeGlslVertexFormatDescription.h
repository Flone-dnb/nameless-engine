#pragma once

// Custom.
#include "shader/glsl/formats/GlslVertexFormatDescription.h"

namespace ne {
    /** Describes vertex format used by MeshNode. */
    class MeshNodeGlslVertexFormatDescription : public GlslVertexFormatDescription {
    public:
        MeshNodeGlslVertexFormatDescription() = default;
        virtual ~MeshNodeGlslVertexFormatDescription() override = default;

        /**
         * Returns vertex description for vertex input binding.
         *
         * @return Vertex input binding description.
         */
        virtual VkVertexInputBindingDescription getVertexBindingDescription() override;

        /**
         * Returns description of all vertex attributes.
         *
         * @return Vertex attribute descriptions.
         */
        virtual std::vector<VkVertexInputAttributeDescription> getVertexAttributeDescriptions() override;
    };
}
