#include "VulkanPipeline.h"

// Standard.
#include <set>
#include <format>

// Custom.
#include "io/Logger.h"
#include "render/vulkan/VulkanRenderer.h"
#include "material/glsl/GlslShader.h"
#include "material/EngineShaderNames.hpp"
#include "render/vulkan/resources/VulkanResourceManager.h"
#include "render/vulkan/resources/VulkanStorageResourceArrayManager.h"
#include "material/glsl/DescriptorSetLayoutGenerator.h"

// External.
#include "vulkan/vk_enum_string_helper.h"

namespace ne {

    VulkanPipeline::~VulkanPipeline() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Destroy pipeline objects if they are valid.
        if (!mtxInternalResources.second.bIsReadyForUsage) {
            // Make sure all pointers were reset.
            if (mtxInternalResources.second.pPipeline != nullptr ||
                mtxInternalResources.second.pPipelineLayout != nullptr) [[unlikely]] {
                Logger::get().error("pipeline marked as not ready for usage but pipeline pointers "
                                    "are not `nullptr`");
            }
            return;
        }

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            Logger::get().error("expected a vulkan renderer");
            return;
        }

        // Make sure the renderer is no longer using this PSO or its resources.
        Logger::get().info(std::format(
            "waiting for the GPU to finish work up to this point before destroying a pipeline "
            "with id \"{}\"",
            getPipelineIdentifier()));
        pVulkanRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Release all resources.
        auto optionalError = VulkanPipeline::releaseInternalResources();
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            Logger::get().error(error.getFullErrorMessage()); // don't throw in destructor
        }
    }

    std::variant<std::shared_ptr<VulkanPipeline>, Error> VulkanPipeline::createGraphicsPipeline(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        const std::string& sVertexShaderName,
        const std::string& sFragmentShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalFragmentShaderMacros) {
        // Create pipeline.
        auto pPipeline = std::shared_ptr<VulkanPipeline>(new VulkanPipeline(
            pRenderer, pPipelineManager, sVertexShaderName, sFragmentShaderName, bUsePixelBlending));

        // Generate Vulkan pipeline.
        auto optionalError = pPipeline->generateGraphicsPipelineForShaders(
            sVertexShaderName,
            sFragmentShaderName,
            bUsePixelBlending,
            additionalVertexShaderMacros,
            additionalFragmentShaderMacros);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return pPipeline;
    }

    std::optional<Error> VulkanPipeline::releaseInternalResources() {
        std::scoped_lock resourcesGuard(mtxInternalResources.first);

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a vulkan renderer");
        }

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be valid");
        }

        // Destroy pipeline.
        vkDestroyPipeline(pLogicalDevice, mtxInternalResources.second.pPipeline, nullptr);
        mtxInternalResources.second.pPipeline = nullptr;

        // Destroy pipeline layout.
        vkDestroyPipelineLayout(pLogicalDevice, mtxInternalResources.second.pPipelineLayout, nullptr);
        mtxInternalResources.second.pPipelineLayout = nullptr;

        // Destroy descriptor pool.
        vkDestroyDescriptorPool(pLogicalDevice, mtxInternalResources.second.pDescriptorPool, nullptr);
        mtxInternalResources.second.pDescriptorPool = nullptr;

        // Destroy descriptor set layout.
        vkDestroyDescriptorSetLayout(
            pLogicalDevice, mtxInternalResources.second.pDescriptorSetLayout, nullptr);
        mtxInternalResources.second.pDescriptorSetLayout = nullptr;

        // Clear descriptor sets.
        for (auto& pDescriptorSet : mtxInternalResources.second.vDescriptorSets) {
            pDescriptorSet = nullptr;
        }

        // Clear resource binding pairs.
        mtxInternalResources.second.resourceBindings.clear();

        // Clear push constants.
        mtxInternalResources.second.pushConstantsData = {};

#if defined(WIN32) && defined(DEBUG)
        static_assert(
            sizeof(InternalResources) == 312, // NOLINT: current struct size
            "release new resources here");
#elif defined(DEBUG)
        static_assert(
            sizeof(InternalResources) == 240, // NOLINT: current struct size
            "release new resources here");
