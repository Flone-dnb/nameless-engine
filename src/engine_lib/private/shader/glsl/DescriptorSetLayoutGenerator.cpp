#include "DescriptorSetLayoutGenerator.h"

// Standard.
#include <format>

// Custom.
#include "shader/glsl/GlslShader.h"
#include "render/vulkan/VulkanRenderer.h"
#include "misc/Profiler.hpp"
#include "shader/general/DescriptorConstants.hpp"

// External.
#include "spirv_reflect.h"
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    std::variant<DescriptorSetLayoutGenerator::Collected, Error>
    DescriptorSetLayoutGenerator::collectInfoFromBytecode(void* pSpirvBytecode, size_t iSizeInBytes) {
        PROFILE_FUNC;

        // Create shader module.
        SpvReflectShaderModule module;
        auto result = spvReflectCreateShaderModule(iSizeInBytes, pSpirvBytecode, &module);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return Error(std::format("failed to create shader module, error: {}", static_cast<int>(result)));
        }

        // Get descriptor binding count.
        uint32_t iDescriptorBindingCount = 0;
        result = spvReflectEnumerateDescriptorBindings(&module, &iDescriptorBindingCount, nullptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return Error(std::format(
                "failed to get shader descriptor binding count, error: {}", static_cast<int>(result)));
        }

        // Get descriptor bindings.
        std::vector<SpvReflectDescriptorBinding*> vDescriptorBindings(iDescriptorBindingCount);
        result = spvReflectEnumerateDescriptorBindings(
            &module, &iDescriptorBindingCount, vDescriptorBindings.data());
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return Error(
                std::format("failed to get shader descriptor bindings, error: {}", static_cast<int>(result)));
        }

        std::unordered_set<std::string> usedNames;
        Collected collected;
        for (const auto& descriptorBinding : vDescriptorBindings) {
            // Make sure binding name is valid.
            if (descriptorBinding->name == nullptr) [[unlikely]] {
                return Error(std::format(
                    "found {} binding(s) but one has unexpected `nullptr` name (was debug info generated "
                    "during compilation?)",
                    vDescriptorBindings.size()));
            }

            // Make sure there was no binding with this ID.
            auto bindingInfoIt = collected.bindingInfo.find(descriptorBinding->binding);
            if (bindingInfoIt != collected.bindingInfo.end()) [[unlikely]] {
                return Error(std::format(
                    "found two resources that use the same binding index {}, these are: \"{}\" and \"{}\"",
                    bindingInfoIt->first,
                    bindingInfoIt->second.sResourceName,
                    descriptorBinding->name));
            }

            // Make sure this resource name was not used yet because we will use names to
            // differentiate resources in the engine.
            auto usedNamesIt = usedNames.find(descriptorBinding->name);
            if (usedNamesIt != usedNames.end()) [[unlikely]] {
                return Error(std::format(
                    "found two resources that have the same name \"{}\"", descriptorBinding->name));
            }

            // Collect new binding info.
            Collected::DescriptorSetLayoutBindingInfo info;
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
            case (SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE): {
                info.resourceType = GlslResourceType::STORAGE_IMAGE;
                break;
            }
            default: {
                return Error(
                    std::format("type the resource \"{}\" is not supported", descriptorBinding->name));
                break;
            }
            }

            // Add to output.
            collected.bindingInfo[descriptorBinding->binding] = std::move(info);
        }

        // Get push constants count.
        uint32_t iPushConstantCount = 0;
        result = spvReflectEnumeratePushConstantBlocks(&module, &iPushConstantCount, nullptr);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return Error(
                std::format("failed to get shader push constant count, error: {}", static_cast<int>(result)));
        }

        // Process push constants here instead of doing this in pipeline to speed up pipeline generation.
        if (iPushConstantCount != 0) {
            // Get push constants.
            std::vector<SpvReflectBlockVariable*> vPushConstants(iPushConstantCount);
            result =
                spvReflectEnumeratePushConstantBlocks(&module, &iPushConstantCount, vPushConstants.data());
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                return Error(
                    std::format("failed to get shader push constants, error: {}", static_cast<int>(result)));
            }

            // Make sure there is only 1 (as only 1 is allowed).
            if (vPushConstants.size() > 1) [[unlikely]] {
                return Error(
                    std::format("expected only 1 push constant but received {}", vPushConstants.size()));
            }

            const auto pPushConstant = vPushConstants[0];

            // Get members.
            std::vector<SpvReflectBlockVariable*> vMembers(pPushConstant->member_count);
            for (size_t i = 0; i < vMembers.size(); i++) {
                vMembers[i] = reinterpret_cast<SpvReflectBlockVariable*>(
                    reinterpret_cast<char*>(pPushConstant->members) + i * sizeof(*(vMembers[0])));
            }

            // Go through each field.
            std::unordered_map<std::string, size_t> uintFieldOffsets;
            for (const auto& memberInfo : vMembers) {
                // Make sure it's a `uint` indeed.
                if (memberInfo->size != sizeof(unsigned int)) [[unlikely]] {
                    return Error(std::format(
                        "found a non `uint` field in push constants named \"{}\" - not supported",
                        memberInfo->name));
                }
                if (memberInfo->type_description->op != SpvOpTypeInt) [[unlikely]] {
                    return Error(std::format(
                        "found a non `uint` field in push constants named \"{}\" - not supported",
                        memberInfo->name));
                }

                // Make sure its absolute offset is a multiple of 4 (sizeof uint).
                if (memberInfo->absolute_offset % 4 != 0) [[unlikely]] {
                    return Error(std::format(
                        "found a field in push constants named \"{}\" with absolute "
                        "offset not being multiple of 4 (absolute offset: {})",
                        memberInfo->name,
                        memberInfo->absolute_offset));
                }

                // Save info.
                uintFieldOffsets[memberInfo->name] = memberInfo->absolute_offset / 4;
            }

            // Save collected info.
            collected.pushConstantUintFieldOffsets = std::move(uintFieldOffsets);
        }

        // Destroy shader module.
        spvReflectDestroyShaderModule(&module);

        return collected;
    }

    std::variant<DescriptorSetLayoutGenerator::Generated, Error>
    DescriptorSetLayoutGenerator::generateGraphics( // NOLINT: TODO: make it simpler
        Renderer* pRenderer,
        GlslShader* pVertexShader,
        GlslShader* pFragmentShader) {
        PROFILE_FUNC;

        // Make sure we use Vulkan renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Make sure that the vertex shader is indeed a vertex shader.
        if (pVertexShader->getShaderType() != ShaderType::VERTEX_SHADER) [[unlikely]] {
            return Error(std::format(
                "the specified shader \"{}\" is not a vertex shader", pVertexShader->getShaderName()));
        }

        // Prepare the name (identifier) of the descriptor layout that we will create.
        std::string sCombinedShadersName = std::format("\"{}\"", pVertexShader->getShaderName());

        if (pFragmentShader != nullptr) {
            // Make sure that the fragment shader is indeed a fragment shader.
            if (pFragmentShader->getShaderType() != ShaderType::FRAGMENT_SHADER) [[unlikely]] {
                return Error(std::format(
                    "the specified shader \"{}\" is not a fragment shader",
                    pFragmentShader->getShaderName()));
            }

            sCombinedShadersName += std::format(" \"{}\"", pFragmentShader->getShaderName());
        }

        // Prepare a dummy mutex for fragment shader in case fragment shader is not specified in order
        // for scoped_lock below to work.
        std::mutex mtxDummy;
        std::mutex* pFragmentDescriptorLayoutInfoMutex = &mtxDummy;

        // Get shaders used descriptor layout info.
        std::pair<std::mutex, std::optional<DescriptorSetLayoutGenerator::Collected>>*
            pMtxFragmentDescriptorLayoutInfo = nullptr;
        if (pFragmentShader != nullptr) {
            pMtxFragmentDescriptorLayoutInfo = pFragmentShader->getDescriptorSetLayoutInfo();
            pFragmentDescriptorLayoutInfoMutex = &pMtxFragmentDescriptorLayoutInfo->first;
        }
        auto pMtxVertexDescriptorLayoutInfo = pVertexShader->getDescriptorSetLayoutInfo();

        // Lock info.
        std::scoped_lock shaderDescriptorLayoutInfoGuard(
            *pFragmentDescriptorLayoutInfoMutex, pMtxVertexDescriptorLayoutInfo->first);

        // Make sure it's not empty.
        if (pMtxFragmentDescriptorLayoutInfo != nullptr) {
            if (!pMtxFragmentDescriptorLayoutInfo->second.has_value()) [[unlikely]] {
                return Error(std::format(
                    "unable to merge descriptor layout of the fragment shader \"{}\" "
                    "because it does have descriptor layout info collected",
                    pFragmentShader->getShaderName()));
            }
        }
        if (!pMtxVertexDescriptorLayoutInfo->second.has_value()) [[unlikely]] {
            return Error(std::format(
                "unable to merge descriptor layout of the vertex shader \"{}\" "
                "because it does have descriptor layout info collected",
                pVertexShader->getShaderName()));
        }

        // Get layout info.
        DescriptorSetLayoutGenerator::Collected* pFragmentShaderDescriptorLayoutInfo = nullptr;
        if (pMtxFragmentDescriptorLayoutInfo != nullptr) {
            pFragmentShaderDescriptorLayoutInfo = &(*pMtxFragmentDescriptorLayoutInfo->second);
        }
        auto& vertexShaderDescriptorLayoutInfo = pMtxVertexDescriptorLayoutInfo->second.value();

        // Prepare some data to be used.
        struct LayoutBindingInfo {
            uint32_t iBindingIndex = 0;
            GlslResourceType descriptorType = GlslResourceType::COMBINED_SAMPLER;
        };
        std::vector<VkDescriptorSetLayoutBinding> vLayoutBindings; // will be used to create layout
        std::vector<VkDescriptorBindingFlags> vLayoutBindingFlags; // will be used to create layout
        std::unordered_map<std::string, LayoutBindingInfo>
            resourceBindings; // will be used for `Generated` data

        if (pFragmentShaderDescriptorLayoutInfo != nullptr) {
            // First, add all bindings used in fragment shader.
            for (const auto& [iBindingIndex, bindingInfo] :
                 pFragmentShaderDescriptorLayoutInfo->bindingInfo) {
                // Make sure we don't have a resource with this name already.
                auto bindingIt = resourceBindings.find(bindingInfo.sResourceName);
                if (bindingIt != resourceBindings.end()) [[unlikely]] {
                    return Error(std::format(
                        "fragment shader \"{}\" has two resources with the same name, "
                        "please make sure resource names are unique",
                        pFragmentShader->getShaderName()));
                }

                // Generate layout binding.
                auto layoutBindingResult = generateLayoutBinding(iBindingIndex, bindingInfo, false);
                if (std::holds_alternative<Error>(layoutBindingResult)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(layoutBindingResult));
                    error.addCurrentLocationToErrorStack();
                    return error;
                }

                // Add binding to be used in layout.
                auto [binding, bindingFlags] =
                    std::get<std::pair<VkDescriptorSetLayoutBinding, VkDescriptorBindingFlags>>(
                        layoutBindingResult);
                vLayoutBindings.push_back(binding);
                vLayoutBindingFlags.push_back(bindingFlags);

                // Save binding info.
                LayoutBindingInfo info;
                info.iBindingIndex = iBindingIndex;
                info.descriptorType = bindingInfo.resourceType;
                resourceBindings[bindingInfo.sResourceName] = info;
            }
        }

        // Now add all bindings used in vertex shader but avoid duplicates.
        for (const auto& [iBindingIndex, bindingInfo] : vertexShaderDescriptorLayoutInfo.bindingInfo) {
            // See if a resource with this name was already added.
            const auto alreadyDefinedBindingIt = resourceBindings.find(bindingInfo.sResourceName);
            if (alreadyDefinedBindingIt != resourceBindings.end()) {
                // Make sure both fragment/vertex resources use the same binding index.
                if (alreadyDefinedBindingIt->second.iBindingIndex != iBindingIndex) [[unlikely]] {
                    // Error, we need unique names: we have 2 resources with the same name but
                    // different binding indices we will not be able to differentiate them later.
                    return Error(std::format(
                        "vertex shader \"{}\" defines a resource named \"{}\" with binding index {} and "
                        "fragment shader \"{}\" also has a resource with this name but different binding "
                        "index {}, we will not be able to differentiate them since we use resource names "
                        "for that, please change the name of vertex or fragment shader resource so that "
                        "all resource names in vertex/fragment shader pairs will be unique",
                        pVertexShader->getShaderName(),
                        bindingInfo.sResourceName,
                        iBindingIndex,
                        pFragmentShader->getShaderName(),
                        alreadyDefinedBindingIt->second.iBindingIndex));
                }

                // We have 2 resources with the same name and they use the same binding index.
                // Make sure they have the same type.
                if (bindingInfo.resourceType != alreadyDefinedBindingIt->second.descriptorType) [[unlikely]] {
                    // Error, we need unique names: we have 2 resources with the same name and
                    // the same binding indices but with different types we will not be able to
                    // differentiate them later.
                    return Error(std::format(
                        "vertex shader \"{}\" defines a resource named \"{}\" with binding index {} and "
                        "fragment shader \"{}\" also has a resource with this name with the same binding "
                        "index but different type, we will not be able to differentiate them since we use "
                        "resource names for that, please change the name of vertex or fragment shader "
                        "resource so that all resource names in vertex/fragment shader pairs will be unique",
                        pVertexShader->getShaderName(),
                        bindingInfo.sResourceName,
                        iBindingIndex,
                        pFragmentShader->getShaderName()));
                }

                // OK: it seems to be a duplicated resource (that might be `frameData` for example)
                // ignore it as we already added it.
                continue;
            }

            if (pFragmentShaderDescriptorLayoutInfo != nullptr) {
                // See if this binding index is already used by some other resource.
                const auto fragmentBindingInfoIt =
                    pFragmentShaderDescriptorLayoutInfo->bindingInfo.find(iBindingIndex);
                if (fragmentBindingInfoIt != pFragmentShaderDescriptorLayoutInfo->bindingInfo.end())
                    [[unlikely]] {
                    // Error: this binding index is already in use by some other resource.
                    return Error(std::format(
                        "vertex shader \"{}\" defines a resource named \"{}\" with binding index {} but "
                        "this binding index is already being used by some other fragment shader \"{}\" "
                        "resource named \"{}\", because these resources have different names they are "
                        "considered different and should use different binding indices, please change "
                        "binding indices in vertex or fragment shader so that they will not conflict, "
                        "otherwise if these resources are the same (have the same type and used for the same"
                        "purpose) please make sure that these resources will have the same name in both "
                        "vertex and fragment shader",
                        pVertexShader->getShaderName(),
                        bindingInfo.sResourceName,
                        iBindingIndex,
                        pFragmentShader->getShaderName(),
                        fragmentBindingInfoIt->second.sResourceName));
                }
            }

            // Generate layout binding.
            auto layoutBindingResult = generateLayoutBinding(iBindingIndex, bindingInfo, false);
            if (std::holds_alternative<Error>(layoutBindingResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(layoutBindingResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Add binding to be used in layout.
            auto [binding, bindingFlags] =
                std::get<std::pair<VkDescriptorSetLayoutBinding, VkDescriptorBindingFlags>>(
                    layoutBindingResult);
            vLayoutBindings.push_back(binding);
            vLayoutBindingFlags.push_back(bindingFlags);

            // Save binding info.
            LayoutBindingInfo info;
            info.iBindingIndex = iBindingIndex;
            info.descriptorType = bindingInfo.resourceType;
            resourceBindings[bindingInfo.sResourceName] = info;
        }

        // Self check: make sure bindings and binding flags arrays have equal size.
        if (vLayoutBindings.size() != vLayoutBindingFlags.size()) [[unlikely]] {
            return Error(std::format(
                "layout binding array size {} is not equal to layout binding flags array size {} (this is a "
                "bug, report to developers",
                vLayoutBindings.size(),
                vLayoutBindingFlags.size()));
        }

        // Describe layout binding flags.
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
        bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlags.pNext = nullptr;
        bindingFlags.pBindingFlags = vLayoutBindingFlags.data();
        bindingFlags.bindingCount = static_cast<uint32_t>(vLayoutBindingFlags.size());

        // Check if some bindings use "update after bind" flag.
        bool bUseUpdateAfterBind = false;
        for (const auto& bindingFlags : vLayoutBindingFlags) {
            if ((bindingFlags & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) != 0) {
                bUseUpdateAfterBind = true;
                break;
            }
        }

        // Describe descriptor set layout.
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(vLayoutBindings.size());
        layoutInfo.pBindings = vLayoutBindings.data();

        // Specify layout flags.
        if (bUseUpdateAfterBind) {
            layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        }

        // Specify binding flags.
        layoutInfo.pNext = &bindingFlags;

        // Prepare output variable.
        Generated generatedData;

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be valid");
        }

        // Create descriptor set layout.
        auto result = vkCreateDescriptorSetLayout(
            pLogicalDevice, &layoutInfo, nullptr, &generatedData.pDescriptorSetLayout);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                std::format("failed to create descriptor set layout, error: {}", string_VkResult(result)));
        }

        // Name this descriptor set layout.
        VulkanRenderer::setObjectDebugOnlyName(
            pRenderer,
            generatedData.pDescriptorSetLayout,
            VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
            std::format("descriptor set layout {}", sCombinedShadersName));

        // Continue to create a descriptor pool.

        // Describe descriptor types that our descriptor sets will contain.
        std::vector<VkDescriptorPoolSize> vPoolSizes(vLayoutBindings.size());
        for (size_t i = 0; i < vPoolSizes.size(); i++) {
            auto& descriptorPoolSize = vPoolSizes[i];

            // Specify descriptor type.
            descriptorPoolSize.type = vLayoutBindings[i].descriptorType;

            // Specify how much descriptors of this type can be allocated using the pool.
            descriptorPoolSize.descriptorCount =
                vLayoutBindings[i].descriptorCount * FrameResourcesManager::getFrameResourcesCount();
        }

        // Describe descriptor pool.
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(vPoolSizes.size());
        poolInfo.pPoolSizes = vPoolSizes.data();

        // Specify flags.
        if (bUseUpdateAfterBind) {
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        }

        // Specify the total amount of descriptor sets that can be allocated from the pool.
        poolInfo.maxSets = FrameResourcesManager::getFrameResourcesCount();

        // Specify if `vkFreeDescriptorSets` is allowed to free individual descriptors.
        // (we are not going to free individual descriptors so we don't need it)
        // poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        // Create descriptor pool.
        result = vkCreateDescriptorPool(pLogicalDevice, &poolInfo, nullptr, &generatedData.pDescriptorPool);
        if (result != VK_SUCCESS) [[unlikely]] {
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedData.pDescriptorSetLayout, nullptr);
            return Error(std::format("failed to create descriptor pool, error: {}", string_VkResult(result)));
        }

        // Name this pool.
        VulkanRenderer::setObjectDebugOnlyName(
            pRenderer,
            generatedData.pDescriptorPool,
            VK_OBJECT_TYPE_DESCRIPTOR_POOL,
            std::format("descriptor pool {}", sCombinedShadersName));

        // Allocate descriptor sets.

        // Prepare layout for each descriptor set to create.
        std::array<VkDescriptorSetLayout, FrameResourcesManager::getFrameResourcesCount()>
            vDescriptorLayouts = {generatedData.pDescriptorSetLayout, generatedData.pDescriptorSetLayout};

        // Allocate descriptor sets.
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
        descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocInfo.descriptorPool = generatedData.pDescriptorPool;
        descriptorSetAllocInfo.descriptorSetCount = static_cast<uint32_t>(vDescriptorLayouts.size());
        descriptorSetAllocInfo.pSetLayouts = vDescriptorLayouts.data();

        // Make sure allocated descriptor set count will fit into our generated descriptor set array.
        if (descriptorSetAllocInfo.descriptorSetCount != generatedData.vDescriptorSets.size()) [[unlikely]] {
            vkDestroyDescriptorPool(pLogicalDevice, generatedData.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedData.pDescriptorSetLayout, nullptr);
            return Error(std::format(
                "attempting to allocate {} descriptor sets will fail because they will not fit into "
                "the generated array of descriptor sets with size {}",
                descriptorSetAllocInfo.descriptorSetCount,
                generatedData.vDescriptorSets.size()));
        }

        // Allocate descriptor sets.
        result = vkAllocateDescriptorSets(
            pLogicalDevice, &descriptorSetAllocInfo, generatedData.vDescriptorSets.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            vkDestroyDescriptorPool(pLogicalDevice, generatedData.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedData.pDescriptorSetLayout, nullptr);
            return Error(std::format("failed to create descriptor sets, error: {}", string_VkResult(result)));
        }

        // Fill resource bindings info.
        for (const auto& [sResourceName, bindingInfo] : resourceBindings) {
            generatedData.resourceBindings[sResourceName] = bindingInfo.iBindingIndex;
        }

        // Merge push constants (if used).
        std::unordered_map<std::string, size_t> pushConstantUintFieldOffsets =
            vertexShaderDescriptorLayoutInfo.pushConstantUintFieldOffsets;
        if (pFragmentShaderDescriptorLayoutInfo != nullptr) {
            for (const auto& [sFieldName, iOffsetInUints] :
                 pFragmentShaderDescriptorLayoutInfo->pushConstantUintFieldOffsets) {
                pushConstantUintFieldOffsets[sFieldName] = iOffsetInUints;
            }
        }

        // Make sure fields have unique offsets.
        std::unordered_map<size_t, std::string> fieldOffsets;
        for (const auto& [sFieldName, iOffsetInUints] : pushConstantUintFieldOffsets) {
            // Make sure this offset was not used before.
            const auto it = fieldOffsets.find(iOffsetInUints);
            if (it != fieldOffsets.end()) [[unlikely]] {
                return Error(std::format(
                    "found 2 fields in push constants with different names but the same "
                    "offsets from struct start, conflicting offset {} was already used on field \"{}\" but "
                    "the field \"{}\" is also using it, this might mean that your vertex and fragment "
                    "shaders use different push constants",
                    iOffsetInUints,
                    it->second,
                    sFieldName));
            }

            // Add offset as used.
            fieldOffsets[iOffsetInUints] = sFieldName;
        }

        // Save push constants.
        generatedData.pushConstantUintFieldOffsets = std::move(pushConstantUintFieldOffsets);

        return generatedData;
    }

    std::variant<DescriptorSetLayoutGenerator::Generated, Error>
    DescriptorSetLayoutGenerator::generateCompute(Renderer* pRenderer, GlslShader* pComputeShader) {
        PROFILE_FUNC;

        // Make sure we use Vulkan renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(pRenderer);
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Make sure that the compute shader is indeed a compute shader.
        if (pComputeShader->getShaderType() != ShaderType::COMPUTE_SHADER) [[unlikely]] {
            return Error(std::format(
                "the specified shader \"{}\" is not a compute shader", pComputeShader->getShaderName()));
        }

        // Get shader's descriptor layout info.
        auto pMtxDescriptorLayoutInfo = pComputeShader->getDescriptorSetLayoutInfo();
        std::scoped_lock shaderDescriptorLayoutInfoGuard(pMtxDescriptorLayoutInfo->first);

        // Make sure it's not empty.
        if (!pMtxDescriptorLayoutInfo->second.has_value()) [[unlikely]] {
            return Error(std::format(
                "unable to generate descriptor layout of the compute shader \"{}\" "
                "because it does have descriptor layout info collected",
                pComputeShader->getShaderName()));
        }

        // Get layout info.
        auto& descriptorLayoutInfo = pMtxDescriptorLayoutInfo->second.value();

        // Prepare some data to be used.
        struct LayoutBindingInfo {
            uint32_t iBindingIndex = 0;
            GlslResourceType descriptorType = GlslResourceType::COMBINED_SAMPLER;
        };
        std::vector<VkDescriptorSetLayoutBinding> vLayoutBindings; // will be used to create layout
        std::vector<VkDescriptorBindingFlags> vLayoutBindingFlags; // will be used to create layout
        std::unordered_map<std::string, LayoutBindingInfo>
            resourceBindings; // will be used for `Generated` data
        std::unordered_set<unsigned int> usedBindingIndices;

        // Add all bindings used in compute shader.
        for (const auto& [iBindingIndex, bindingInfo] : descriptorLayoutInfo.bindingInfo) {
            // Make sure we don't have a resource with this name already.
            auto bindingIt = resourceBindings.find(bindingInfo.sResourceName);
            if (bindingIt != resourceBindings.end()) [[unlikely]] {
                return Error(std::format(
                    "compute shader \"{}\" has two resources with the same name, "
                    "please make sure resource names are unique",
                    pComputeShader->getShaderName()));
            }

            // See if this binding index is already used by some other resource.
            const auto usedBindingIndexIt = usedBindingIndices.find(iBindingIndex);
            if (usedBindingIndexIt != usedBindingIndices.end()) [[unlikely]] {
                // Error: this binding index is already in use by some other resource.
                return Error(std::format(
                    "found 2 resources that use the same binding index {} in compute shader \"{}\"",
                    iBindingIndex,
                    pComputeShader->getShaderName()));
            }

            // Generate layout binding.
            auto layoutBindingResult = generateLayoutBinding(iBindingIndex, bindingInfo, true);
            if (std::holds_alternative<Error>(layoutBindingResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(layoutBindingResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Add binding to be used in layout.
            auto [binding, bindingFlags] =
                std::get<std::pair<VkDescriptorSetLayoutBinding, VkDescriptorBindingFlags>>(
                    layoutBindingResult);
            vLayoutBindings.push_back(binding);
            vLayoutBindingFlags.push_back(bindingFlags);

            // Save binding info.
            LayoutBindingInfo info;
            info.iBindingIndex = iBindingIndex;
            info.descriptorType = bindingInfo.resourceType;
            resourceBindings[bindingInfo.sResourceName] = info;

            // Mark binding index as used.
            usedBindingIndices.insert(iBindingIndex);
        }

        // Self check: make sure bindings and binding flags arrays have equal size.
        if (vLayoutBindings.size() != vLayoutBindingFlags.size()) [[unlikely]] {
            return Error(std::format(
                "layout binding array size {} is not equal to layout binding flags array size {} (this is a "
                "bug, report to developers",
                vLayoutBindings.size(),
                vLayoutBindingFlags.size()));
        }

        // Describe layout binding flags.
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
        bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlags.pNext = nullptr;
        bindingFlags.pBindingFlags = vLayoutBindingFlags.data();
        bindingFlags.bindingCount = static_cast<uint32_t>(vLayoutBindingFlags.size());

        // Check if some bindings use "update after bind" flag.
        bool bUseUpdateAfterBind = false;
        for (const auto& bindingFlags : vLayoutBindingFlags) {
            if ((bindingFlags & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) != 0) {
                bUseUpdateAfterBind = true;
                break;
            }
        }

        // Describe descriptor set layout.
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(vLayoutBindings.size());
        layoutInfo.pBindings = vLayoutBindings.data();

        // Make sure we don't use "update after bind" because it's currently not handled properly for compute
        // shaders.
        if (bUseUpdateAfterBind) [[unlikely]] {
            return Error(std::format(
                "unexpected compute shader \"{}\" to use \"update after bind\"",
                pComputeShader->getShaderName()));
        }

        // Specify binding flags.
        layoutInfo.pNext = &bindingFlags;

        // Prepare output variable.
        Generated generatedData;

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be valid");
        }

        // Create descriptor set layout.
        auto result = vkCreateDescriptorSetLayout(
            pLogicalDevice, &layoutInfo, nullptr, &generatedData.pDescriptorSetLayout);
        if (result != VK_SUCCESS) [[unlikely]] {
            return Error(
                std::format("failed to create descriptor set layout, error: {}", string_VkResult(result)));
        }

        // Continue to create a descriptor pool.

        // Describe descriptor types that our descriptor sets will contain.
        std::vector<VkDescriptorPoolSize> vPoolSizes(vLayoutBindings.size());
        for (size_t i = 0; i < vPoolSizes.size(); i++) {
            auto& descriptorPoolSize = vPoolSizes[i];

            // Specify descriptor type.
            descriptorPoolSize.type = vLayoutBindings[i].descriptorType;

            // Specify how much descriptors of this type can be allocated using the pool.
            descriptorPoolSize.descriptorCount =
                vLayoutBindings[i].descriptorCount *
                FrameResourcesManager::getFrameResourcesCount(); // also use frame resources just like in
                                                                 // graphics pipelines to be consistent
                                                                 // although we don't need that much
                                                                 // descriptors
        }

        // Describe descriptor pool.
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(vPoolSizes.size());
        poolInfo.pPoolSizes = vPoolSizes.data();

        // Specify flags.
        if (bUseUpdateAfterBind) {
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        }

        // Specify the total amount of descriptor sets that can be allocated from the pool.
        poolInfo.maxSets =
            FrameResourcesManager::getFrameResourcesCount(); // same as in graphics to be consistent

        // Create descriptor pool.
        result = vkCreateDescriptorPool(pLogicalDevice, &poolInfo, nullptr, &generatedData.pDescriptorPool);
        if (result != VK_SUCCESS) [[unlikely]] {
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedData.pDescriptorSetLayout, nullptr);
            return Error(std::format("failed to create descriptor pool, error: {}", string_VkResult(result)));
        }

        // Allocate descriptor sets.

        // Prepare layout for each descriptor set to create.
        std::array<VkDescriptorSetLayout, FrameResourcesManager::getFrameResourcesCount()>
            vDescriptorLayouts = {generatedData.pDescriptorSetLayout, generatedData.pDescriptorSetLayout};

        // Allocate descriptor sets.
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
        descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocInfo.descriptorPool = generatedData.pDescriptorPool;
        descriptorSetAllocInfo.descriptorSetCount = static_cast<uint32_t>(vDescriptorLayouts.size());
        descriptorSetAllocInfo.pSetLayouts = vDescriptorLayouts.data();

        // Make sure allocated descriptor set count will fit into our generated descriptor set array.
        if (descriptorSetAllocInfo.descriptorSetCount != generatedData.vDescriptorSets.size()) [[unlikely]] {
            vkDestroyDescriptorPool(pLogicalDevice, generatedData.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedData.pDescriptorSetLayout, nullptr);
            return Error(std::format(
                "attempting to allocate {} descriptor sets will fail because they will not fit into "
                "the generated array of descriptor sets with size {}",
                descriptorSetAllocInfo.descriptorSetCount,
                generatedData.vDescriptorSets.size()));
        }

        // Allocate descriptor sets.
        result = vkAllocateDescriptorSets(
            pLogicalDevice, &descriptorSetAllocInfo, generatedData.vDescriptorSets.data());
        if (result != VK_SUCCESS) [[unlikely]] {
            vkDestroyDescriptorPool(pLogicalDevice, generatedData.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedData.pDescriptorSetLayout, nullptr);
            return Error(std::format("failed to create descriptor sets, error: {}", string_VkResult(result)));
        }

        // Fill resource bindings info.
        for (const auto& [sResourceName, bindingInfo] : resourceBindings) {
            generatedData.resourceBindings[sResourceName] = bindingInfo.iBindingIndex;
        }

        return generatedData;
    }

    std::variant<std::pair<VkDescriptorSetLayoutBinding, VkDescriptorBindingFlags>, Error>
    DescriptorSetLayoutGenerator::generateLayoutBinding(
        uint32_t iBindingIndex,
        const Collected::DescriptorSetLayoutBindingInfo& bindingInfo,
        bool bIsComputeShader) {
        VkDescriptorSetLayoutBinding layoutBinding{};
        VkDescriptorBindingFlags bindingFlags{};

        // Set binding index.
        layoutBinding.binding = iBindingIndex;

        // Set descriptor count.
        layoutBinding.descriptorCount = 1;

        // Set stage.
        layoutBinding.stageFlags =
            bIsComputeShader ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ALL_GRAPHICS;

        // Set descriptor type.
        switch (bindingInfo.resourceType) {
        case (GlslResourceType::STORAGE_BUFFER): {
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        }
        case (GlslResourceType::UNIFORM_BUFFER): {
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        }
        case (GlslResourceType::COMBINED_SAMPLER): {
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            if (!bIsComputeShader) {
                // Override descriptor count.
                layoutBinding.descriptorCount = DescriptorConstants::iBindlessTextureArrayDescriptorCount;

                // Specify flags for bindless bindings.
                bindingFlags =
                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
            }

            break;
        }
        case (GlslResourceType::STORAGE_IMAGE): {
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
        }
        default: {
            return Error(std::format("resource \"{}\" has unsupported type", bindingInfo.sResourceName));
            break;
        }
        }

        return std::pair<VkDescriptorSetLayoutBinding, VkDescriptorBindingFlags>{layoutBinding, bindingFlags};
    }

} // namespace ne
