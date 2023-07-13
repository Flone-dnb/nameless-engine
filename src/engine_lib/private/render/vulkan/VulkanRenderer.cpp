#include "render/vulkan/VulkanRenderer.h"

// Custom.
#include "render/general/pso/PsoManager.h"

namespace ne {
    VulkanRenderer::VulkanRenderer(GameManager* pGameManager) : Renderer(pGameManager) {}

    std::optional<Error> VulkanRenderer::compileEngineShaders() const {
        throw std::runtime_error("not implemented");
    }

    std::optional<Error> VulkanRenderer::initialize() {
        std::scoped_lock frameGuard(*getRenderResourcesMutex());

        // Initialize essential entities.
        auto optionalError = initializeRenderer();
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    std::variant<std::unique_ptr<Renderer>, Error> VulkanRenderer::create(GameManager* pGameManager) {
        // Create an empty (uninitialized) Vulkan renderer.
        auto pRenderer = std::unique_ptr<VulkanRenderer>(new VulkanRenderer(pGameManager));

        // Initialize renderer.
        const auto optionalError = pRenderer->initialize();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            return error;
        }

        return pRenderer;
    }

    std::variant<std::vector<std::string>, Error> VulkanRenderer::getSupportedGpuNames() const {
        throw std::runtime_error("not implemented");
    }

    std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
    VulkanRenderer::getSupportedRenderResolutions() const {
        throw std::runtime_error("not implemented");
    }

    std::variant<std::set<std::pair<unsigned int, unsigned int>>, Error>
    VulkanRenderer::getSupportedRefreshRates() const {
        throw std::runtime_error("not implemented");
    }

    RendererType VulkanRenderer::getType() const { return RendererType::VULKAN; }

    std::string VulkanRenderer::getUsedApiVersion() const {
        static_assert(iUsedVulkanVersion == VK_API_VERSION_1_0, "update returned version string");
        return "1.0";
    }

    std::string VulkanRenderer::getCurrentlyUsedGpuName() const {
        throw std::runtime_error("not implemented");
    }

    size_t VulkanRenderer::getTotalVideoMemoryInMb() const { throw std::runtime_error("not implemented"); }

    size_t VulkanRenderer::getUsedVideoMemoryInMb() const { throw std::runtime_error("not implemented"); }

    void VulkanRenderer::waitForGpuToFinishWorkUpToThisPoint() {
        throw std::runtime_error("not implemented");
    }

    void VulkanRenderer::drawNextFrame() { throw std::runtime_error("not implemented"); }

    std::optional<Error> VulkanRenderer::updateRenderBuffers() {
        throw std::runtime_error("not implemented");
    }

    std::optional<Error> VulkanRenderer::createDepthStencilBuffer() {
        throw std::runtime_error("not implemented");
    }

    bool VulkanRenderer::isInitialized() const { throw std::runtime_error("not implemented"); }
} // namespace ne
