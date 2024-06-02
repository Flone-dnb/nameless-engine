#pragma once

// Standard.
#include <vector>
#include <string>

// Custom.
#include "shader/general/formats/VertexFormat.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    /** Describes a vertex format for Vulkan/GLSL. */
    class GlslVertexFormatDescription : public VertexFormatDescription {
    public:
        GlslVertexFormatDescription() = default;
        virtual ~GlslVertexFormatDescription() override = default;

        /**
         * Creates vertex format description.
         *
         * @param type Type of the vertex format.
         *
         * @return Vertex format description.
         */
        static std::unique_ptr<GlslVertexFormatDescription> createDescription(VertexFormat type);

        /**
         * Returns an array of macros (related to vertex format) used in GLSL shader.
         *
         * @return Array of macro names where index in the array means binding location (index).
         */
        virtual std::vector<std::string> getVertexLayoutBindingIndexMacros() = 0;

        /**
         * Returns vertex description for vertex input binding.
         *
         * @return Vertex input binding description.
         */
        virtual VkVertexInputBindingDescription getVertexBindingDescription() = 0;

        /**
         * Returns description of all vertex attributes.
         *
         * @return Vertex attribute descriptions.
         */
        virtual std::vector<VkVertexInputAttributeDescription> getVertexAttributeDescriptions() = 0;

    protected:
        /**
         * Returns index of the vertex input binding.
         *
         * @return Binding index.
         */
        static constexpr uint32_t getVertexBindingIndex() { return iVertexBindingIndex; }

    private:
        /** Index of the vertex input binding. */
        static constexpr uint32_t iVertexBindingIndex = 0;
    };
}
