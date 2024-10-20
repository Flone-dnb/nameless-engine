#pragma once

// Custom.
#include "shader/glsl/format/GlslVertexFormatDescription.h"

namespace ne {
    /** Describes vertex format used by MeshNode. */
    class MeshNodeGlslVertexFormatDescription : public GlslVertexFormatDescription {
    public:
        MeshNodeGlslVertexFormatDescription() = default;
        virtual ~MeshNodeGlslVertexFormatDescription() override = default;

        /**
         * Returns an array of macros (related to vertex format) used in GLSL shader.
         *
         * @return Array of macro names where index in the array means binding location (index).
         */
        virtual std::vector<std::string> getVertexLayoutBindingIndexMacros() override;

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
