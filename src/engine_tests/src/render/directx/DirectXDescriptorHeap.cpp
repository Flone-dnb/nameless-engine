// Standard.
#include <random>

// Custom.
#include "render/directx/resources/DirectXResourceManager.h"
#include "game/GameInstance.h"
#include "render/directx/DirectXRenderer.h"
#include "game/Window.h"
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/directx/resources/DirectXResource.h"

// External.
#include "catch2/catch_test_macros.hpp"
#include "directx/d3dx12.h"
#include "D3D12MemoryAllocator/include/D3D12MemAlloc.h"

constexpr size_t iResourceSizeInBytes = 1024;

TEST_CASE("make the CBV heap expand") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());
            const auto pHeapManager = pResourceManager->getCbvSrvUavHeap();

            const auto iInitialHeapCapacity = pHeapManager->getHeapCapacity();
            const auto iResourcesTilExpand = pHeapManager->getHeapCapacity() - pHeapManager->getHeapSize();

            // Prepare data for resource creation.
            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iResourceSizeInBytes);

            std::vector<std::unique_ptr<DirectXResource>> vCreatedResources(iResourcesTilExpand);

            for (int i = 0; i < iResourcesTilExpand; i++) {
                // Create resource.
                auto result = pResourceManager->createResource(
                    "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
                if (std::holds_alternative<Error>(result)) {
                    auto err = std::get<Error>(std::move(result));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

                // Bind CBV.
                auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::CBV);
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Add to array.
                vCreatedResources[i] = std::move(pResource);
            }

            REQUIRE(pHeapManager->getHeapCapacity() == iInitialHeapCapacity);

            // Create one more resource so that the heap will expand.
            auto result = pResourceManager->createResource(
                "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind CBV.
            auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::CBV);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Add to array.
            vCreatedResources.push_back(std::move(pResource));

            REQUIRE(pHeapManager->getHeapCapacity() > iInitialHeapCapacity);
            REQUIRE(pHeapManager->getHeapSize() == iInitialHeapCapacity + 1);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("make the CBV heap shrink") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            // Get renderer.
            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            // Get heap.
            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());
            const auto pHeapManager = pResourceManager->getCbvSrvUavHeap();

            // Save current heap capacity/size to compare later.
            const auto iInitialHeapCapacity = pHeapManager->getHeapCapacity();
            const auto iInitialHeapSize = pHeapManager->getHeapSize();

            // Calculate how much descriptors we need to expand
            const auto iResourcesToCreateCount =
                (pHeapManager->getHeapCapacity() - pHeapManager->getHeapSize()) +
                DirectXDescriptorHeap::getHeapGrowSize() + 1;
            const auto iTargetCapacity = iInitialHeapCapacity + DirectXDescriptorHeap::getHeapGrowSize() * 2;

            // Prepare data for resource creation.
            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iResourceSizeInBytes);

            std::vector<std::unique_ptr<DirectXResource>> vCreatedResources(iResourcesToCreateCount);

            for (int i = 0; i < iResourcesToCreateCount; i++) {
                // Create resource.
                auto result = pResourceManager->createResource(
                    "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
                if (std::holds_alternative<Error>(result)) {
                    auto err = std::get<Error>(std::move(result));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

                // Bind CBV.
                auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::CBV);
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Add to array.
                vCreatedResources[i] = std::move(pResource);
            }

            // Check heap capacity/size.
            REQUIRE(pHeapManager->getHeapCapacity() == iTargetCapacity);
            REQUIRE(
                pHeapManager->getHeapSize() ==
                iInitialHeapCapacity + DirectXDescriptorHeap::getHeapGrowSize() + 1);

            // Remove all resources.
            vCreatedResources.clear();

            // Check heap capacity.
            REQUIRE(
                (pHeapManager->getHeapCapacity() == iInitialHeapCapacity ||
                 pHeapManager->getHeapCapacity() ==
                     iInitialHeapCapacity + DirectXDescriptorHeap::getHeapGrowSize()));
            REQUIRE(pHeapManager->getHeapSize() == iInitialHeapSize);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("assign multiple descriptors to one resource") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());

            // Prepare data for resource creation.
            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const auto resourceDesc = CD3DX12_RESOURCE_DESC(
                D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                0,
                1024,
                1024,
                1,
                1,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                1,
                0,
                D3D12_TEXTURE_LAYOUT_UNKNOWN,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            // Create SRV resource.
            auto result = pResourceManager->createResource(
                "Test SRV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_COMMON, {});
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind SRV.
            auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::SRV);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Assign a UAV descriptor to this resource.
            optionalError = pResource->bindDescriptor(DirectXDescriptorType::UAV);
            if (optionalError.has_value()) {
                optionalError->addCurrentLocationToErrorStack();
                INFO(optionalError->getFullErrorMessage());
                REQUIRE(false);
            }

            // Assign a SRV descriptor to this resource (again).
            // (should not fail, will be just ignored).
            optionalError = pResource->bindDescriptor(DirectXDescriptorType::SRV);
            REQUIRE(!optionalError.has_value());

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("all assigned descriptors are freed when resource is destroyed") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());

            const auto iNoLongerUsedDescriptorCount =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager())
                    ->getCbvSrvUavHeap()
                    ->getNoLongerUsedDescriptorCount();
            REQUIRE(iNoLongerUsedDescriptorCount == 0);

            {
                // Prepare data for resource creation.
                D3D12MA::ALLOCATION_DESC allocationDesc = {};
                allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
                const auto resourceDesc = CD3DX12_RESOURCE_DESC(
                    D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                    0,
                    1024,
                    1024,
                    1,
                    1,
                    DXGI_FORMAT_R8G8B8A8_UNORM,
                    1,
                    0,
                    D3D12_TEXTURE_LAYOUT_UNKNOWN,
                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

                // Create SRV resource.
                auto result = pResourceManager->createResource(
                    "Test SRV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_COMMON, {});
                if (std::holds_alternative<Error>(result)) {
                    auto err = std::get<Error>(std::move(result));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

                // Bind SRV.
                auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::SRV);
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Assign a UAV descriptor to this resource.
                optionalError = pResource->bindDescriptor(DirectXDescriptorType::UAV);
                if (optionalError.has_value()) {
                    optionalError->addCurrentLocationToErrorStack();
                    INFO(optionalError->getFullErrorMessage());
                    REQUIRE(false);
                }
            }

            const auto iNewNoLongerUsedDescriptorCount =
                pResourceManager->getCbvSrvUavHeap()->getNoLongerUsedDescriptorCount();
            REQUIRE(iNewNoLongerUsedDescriptorCount == 2);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("create CBV resource") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());

            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iResourceSizeInBytes);

            // Create CBV resource.
            auto result = pResourceManager->createResource(
                "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind CBV.
            auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::CBV);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("create SRV resource") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());

            // Prepare data for resource creation.
            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const auto resourceDesc = CD3DX12_RESOURCE_DESC(
                D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                0,
                1024,
                1024,
                1,
                1,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                1,
                0,
                D3D12_TEXTURE_LAYOUT_UNKNOWN,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            // Create SRV resource.
            auto result = pResourceManager->createResource(
                "Test SRV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_COMMON, {});
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind SRV.
            auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::SRV);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("create UAV resource") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());

            // Prepare data for resource creation.
            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const auto resourceDesc = CD3DX12_RESOURCE_DESC(
                D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                0,
                1024,
                1024,
                1,
                1,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                1,
                0,
                D3D12_TEXTURE_LAYOUT_UNKNOWN,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            // Create UAV resource.
            auto result = pResourceManager->createResource(
                "Test UAV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_COMMON, {});
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind UAV.
            auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::UAV);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("create RTV resource") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());

            // Prepare data for resource creation.
            const auto resourceDesc = CD3DX12_RESOURCE_DESC(
                D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                0,
                1024,
                1024,
                1,
                1,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                1,
                0,
                D3D12_TEXTURE_LAYOUT_UNKNOWN,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

            D3D12_CLEAR_VALUE clearValue;
            clearValue.Format = resourceDesc.Format;

            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

            // Create RTV resource.
            auto result = pResourceManager->createResource(
                "Test RTV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_COMMON, clearValue);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind RTV.
            auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::RTV);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("create DSV resource") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());

            // Prepare data for resource creation.
            D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC(
                D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                0,
                1024,
                1024,
                1,
                1,
                DXGI_FORMAT_D24_UNORM_S8_UINT,
                1,
                0,
                D3D12_TEXTURE_LAYOUT_UNKNOWN,
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

            D3D12_CLEAR_VALUE depthClear;
            depthClear.Format = depthStencilDesc.Format;
            depthClear.DepthStencil.Depth = 1.0f;
            depthClear.DepthStencil.Stencil = 0;

            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

            // Create DSV resource.
            auto result = pResourceManager->createResource(
                "Test DSV resource",
                allocationDesc,
                depthStencilDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                depthClear);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind DSV.
            auto optionalError = pResource->bindDescriptor(DirectXDescriptorType::DSV);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("make descriptor range expand/shrink") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            // Get renderer.
            const auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer != nullptr);

            // Get resource manager.
            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());
            REQUIRE(pResourceManager != nullptr);

            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iResourceSizeInBytes);

            // Create 2 resources.
            auto result = pResourceManager->createResource(
                "Test CBV resource 1", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource1 = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            result = pResourceManager->createResource(
                "Test CBV resource 2", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource2 = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Get descriptor heap.
            const auto pCbvHeap = pResourceManager->getCbvSrvUavHeap();
            const auto iHeapSizeBeforeRange = pCbvHeap->getHeapSize();

            // Allocate descriptor range.
            auto rangeResult = pCbvHeap->allocateContinuousDescriptorRange(
                "test CBV range", [this]() { onRangeIndicesChanged(); });
            if (std::holds_alternative<Error>(rangeResult)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pRange = std::get<std::unique_ptr<ContinuousDirectXDescriptorRange>>(std::move(rangeResult));

            // Check heap size.
            REQUIRE(
                iHeapSizeBeforeRange + ContinuousDirectXDescriptorRange::getRangeGrowSize() ==
                pCbvHeap->getHeapSize());

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == 0);

            // Bind CBV from range to resource 1.
            auto optionalError = pResource1->bindDescriptor(DirectXDescriptorType::CBV, pRange.get());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == 1);

            // Get descriptor.
            const auto pCbvDescriptor1 = pResource1->getDescriptor(DirectXDescriptorType::CBV);
            REQUIRE(pCbvDescriptor1 != nullptr);

            // Make sure descriptor index actually uses range space.
            REQUIRE(pCbvDescriptor1->getDescriptorOffsetInDescriptors() == pRange->getRangeStartInHeap());

            // Bind CBV from range to resource 2.
            optionalError = pResource2->bindDescriptor(DirectXDescriptorType::CBV, pRange.get());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == 2);

            // Get descriptor.
            const auto pCbvDescriptor2 = pResource2->getDescriptor(DirectXDescriptorType::CBV);
            REQUIRE(pCbvDescriptor2 != nullptr);

            // Make sure descriptor index actually uses range space.
            REQUIRE(pCbvDescriptor2->getDescriptorOffsetInDescriptors() == pRange->getRangeStartInHeap() + 1);

            // Fill range so that there's no free space.
            constexpr auto iAdditionalResourceCount =
                ContinuousDirectXDescriptorRange::getRangeGrowSize() - 2;
            std::array<std::unique_ptr<DirectXResource>, iAdditionalResourceCount> vResources;

            for (size_t i = 0; i < iAdditionalResourceCount; i++) {
                // Create resource.
                result = pResourceManager->createResource(
                    "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
                if (std::holds_alternative<Error>(result)) [[unlikely]] {
                    auto err = std::get<Error>(std::move(result));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }
                vResources[i] = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

                // Bind CBV.
                optionalError = vResources[i]->bindDescriptor(DirectXDescriptorType::CBV, pRange.get());
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Check range size/capacity.
                REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
                REQUIRE(pRange->getRangeSize() == 2 + i + 1);

                // Get descriptor.
                const auto pCbvDescriptor = vResources[i]->getDescriptor(DirectXDescriptorType::CBV);
                REQUIRE(pCbvDescriptor != nullptr);

                // Make sure descriptor index actually uses range space.
                REQUIRE(
                    pCbvDescriptor->getDescriptorOffsetInDescriptors() ==
                    pRange->getRangeStartInHeap() + 2 + i);
            }

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == ContinuousDirectXDescriptorRange::getRangeGrowSize());

            // Create resource.
            result = pResourceManager->createResource(
                "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            const auto pSingleResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind CBV in the heap (not in the range).
            optionalError = pSingleResource->bindDescriptor(DirectXDescriptorType::CBV);
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == ContinuousDirectXDescriptorRange::getRangeGrowSize());

            // Prepare to delete one resource and remember its descriptor index.
            const auto iDeletedDescriptorIndexInHeap =
                vResources[0]->getDescriptor(DirectXDescriptorType::CBV)->getDescriptorOffsetInDescriptors();

            // Now delete one resource.
            vResources[0] = nullptr;

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == ContinuousDirectXDescriptorRange::getRangeGrowSize() - 1);

            // Re-create deleted resource.
            result = pResourceManager->createResource(
                "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            vResources[0] = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Bind CBV in the range.
            optionalError = vResources[0]->bindDescriptor(DirectXDescriptorType::CBV, pRange.get());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == ContinuousDirectXDescriptorRange::getRangeGrowSize());

            // Descriptor index should have been reused.
            REQUIRE(
                vResources[0]
                    ->getDescriptor(DirectXDescriptorType::CBV)
                    ->getDescriptorOffsetInDescriptors() == iDeletedDescriptorIndexInHeap);

            // Create resource.
            result = pResourceManager->createResource(
                "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pExpansionResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            REQUIRE(iRangeCallbackCallCount == 0);

            // Prepare for range to expand.
            bIsExpencingRangeIndicesToChange = true;

            // Bind CBV in the range.
            optionalError = pExpansionResource->bindDescriptor(DirectXDescriptorType::CBV, pRange.get());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Done expanding.
            bIsExpencingRangeIndicesToChange = false;

            REQUIRE(iRangeCallbackCallCount == 1);

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize() * 2);
            REQUIRE(pRange->getRangeSize() == ContinuousDirectXDescriptorRange::getRangeGrowSize() + 1);

            // Now delete 3 resources.
            pResource1 = nullptr;
            pResource2 = nullptr;
            pExpansionResource = nullptr;

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize() * 2);
            REQUIRE(pRange->getRangeSize() == ContinuousDirectXDescriptorRange::getRangeGrowSize() - 2);

            // Now delete a bunch of resources (except for one).
            bIsExpencingRangeIndicesToChange = true;
            for (size_t i = 1; i < vResources.size(); i++) {
                vResources[i] = nullptr;
            }
            bIsExpencingRangeIndicesToChange = false;

            REQUIRE(iRangeCallbackCallCount == 2);

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == 1);

            // Now make the heap expand and check that range callback is called.
            const auto iResourcesToCreate = pCbvHeap->getHeapCapacity() - pCbvHeap->getHeapSize();
            std::vector<std::unique_ptr<DirectXResource>> vAdditionalHeapResources;
            vAdditionalHeapResources.resize(iResourcesToCreate);

            for (size_t i = 0; i < iResourcesToCreate; i++) {
                // Create resource.
                result = pResourceManager->createResource(
                    "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
                if (std::holds_alternative<Error>(result)) [[unlikely]] {
                    auto err = std::get<Error>(std::move(result));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }
                vAdditionalHeapResources[i] = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

                // Bind CBV.
                optionalError = vAdditionalHeapResources[i]->bindDescriptor(DirectXDescriptorType::CBV);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
            }

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == 1);

            // Check heap size.
            REQUIRE(pCbvHeap->getHeapSize() == pCbvHeap->getHeapCapacity());

            // Now create the last resource (that will cause heap expansion).
            result = pResourceManager->createResource(
                "Test CBV resource", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pHeapExpansionResource = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            bIsExpencingRangeIndicesToChange = true;

            // Bind CBV.
            optionalError = pHeapExpansionResource->bindDescriptor(DirectXDescriptorType::CBV);
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            bIsExpencingRangeIndicesToChange = false;

            REQUIRE(iRangeCallbackCallCount == 3);

            // Check range size/capacity.
            REQUIRE(pRange->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange->getRangeSize() == 1);

            // Destroy last descriptor before destructing the range.
            vResources[0] = nullptr;

            pRange = nullptr;

            pGameWindow->close();
        }

        void onRangeIndicesChanged() {
            REQUIRE(bIsExpencingRangeIndicesToChange);

            iRangeCallbackCallCount += 1;
        }

        virtual ~TestGameInstance() override = default;

    private:
        bool bIsExpencingRangeIndicesToChange = false;
        size_t iRangeCallbackCallCount = 0;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("descriptor ranges have correct index offset") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            // Get renderer.
            const auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer != nullptr);

            // Get resource manager.
            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());
            REQUIRE(pResourceManager != nullptr);

            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iResourceSizeInBytes);

            // Create 2 resources.
            auto result = pResourceManager->createResource(
                "Test CBV resource 1", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource1 = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            result = pResourceManager->createResource(
                "Test CBV resource 2", allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, {});
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pResource2 = std::get<std::unique_ptr<DirectXResource>>(std::move(result));

            // Get descriptor heap.
            const auto pCbvHeap = pResourceManager->getCbvSrvUavHeap();
            const auto iHeapSizeBeforeRange = pCbvHeap->getHeapSize();

            // Allocate descriptor range 1.
            auto rangeResult = pCbvHeap->allocateContinuousDescriptorRange("test CBV range 1", []() {});
            if (std::holds_alternative<Error>(rangeResult)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pRange1 =
                std::get<std::unique_ptr<ContinuousDirectXDescriptorRange>>(std::move(rangeResult));

            // Allocate descriptor range 2.
            rangeResult = pCbvHeap->allocateContinuousDescriptorRange("test CBV range 2", []() {});
            if (std::holds_alternative<Error>(rangeResult)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                INFO(err.getFullErrorMessage());
                REQUIRE(false);
            }
            auto pRange2 =
                std::get<std::unique_ptr<ContinuousDirectXDescriptorRange>>(std::move(rangeResult));

            // Check heap size.
            REQUIRE(
                iHeapSizeBeforeRange + ContinuousDirectXDescriptorRange::getRangeGrowSize() * 2 ==
                pCbvHeap->getHeapSize());

            // Check range size/capacity.
            REQUIRE(pRange1->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange1->getRangeSize() == 0);
            REQUIRE(pRange2->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange2->getRangeSize() == 0);

            // Bind CBV resource 1 - range 1.
            auto optionalError = pResource1->bindDescriptor(DirectXDescriptorType::CBV, pRange1.get());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Check range size/capacity.
            REQUIRE(pRange1->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange1->getRangeSize() == 1);

            // Get descriptor.
            const auto pCbvDescriptor1 = pResource1->getDescriptor(DirectXDescriptorType::CBV);
            REQUIRE(pCbvDescriptor1 != nullptr);

            // Make sure descriptor index actually uses range space.
            REQUIRE(pCbvDescriptor1->getDescriptorOffsetInDescriptors() == pRange1->getRangeStartInHeap());

            // Bind CBV resource 2 - range 2.
            optionalError = pResource2->bindDescriptor(DirectXDescriptorType::CBV, pRange2.get());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }

            // Check range size/capacity.
            REQUIRE(pRange2->getRangeCapacity() == ContinuousDirectXDescriptorRange::getRangeGrowSize());
            REQUIRE(pRange2->getRangeSize() == 1);

            // Get descriptor.
            const auto pCbvDescriptor2 = pResource2->getDescriptor(DirectXDescriptorType::CBV);
            REQUIRE(pCbvDescriptor2 != nullptr);

            // Make sure descriptor index actually uses range space.
            REQUIRE(pCbvDescriptor2->getDescriptorOffsetInDescriptors() == pRange2->getRangeStartInHeap());

            // Make sure distance (offset) between range starts is correct.
            INT iLowRangeStart = 0;
            INT iHighRangeStart = 0;
            if (pCbvDescriptor1->getDescriptorOffsetInDescriptors() <
                pCbvDescriptor2->getDescriptorOffsetInDescriptors()) {
                iLowRangeStart = pCbvDescriptor1->getDescriptorOffsetInDescriptors();
                iHighRangeStart = pCbvDescriptor2->getDescriptorOffsetInDescriptors();
            } else {
                iLowRangeStart = pCbvDescriptor2->getDescriptorOffsetInDescriptors();
                iHighRangeStart = pCbvDescriptor1->getDescriptorOffsetInDescriptors();
            }
            REQUIRE(iHighRangeStart - iLowRangeStart == ContinuousDirectXDescriptorRange::getRangeGrowSize());

            // Destroy resources before ranges.
            pResource1 = nullptr;
            pResource2 = nullptr;

            // Destroy ranges.
            pRange1 = nullptr;
            pRange2 = nullptr;

            pGameWindow->close();
        }

        virtual ~TestGameInstance() override = default;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
