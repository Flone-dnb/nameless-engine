#include "DescriptorSetLayoutGenerator.h"

// External.
#include "spirv_reflect.h"
#include "fmt/core.h"

namespace ne {

    std::variant<std::unordered_map<uint32_t, DescriptorSetLayoutBindingInfo>, Error>
    DescriptorSetLayoutGenerator::collectInfoFromBytecode(void* pSpirvBytecode, size_t iSizeInBytes) {
        SpvReflectShaderModule module;
        // Create shader module.
        auto result = spvReflectCreateShaderModule(iSizeInBytes, pSpirvBytecode, &module);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return Error(fmt::format("failed to create shader module, error: {}", static_cast<int>(result)));
        }

        // Get descriptor binding count.
        uint32_t iDescriptorBindingCount = 0;
        result = spvReflectEnumerateDescriptorBindings(&module, &iDescriptorBindingCount, nullptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return Error(fmt::format(
                "failed to get shader descriptor binding count, error: {}", static_cast<int>(result)));
        }

        // Get descriptor bindings.
        std::vector<SpvReflectDescriptorBinding*> vDescriptorBindings(iDescriptorBindingCount);
        result = spvReflectEnumerateDescriptorBindings(
            &module, &iDescriptorBindingCount, vDescriptorBindings.data());
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return Error(
                fmt::format("failed to get shader descriptor bindings, error: {}", static_cast<int>(result)));
        }

        std::unordered_map<uint32_t, DescriptorSetLayoutBindingInfo> bindingInfo;
        for (const auto& descriptorBinding : vDescriptorBindings) {
            // Make sure there was no binding with this ID.
            auto it = bindingInfo.find(descriptorBinding->binding);
            if (it != bindingInfo.end()) [[unlikely]] {
                return Error(fmt::format(
                    "found two resources that use the same binding index {}, these are: \"{}\" and \"{}\"",
                    it->first,
                    it->second.sResourceName,
                    descriptorBinding->name));
            }

            // Collect new binding info.
            DescriptorSetLayoutBindingInfo info;
            info.sResourceName = descriptorBinding->name;
            switch (descriptorBinding->descriptor_type) {
            case (SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER): {
                info.resourceType = GlslResourceType::COMBINED_SAMPLER;
                break;
            }
            case (SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER): {
                info.resourceType = GlslResourceType::UNIFORM_BUFFER;
                break;
            }
            case (SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER): {
                info.resourceType = GlslResourceType::STORAGE_BUFFER;
                break;
            }
            default: {
                return Error(
                    fmt::format("type the resource \"{}\" is not supported", descriptorBinding->name));
                break;
            }
            }

            // Add to output.
            bindingInfo[descriptorBinding->binding] = std::move(info);
        }

        // Destroy shader module.
        spvReflectDestroyShaderModule(&module);

        return bindingInfo;
    }

} // namespace ne
