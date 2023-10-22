// Custom.
#include "game/GameInstance.h"
#include "game/Window.h"
#include "materials/resources/LightingShaderResourceManager.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("resetting a slot erases it from the array's active slots") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Get manager and array to test.
                const auto pManager = getWindow()->getRenderer()->getLightingShaderResourceManager();
                const auto pArray = pManager->getPointLightDataArray();
                const auto pMtxArrayResources = pArray->getInternalResources();

                // Prepare dummy data.
                struct SomeData {
                    int test = 1;
                };
                SomeData data;

                // Make sure there are zero active slots.
                REQUIRE(pMtxArrayResources->second.activeSlots.size() == 0);

                // Reserve a new slot.
                auto result = pArray->reserveNewSlot(
                    sizeof(SomeData), [&]() -> void* { return &data; }, []() {});
                if (std::holds_alternative<Error>(result)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto pSlot = std::get<std::unique_ptr<ShaderLightArraySlot>>(std::move(result));

                // Now one slot should exist.
                REQUIRE(pMtxArrayResources->second.activeSlots.size() == 1);

                // Mark as outdated...
                pSlot->markAsNeedsUpdate();
                for (const auto& slots : pMtxArrayResources->second.vSlotsToBeUpdated) {
                    REQUIRE(slots.size() == 1);
                }

                // ... and instantly reset.
                pSlot = nullptr;

                // Make sure there are zero active slots.
                REQUIRE(pMtxArrayResources->second.activeSlots.size() == 0);
                for (const auto& slots : pMtxArrayResources->second.vSlotsToBeUpdated) {
                    REQUIRE(slots.size() == 0);
                }

                getWindow()->close();
            });
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("onSizeChanged callback is called") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Get manager and array to test.
                const auto pManager = getWindow()->getRenderer()->getLightingShaderResourceManager();
                const auto pArray = pManager->getPointLightDataArray();
                const auto pMtxManagerInternalResources = pManager->getInternalResources();

                // Prepare dummy data.
                struct SomeData {
                    int test = 1;
                };
                SomeData data;

                // Make sure there are point light slots.
                REQUIRE(pMtxManagerInternalResources->second.generalData.iPointLightCount == 0);

                // Reserve a new slot.
                auto result = pArray->reserveNewSlot(
                    sizeof(SomeData), [&]() -> void* { return &data; }, []() {});
                if (std::holds_alternative<Error>(result)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto pSlot = std::get<std::unique_ptr<ShaderLightArraySlot>>(std::move(result));

                // Now the manager should be notified.
                REQUIRE(pMtxManagerInternalResources->second.generalData.iPointLightCount == 1);

                // Reset.
                pSlot = nullptr;

                // Again, manager should be notified.
                REQUIRE(pMtxManagerInternalResources->second.generalData.iPointLightCount == 0);

                getWindow()->close();
            });
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}
