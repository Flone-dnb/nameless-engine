#pragma once

// Standard.
#include <variant>
#include <vector>
#include <unordered_map>

// Custom.
#include "misc/Error.h"

namespace ne {
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
        DescriptorSetLayoutGenerator() = delete;
        DescriptorSetLayoutGenerator(const DescriptorSetLayoutGenerator&) = delete;
        DescriptorSetLayoutGenerator& operator=(const DescriptorSetLayoutGenerator&) = delete;

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
    };
} // namespace ne
