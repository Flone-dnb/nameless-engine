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
            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager =
                dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());
            const auto pHeapManager = pResourceManager->getCbvSrvUavHeap();

            const auto iInitialHeapCapacity = pHeapManager->getHeapCapacity();
            const auto iResourcesTilExpand =
                (pHeapManager->getHeapCapacity() - pHeapManager->getHeapSize()) + 1;

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

            REQUIRE(pHeapManager->getHeapCapacity() > iInitialHeapCapacity);
            REQUIRE(pHeapManager->getHeapSize() == iInitialHeapCapacity + 1);

            // Remove more than half of the resources to make the heap shrink.
            const auto iRemoveResourceCount =
                static_cast<size_t>(static_cast<float>(vCreatedResources.size()) * 0.6f);
            std::random_device dev;
            std::mt19937 rng(dev());
            for (size_t i = 0; i < iRemoveResourceCount; i++) {
                std::uniform_int_distribution<long long> dist(
                    0, static_cast<long long>(vCreatedResources.size()) - 1);
                vCreatedResources.erase(std::next(vCreatedResources.begin(), dist(rng)));
            }

            REQUIRE(pHeapManager->getHeapCapacity() == iInitialHeapCapacity);

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
            // (should fail because descriptor of this type is already added)
            optionalError = pResource->bindDescriptor(DirectXDescriptorType::SRV);
            REQUIRE(optionalError.has_value());

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
