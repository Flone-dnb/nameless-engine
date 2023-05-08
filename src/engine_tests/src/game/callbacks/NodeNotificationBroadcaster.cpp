// Custom.
#include "game/nodes/Node.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/callbacks/NodeNotificationBroadcaster.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("broadcast does not trigger despawned nodes") {
    using namespace ne;

    class MyNode : public Node {
    public:
        void createBroadcaster() {
            pBroadcaster = createNotificationBroadcaster<void()>();
            REQUIRE(pBroadcaster != nullptr);
        }

        NodeFunction<void()> getCallback() {
            return NodeFunction<void()>(getNodeId().value(), [this]() { myCallback(); });
        }

        void subscribe(const NodeFunction<void()>& callback) { pBroadcaster->subscribe(callback); }

        void broadcast() { pBroadcaster->broadcast(); }

        size_t getSubscriberCount() { return pBroadcaster->getSubscriberCount(); }

        size_t iCallbackCallCount = 0;

    private:
        void myCallback() {
            REQUIRE(isSpawned());
            iCallbackCallCount += 1;
        }

        NodeNotificationBroadcaster<void()>* pBroadcaster = nullptr;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create nodes.
                pBroadcasterNode = gc_new<MyNode>();
                pBroadcasterNode->createBroadcaster();
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 0);

                pSubscriberNode1 = gc_new<MyNode>();
                pSubscriberNode2 = gc_new<MyNode>();

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 0);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 0);

                // Now spawn nodes.
                getWorldRootNode()->addChildNode(pBroadcasterNode);
                getWorldRootNode()->addChildNode(pSubscriberNode1);
                getWorldRootNode()->addChildNode(pSubscriberNode2);

                // Subscribe.
                pBroadcasterNode->subscribe(pSubscriberNode1->getCallback());
                pBroadcasterNode->subscribe(pSubscriberNode2->getCallback());
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                // Broadcast.
                pBroadcasterNode->broadcast(); // queues a deferred task

                // wait a few frames
                REQUIRE(iTickCount == 0);
            });
        }

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);

            iTickCount += 1;

            if (iTickCount == 3) {
                REQUIRE(pSubscriberNode1->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                // Now broadcast again.
                pBroadcasterNode->broadcast();

                // ... but this time despawn subscriber 1.
                pSubscriberNode1->detachFromParentAndDespawn();
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2); // not detected yet
            } else if (iTickCount == 5) {
                REQUIRE(pSubscriberNode1->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 2);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 1);

                getWindow()->close();
            }
        }

    private:
        gc<MyNode> pBroadcasterNode = nullptr;
        gc<MyNode> pSubscriberNode1 = nullptr;
        gc<MyNode> pSubscriberNode2 = nullptr;
        size_t iTickCount = 0;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
