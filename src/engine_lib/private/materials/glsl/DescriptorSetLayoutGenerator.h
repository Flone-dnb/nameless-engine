#pragma once

// Standard.
#include <variant>
#include <vector>
#include <unordered_map>
#include <array>

// Custom.
#include "misc/Error.h"
#include "render/general/resources/FrameResourcesManager.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    class GlslShader;
    class Renderer;

    /** Describes type of a resource that was written in the GLSL code. */
    enum class GlslResourceType {
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        COMBINED_SAMPLER,
        // ... add new resource types here ...
    };

    /** Contains information about a descriptor set layout binding. */
    struct DescriptorSetLayoutBindingInfo {
        /** Type of the shader resource. */
        GlslResourceType resourceType;

        /** Name of the resource (written in the GLSL code). */
        std::string sResourceName;
    };

    /** Generates Descriptor Set Layout based on GLSL code. */
    class DescriptorSetLayoutGenerator {
    public:
        /** Groups generated data. */
        struct Generated {
            /** Created descriptor set layout. */
            VkDescriptorSetLayout pDescriptorSetLayout = nullptr;

            /** Created descriptor pool. */
            VkDescriptorPool pDescriptorPool = nullptr;

            /** Created descriptor set per each frame resource. */
            std::array<VkDescriptorSet, FrameResourcesManager::getFrameResourcesCount()> vDescriptorSets;

            /**
             * Map of pairs "resource name" (from GLSL code) - "layout binding index".
             *
             * @remark Binding index in the map reference descriptor sets from @ref vDescriptorSets.
             *
             * @remark Generally used to bind/update data of some GLSL resource to a specific
             * descriptor in a descriptor set.
             */
            std::unordered_map<std::string, uint32_t> resourceBindings;
        };

        DescriptorSetLayoutGenerator() = delete;
        DescriptorSetLayoutGenerator(const DescriptorSetLayoutGenerator&) = delete;
        DescriptorSetLayoutGenerator& operator=(const DescriptorSetLayoutGenerator&) = delete;

        /**
         * Returns binding index that shaders should use for "frameData" uniform buffer.
         *
         * @return Binding index in descriptor set layout.
         */
        static constexpr uint32_t getFrameUniformBufferBindingIndex() {
            return iFrameUniformBufferBindingIndex;
        }

        /**
         * Collection information from the specified SPIR-V bytecode that can be used to generate
         * a descriptor set layout.
         *
         * @param pSpirvBytecode SPIR-V bytecode to analyze.
         * @param iSizeInBytes   Size of bytecode in bytes.
         *
         * @return Error if something went wrong, otherwise a map of descriptor set layout binding
         * where key is binding index and value is descriptor info.
         */
        static std::variant<std::unordered_map<uint32_t, DescriptorSetLayoutBindingInfo>, Error>
        collectInfoFromBytecode(void* pSpirvBytecode, size_t iSizeInBytes);

        /**
         * Generates a new descriptor layout, pool and descriptor sets using the specified vertex and
         * fragment shaders.
         *
         * @remark Expects that descriptor layout information is already collected for both
         * shaders (see @ref collectInfoFromBytecode), otherwise returns error.
         *
         * @param pRenderer       Current renderer.
         * @param pVertexShader   Vertex shader.
         * @param pFragmentShader Fragment shader.
         *
         * @return Error if something went wrong, otherwise generated descriptor layout data.
         */
        static std::variant<Generated, Error>
        generate(Renderer* pRenderer, GlslShader* pVertexShader, GlslShader* pFragmentShader);

    private:
        /**
         * Generates Vulkan layout binding that could be used to create a descriptor set layout.
         *
         * @param iBindingIndex Index of the binding that was specified in the GLSL code.
         * @param bindingInfo   Information about the GLSL resource used in this binding.
         *
         * @return
         */
        static std::variant<VkDescriptorSetLayoutBinding, Error>
        generateLayoutBinding(uint32_t iBindingIndex, const DescriptorSetLayoutBindingInfo& bindingInfo);

        /** Name of the `uniform` buffer used to store frame data in GLSL shaders. */
        static inline const std::string sFrameUniformBufferName = "frameData";

        /** Binding index that shaders should use for "frameData" uniform buffer. */
        static constexpr uint32_t iFrameUniformBufferBindingIndex = 0;
    };
} // namespace ne