#endif

        // Done.
        mtxInternalResources.second.bIsReadyForUsage = false;

        return {};
    }

    std::optional<Error> VulkanPipeline::restoreInternalResources() {
        std::scoped_lock resourcesGuard(mtxInternalResources.first);

        // Recreate internal pipeline and other resources.
        auto optionalError = generateGraphicsPipelineForShaders(
            getVertexShaderName(),
            getPixelShaderName(),
            isUsingPixelBlending(),
            getAdditionalVertexShaderMacros(),
            getAdditionalPixelShaderMacros());
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return {};
    }

    std::variant<VkPushConstantRange, Error> VulkanPipeline::definePushConstants(
        const std::unordered_map<std::string, size_t>& pushConstantUintFieldOffsets,
        const std::unordered_map<std::string, uint32_t>& resourceBindings) {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure push constants data do not exist yet.
        if (mtxInternalResources.second.pushConstantsData.has_value()) [[unlikely]] {
            return Error("push constants data already exists");
        }

        // Make sure push constants are specified.
        if (pushConstantUintFieldOffsets.empty()) {
            return Error("received empty array of push constants");
        }

        // Make sure push constants referencing existing resources.
        for (const auto& [sFieldName, iOffsetInUints] : pushConstantUintFieldOffsets) {
            // Push constants names should be equal to resource name that they index into.
            const auto it = resourceBindings.find(sFieldName);
            if (it == resourceBindings.end()) [[unlikely]] {
                return Error(std::format(
                    "push constant \"{}\" is referencing a non-existing shader resource \"{}\", make sure "
                    "the name of your push constant is equal to the name of a shader resource you want "
                    "to index into",
                    sFieldName,
                    sFieldName));
            }
        }

        // Create new push constants data.
        mtxInternalResources.second.pushConstantsData = InternalResources::PushConstantsData{};

        // Save info about which resources will use which indices into push constants manager.
        mtxInternalResources.second.pushConstantsData.value().uintFieldIndicesToUse =
            pushConstantUintFieldOffsets;

        // Create push constants manager.
        mtxInternalResources.second.pushConstantsData.value().pPushConstantsManager =
            std::make_unique<VulkanPushConstantsManager>(pushConstantUintFieldOffsets.size());

        // Specify range (not creating multiple ranges since it's very complicated to setup).
        VkPushConstantRange range{};
        range.offset = 0;
        range.size = mtxInternalResources.second.pushConstantsData.value()
                         .pPushConstantsManager->getTotalSizeInBytes();
        range.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

        return range;
    }

    VulkanPipeline::VulkanPipeline(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        const std::string& sVertexShaderName,
        const std::string& sFragmentShaderName,
        bool bUsePixelBlending)
        : Pipeline(pRenderer, pPipelineManager, sVertexShaderName, sFragmentShaderName, bUsePixelBlending) {}

    std::optional<Error> VulkanPipeline::generateGraphicsPipelineForShaders(
        const std::string& sVertexShaderName,
        const std::string& sFragmentShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalFragmentShaderMacros) {
        std::scoped_lock resourcesGuard(mtxInternalResources.first);

        // Make sure the pipeline is not initialized yet.
        if (mtxInternalResources.second.bIsReadyForUsage) [[unlikely]] {
            Logger::get().warn(
                "pipeline was requested to generate internal PSO resources but internal resources are"
                "already created, ignoring this request");
            return {};
        }

        // Assign new shaders.
        const bool bVertexShaderNotFound = addShader(sVertexShaderName);
        const bool bFragmentShaderNotFound = addShader(sFragmentShaderName);

        // Check if shaders were found.
        if (bVertexShaderNotFound || bFragmentShaderNotFound) [[unlikely]] {
            return Error(std::format(
                "shaders not found in Shader Manager: vertex \"{}\" (found: {}), fragment \"{}\" "
                "(found: {}) ",
                sVertexShaderName,
                !bVertexShaderNotFound,
                bFragmentShaderNotFound,
                !bFragmentShaderNotFound));
        }

        // Get assigned shader packs.
        const auto pVertexShaderPack = getShader(ShaderType::VERTEX_SHADER).value();
        const auto pFragmentShaderPack = getShader(ShaderType::PIXEL_SHADER).value();

        // Get shaders for the current configuration.
        std::set<ShaderMacro> fullVertexShaderConfiguration;
        std::set<ShaderMacro> fullFragmentShaderConfiguration;
        auto pVertexShader = std::dynamic_pointer_cast<GlslShader>(
            pVertexShaderPack->getShader(additionalVertexShaderMacros, fullVertexShaderConfiguration));
        auto pFragmentShader = std::dynamic_pointer_cast<GlslShader>(
            pFragmentShaderPack->getShader(additionalFragmentShaderMacros, fullFragmentShaderConfiguration));

        // Get Vulkan renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Create graphics pipeline.
        auto optionalError = createGraphicsPipeline(
            pVulkanRenderer, pVertexShader.get(), pFragmentShader.get(), bUsePixelBlending);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Done generating pipeline.
        saveUsedShaderConfiguration(ShaderType::VERTEX_SHADER, std::move(fullVertexShaderConfiguration));
        saveUsedShaderConfiguration(ShaderType::PIXEL_SHADER, std::move(fullFragmentShaderConfiguration));

        // Bind "frameData" descriptors to frame uniform buffer.
        optionalError = bindFrameDataDescriptors();
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Get Vulkan resource manager.
        const auto pVulkanResourceManager =
            dynamic_cast<VulkanResourceManager*>(pVulkanRenderer->getResourceManager());
        const auto pStorageArrayManager = pVulkanResourceManager->getStorageResourceArrayManager();

        // Bind descriptors that use storage arrays.
        for (const auto& [sShaderResourceName, iBindingIndex] :
             mtxInternalResources.second.resourceBindings) {
            // Update descriptors.
            optionalError = pStorageArrayManager->updateDescriptorsForPipelineResource(
                pVulkanRenderer, this, sShaderResourceName, iBindingIndex);
            if (optionalError.has_value()) [[unlikely]] {
                auto error = std::move(optionalError.value());
                error.addCurrentLocationToErrorStack();
                return error;
            }
        }

        return {};
    }

    std::optional<Error> VulkanPipeline::createGraphicsPipeline(
        VulkanRenderer* pVulkanRenderer,
        GlslShader* pVertexShader,
        GlslShader* pFragmentShader,
        bool bUsePixelBlending) {
        // Get vertex shader bytecode and generate its descriptor layout.
        auto shaderBytecode = pVertexShader->getCompiledBytecode();
        if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
            auto err = std::get<Error>(std::move(shaderBytecode));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        const auto pMtxVertexShaderBytecode =
            std::get<std::pair<std::recursive_mutex, std::vector<char>>*>(std::move(shaderBytecode));

        // Get fragment shader bytecode and generate its descriptor layout.
        shaderBytecode = pFragmentShader->getCompiledBytecode();
        if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
            auto err = std::get<Error>(std::move(shaderBytecode));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        const auto pMtxFragmentShaderBytecode =
            std::get<std::pair<std::recursive_mutex, std::vector<char>>*>(std::move(shaderBytecode));

        // Generate one descriptor layout from both shaders.
        auto layoutResult =
            DescriptorSetLayoutGenerator::generate(pVulkanRenderer, pVertexShader, pFragmentShader);
        if (std::holds_alternative<Error>(layoutResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(layoutResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto generatedLayout = std::get<DescriptorSetLayoutGenerator::Generated>(std::move(layoutResult));

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            vkDestroyDescriptorPool(pLogicalDevice, generatedLayout.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedLayout.pDescriptorSetLayout, nullptr);
            return Error("expected logical device to be valid");
        }

        std::scoped_lock bytecodeGuard(pMtxVertexShaderBytecode->first, pMtxFragmentShaderBytecode->first);

        // Describe vertex shader module.
        VkShaderModuleCreateInfo vertexShaderModuleInfo{};
        vertexShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertexShaderModuleInfo.codeSize = pMtxVertexShaderBytecode->second.size();
        vertexShaderModuleInfo.pCode =
            reinterpret_cast<const uint32_t*>(pMtxVertexShaderBytecode->second.data());

        // Create vertex shader module.
        VkShaderModule pVertexShaderModule = nullptr;
        auto result =
            vkCreateShaderModule(pLogicalDevice, &vertexShaderModuleInfo, nullptr, &pVertexShaderModule);
        if (result != VK_SUCCESS) [[unlikely]] {
            vkDestroyDescriptorPool(pLogicalDevice, generatedLayout.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedLayout.pDescriptorSetLayout, nullptr);
            return Error(std::format(
                "failed to create a vertex shader module \"{}\", error: {}",
                pVertexShader->getShaderName(),
                string_VkResult(result)));
        }

        // Describe fragment shader module.
        VkShaderModuleCreateInfo fragmentShaderModuleInfo{};
        fragmentShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragmentShaderModuleInfo.codeSize = pMtxFragmentShaderBytecode->second.size();
        fragmentShaderModuleInfo.pCode =
            reinterpret_cast<const uint32_t*>(pMtxFragmentShaderBytecode->second.data());

        // Create fragment shader module.
        VkShaderModule pFragmentShaderModule = nullptr;
        result =
            vkCreateShaderModule(pLogicalDevice, &fragmentShaderModuleInfo, nullptr, &pFragmentShaderModule);
        if (result != VK_SUCCESS) [[unlikely]] {
            vkDestroyShaderModule(pLogicalDevice, pVertexShaderModule, nullptr);
            vkDestroyDescriptorPool(pLogicalDevice, generatedLayout.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedLayout.pDescriptorSetLayout, nullptr);
            return Error(std::format(
                "failed to create a fragment shader module \"{}\", error: {}",
                pFragmentShader->getShaderName(),
                string_VkResult(result)));
        }

        // Describe vertex shader pipeline stage.
        VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
        vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexShaderStageInfo.module = pVertexShaderModule;
        vertexShaderStageInfo.pName = "main";

        // Describe fragment shader pipeline stage.
        VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
        fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentShaderStageInfo.module = pFragmentShaderModule;
        fragmentShaderStageInfo.pName = "main";

        // Group shader stages.
        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
            vertexShaderStageInfo, fragmentShaderStageInfo};

        // Get bindings and attributes.
        const auto bindingDescription = GlslShader::getVertexBindingDescription();
        const auto attributeDescriptions = GlslShader::getVertexAttributeDescriptions();

        // Describe the vertex input.
        VkPipelineVertexInputStateCreateInfo vertexInputStateInfo{};
        vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateInfo.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputStateInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputStateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // Describe the input assembly.
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo{};
        inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

        // Get swap chain image size.
        auto optionalSwapChainExtent = pVulkanRenderer->getSwapChainExtent();
        if (!optionalSwapChainExtent.has_value()) [[unlikely]] {
            vkDestroyShaderModule(pLogicalDevice, pVertexShaderModule, nullptr);
            vkDestroyShaderModule(pLogicalDevice, pFragmentShaderModule, nullptr);
            vkDestroyDescriptorPool(pLogicalDevice, generatedLayout.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedLayout.pDescriptorSetLayout, nullptr);
            return Error("failed to get swap chain extent");
        }
        const auto swapChainExtent = optionalSwapChainExtent.value();

        // Describe viewport.
        VkViewport viewport{};
        viewport.x = 0.0F;
        viewport.y = static_cast<float>(swapChainExtent.height); // flip view space Y to behave as in DirectX
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height =
            -static_cast<float>(swapChainExtent.height); // flip view space Y to behave as in DirectX
        viewport.minDepth = Renderer::getMinDepth();
        viewport.maxDepth = Renderer::getMaxDepth();

        // Describe scissor.
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;

        // Combine viewport and scissor into a viewport state.
        VkPipelineViewportStateCreateInfo viewportStateInfo{};
        viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateInfo.viewportCount = 1;
        viewportStateInfo.pViewports = &viewport;
        viewportStateInfo.scissorCount = 1;
        viewportStateInfo.pScissors = &scissor;

        // Describe rasterizer.
        VkPipelineRasterizationStateCreateInfo rasterizerStateInfo{};
        rasterizerStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizerStateInfo.depthClampEnable = VK_FALSE;
        rasterizerStateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizerStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizerStateInfo.lineWidth = 1.0F;
        rasterizerStateInfo.cullMode = bUsePixelBlending ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        rasterizerStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizerStateInfo.depthBiasEnable = VK_FALSE;
        rasterizerStateInfo.depthBiasConstantFactor = 0.0F;
        rasterizerStateInfo.depthBiasClamp = 0.0F;
        rasterizerStateInfo.depthBiasSlopeFactor = 0.0F;

        // Get settings.
        const auto pRenderSettings = getRenderer()->getRenderSettings();
        std::scoped_lock resourcesGuard(mtxInternalResources.first, pRenderSettings->first);

        // Describe multisampling.
        VkPipelineMultisampleStateCreateInfo multisamplingStateInfo{};
        multisamplingStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisamplingStateInfo.sampleShadingEnable = VK_FALSE;
        multisamplingStateInfo.rasterizationSamples = pVulkanRenderer->getMsaaSampleCount();
        multisamplingStateInfo.minSampleShading = 1.0F;
        multisamplingStateInfo.pSampleMask = nullptr;
        multisamplingStateInfo.alphaToOneEnable = VK_FALSE;
        multisamplingStateInfo.alphaToCoverageEnable = VK_FALSE;

        // Describe depth and stencil state.
        VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo{};
        depthStencilStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateInfo.depthTestEnable = VK_TRUE;
        depthStencilStateInfo.depthWriteEnable = VK_TRUE;
        depthStencilStateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilStateInfo.minDepthBounds = Renderer::getMinDepth();
        depthStencilStateInfo.maxDepthBounds = Renderer::getMaxDepth();
        depthStencilStateInfo.stencilTestEnable = VK_FALSE;
        depthStencilStateInfo.front = {}; // for stencil
        depthStencilStateInfo.back = {};  // for stencil

        // Describe color blending per attached framebuffer.
        VkPipelineColorBlendAttachmentState colorBlendAttachmentStateInfo{};
        colorBlendAttachmentStateInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachmentStateInfo.blendEnable = bUsePixelBlending ? VK_TRUE : VK_FALSE;
        colorBlendAttachmentStateInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachmentStateInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachmentStateInfo.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachmentStateInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachmentStateInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachmentStateInfo.alphaBlendOp = VK_BLEND_OP_ADD;

        // Describe global color blending info.
        VkPipelineColorBlendStateCreateInfo colorBlendingStateInfo{};
        colorBlendingStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendingStateInfo.logicOpEnable = VK_FALSE;
        colorBlendingStateInfo.logicOp = VK_LOGIC_OP_COPY;
        colorBlendingStateInfo.attachmentCount = 1;
        colorBlendingStateInfo.pAttachments = &colorBlendAttachmentStateInfo;
        colorBlendingStateInfo.blendConstants[0] = 0.0F;
        colorBlendingStateInfo.blendConstants[1] = 0.0F;
        colorBlendingStateInfo.blendConstants[2] = 0.0F;
        colorBlendingStateInfo.blendConstants[3] = 0.0F;

        // Describe pipeline layout.
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &generatedLayout.pDescriptorSetLayout;

        // Specify push constants (if used).
        VkPushConstantRange pushConstants{};
        if (generatedLayout.pushConstantUintFieldOffsets.has_value()) {
            // Process push constants.
            auto pushConstantsResult = definePushConstants(
                generatedLayout.pushConstantUintFieldOffsets.value(), generatedLayout.resourceBindings);
            if (std::holds_alternative<Error>(pushConstantsResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(pushConstantsResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pushConstants = std::get<VkPushConstantRange>(pushConstantsResult);

            // Specify in layout.
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushConstants;
        }

        // Create pipeline layout.
        VkPipelineLayout pPipelineLayout = nullptr;
        result = vkCreatePipelineLayout(pLogicalDevice, &pipelineLayoutInfo, nullptr, &pPipelineLayout);
        if (result != VK_SUCCESS) [[unlikely]] {
            vkDestroyShaderModule(pLogicalDevice, pVertexShaderModule, nullptr);
            vkDestroyShaderModule(pLogicalDevice, pFragmentShaderModule, nullptr);
            vkDestroyDescriptorPool(pLogicalDevice, generatedLayout.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedLayout.pDescriptorSetLayout, nullptr);
            return Error(std::format("failed to create pipeline layout, error: {}", string_VkResult(result)));
        }

        // Describe graphics pipeline.
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        // Specify shader stages.
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();

        // Specify fixed-function stages.
        pipelineInfo.pVertexInputState = &vertexInputStateInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
        pipelineInfo.pViewportState = &viewportStateInfo;
        pipelineInfo.pRasterizationState = &rasterizerStateInfo;
        pipelineInfo.pMultisampleState = &multisamplingStateInfo;
        pipelineInfo.pDepthStencilState = &depthStencilStateInfo;
        pipelineInfo.pColorBlendState = &colorBlendingStateInfo;
        pipelineInfo.pDynamicState = nullptr;

        // Specify pipeline layout.
        pipelineInfo.layout = pPipelineLayout;

        // Get render pass.
        const auto pRenderPass = pVulkanRenderer->getRenderPass();
        if (pRenderPass == nullptr) [[unlikely]] {
            return Error("expected render pass to be valid");
        }

        // Specify render pass.
        pipelineInfo.renderPass = pRenderPass;
        pipelineInfo.subpass = 0;

        // Specify parent pipeline.
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        // Create graphics pipeline.
        VkPipeline pPipeline = nullptr;
        result =
            vkCreateGraphicsPipelines(pLogicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pPipeline);
        if (result != VK_SUCCESS) [[unlikely]] {
            vkDestroyPipelineLayout(pLogicalDevice, pPipelineLayout, nullptr);
            vkDestroyShaderModule(pLogicalDevice, pVertexShaderModule, nullptr);
            vkDestroyShaderModule(pLogicalDevice, pFragmentShaderModule, nullptr);
            vkDestroyDescriptorPool(pLogicalDevice, generatedLayout.pDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(pLogicalDevice, generatedLayout.pDescriptorSetLayout, nullptr);
            return Error(
                std::format("failed to create graphics pipeline, error: {}", string_VkResult(result)));
        }

        // Destroy shader modules since we don't need them after the pipeline was created.
        vkDestroyShaderModule(pLogicalDevice, pVertexShaderModule, nullptr);
        vkDestroyShaderModule(pLogicalDevice, pFragmentShaderModule, nullptr);

        // Save created resources.
        mtxInternalResources.second.pDescriptorSetLayout = generatedLayout.pDescriptorSetLayout;
        mtxInternalResources.second.pDescriptorPool = generatedLayout.pDescriptorPool;
        mtxInternalResources.second.vDescriptorSets = generatedLayout.vDescriptorSets;
        mtxInternalResources.second.resourceBindings = std::move(generatedLayout.resourceBindings);
        mtxInternalResources.second.pPipelineLayout = pPipelineLayout;
        mtxInternalResources.second.pPipeline = pPipeline;

        // Make sure map of bindless array index managers references only existing resources.
        for (const auto& [sShaderResourceName, pIndexManager] :
             mtxInternalResources.second.bindlessArrayIndexManagers) {
            // Look if this shader resource name is used in this pipeline.
            const auto it = mtxInternalResources.second.resourceBindings.find(sShaderResourceName);
            if (it == mtxInternalResources.second.resourceBindings.end()) [[unlikely]] {
                // Unexpected.
                Error error(std::format(
                    "pipeline \"{}\" restored its resources but some previously used index manager is "
                    "now handling indices for non-existing (no longer used) shader resource \"{}\"",
                    getPipelineIdentifier(),
                    sShaderResourceName));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
        }

        // Mark as ready to be used.
        mtxInternalResources.second.bIsReadyForUsage = true;

        return {};
    }

    std::optional<Error> VulkanPipeline::bindFrameDataDescriptors() {
        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("expected logical device to be valid");
        }

        // Get frame resource manager.
        const auto pFrameResourceManager = getRenderer()->getFrameResourcesManager();
        if (pFrameResourceManager == nullptr) [[unlikely]] {
            return Error("frame resource manager is `nullptr`");
        }

        // Get all frame resources.
        auto mtxAllFrameResource = pFrameResourceManager->getAllFrameResources();

        // Lock both frame resources and internal resources.
        std::scoped_lock frameResourceGuard(*mtxAllFrameResource.first, mtxInternalResources.first);

        // Make sure frame resource count is equal to our number of descriptor sets.
        if (mtxAllFrameResource.second.size() != mtxInternalResources.second.vDescriptorSets.size())
            [[unlikely]] {
            return Error(std::format(
                "expected frame resource count ({}) to be equal to descriptor set count ({})",
                mtxAllFrameResource.second.size(),
                mtxInternalResources.second.vDescriptorSets.size()));
        }

        for (size_t i = 0; i < mtxAllFrameResource.second.size(); i++) {
            // Get frame resource.
            const auto pFrameResource = mtxAllFrameResource.second[i]->pFrameConstantBuffer.get();

            // Convert frame resource to a Vulkan resource.
            const auto pVulkanResource = dynamic_cast<VulkanResource*>(pFrameResource->getInternalResource());
            if (pVulkanResource == nullptr) [[unlikely]] {
                return Error("expected a Vulkan resource");
            }

            // Get internal VkBuffer.
            const auto pVkFrameBuffer = pVulkanResource->getInternalBufferResource();
            if (pVkFrameBuffer == nullptr) [[unlikely]] {
                return Error("expected frame resource to be a buffer");
            }

            // Prepare info to bind frame uniform buffer to descriptor.
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = pVkFrameBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = pFrameResource->getElementSizeInBytes() * pFrameResource->getElementCount();

            // Bind buffer to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                mtxInternalResources.second.vDescriptorSets[i]; // descriptor set to update
            descriptorUpdateInfo.dstBinding =
                DescriptorSetLayoutGenerator::getFrameUniformBufferBindingIndex();
            descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
            descriptorUpdateInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorUpdateInfo.descriptorCount = 1;       // how much descriptors in array to update
            descriptorUpdateInfo.pBufferInfo = &bufferInfo; // descriptor refers to buffer data

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

} // namespace ne
