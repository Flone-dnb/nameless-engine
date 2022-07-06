// STL.
#include <random>

// Custom.
#include "render/directx/resources/DirectXResourceManager.h"
#include "game/IGameInstance.h"
#include "render/directx/DirectXRenderer.h"
#include "game/window.h"
#include "render/directx/descriptors/DirectXDescriptorHeapManager.h"
#include "render/directx/resources/DirectXResource.h"

// External.
#include "Catch2/catch_test_macros.hpp"
#include "directx/d3dx12.h"
#include "D3D12MemoryAllocator/include/D3D12MemAlloc.h"

constexpr size_t iResourceSizeInBytes = 1024;

TEST_CASE("make the CBV heap expand") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager = pRenderer->getResourceManager();
            const auto pHeapManager = pResourceManager->getCbvSrvUavHeap();

            const auto iInitialHeapCapacity = pHeapManager->getHeapCapacity();
            const auto iResourcesTilExpand = pHeapManager->getHeapCapacity() - pHeapManager->getHeapSize();

            // Prepare data for resource creation.
            D3D12MA::ALLOCATION_DESC allocationDesc = {};
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iResourceSizeInBytes);

            std::vector<std::unique_ptr<DirectXResource>> vCreatedResources(iResourcesTilExpand);

            for (int i = 0; i < iResourcesTilExpand; i++) {
                auto result = pResourceManager->createCbvSrvUavResource(
                    allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ);
                if (std::holds_alternative<Error>(result)) {
                    auto err = std::get<Error>(std::move(result));
                    err.addEntry();
                    INFO(err.getError());
                    REQUIRE(false);
                }

                vCreatedResources[i] = std::get<std::unique_ptr<DirectXResource>>(std::move(result));
            }

            REQUIRE(pHeapManager->getHeapCapacity() == iInitialHeapCapacity);

            // Create one more resource so that the heap will expand.
            auto result = pResourceManager->createCbvSrvUavResource(
                allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                INFO(err.getError());
                REQUIRE(false);
            }

            vCreatedResources.push_back(std::get<std::unique_ptr<DirectXResource>>(std::move(result)));

            REQUIRE(pHeapManager->getHeapCapacity() > iInitialHeapCapacity);
            REQUIRE(pHeapManager->getHeapSize() == iInitialHeapCapacity + 1);

            // TODO: check that resources and views are valid

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("make the CBV heap shrink") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
            auto pRenderer = dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer());
            REQUIRE(pRenderer);

            const auto pResourceManager = pRenderer->getResourceManager();
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
                auto result = pResourceManager->createCbvSrvUavResource(
                    allocationDesc, resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ);
                if (std::holds_alternative<Error>(result)) {
                    auto err = std::get<Error>(std::move(result));
                    err.addEntry();
                    INFO(err.getError());
                    REQUIRE(false);
                }

                vCreatedResources[i] = std::get<std::unique_ptr<DirectXResource>>(std::move(result));
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

            // TODO: check that resources and views are valid

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}