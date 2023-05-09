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
        virtual ~MyNode() override = default;

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
                pSubscriberNode3 = gc_new<MyNode>();

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 0);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 0);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 0);

                // Now spawn nodes.
                getWorldRootNode()->addChildNode(pBroadcasterNode);
                getWorldRootNode()->addChildNode(pSubscriberNode1);
                getWorldRootNode()->addChildNode(pSubscriberNode2);
                getWorldRootNode()->addChildNode(pSubscriberNode3);

                // Subscribe.
                pBroadcasterNode->subscribe(pSubscriberNode1->getCallback());
                pBroadcasterNode->subscribe(pSubscriberNode2->getCallback());
                pBroadcasterNode->subscribe(pSubscriberNode3->getCallback());
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3);

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
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 1);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3);

                // Now broadcast again.
                pBroadcasterNode->broadcast();

                // ... but this time despawn subscriber 2.
                pSubscriberNode2->detachFromParentAndDespawn();
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3); // not detected yet
            } else if (iTickCount == 5) {
                REQUIRE(pSubscriberNode1->iCallbackCallCount == 2);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 2);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                getWindow()->close();
            }
        }

    private:
        gc<MyNode> pBroadcasterNode = nullptr;
        gc<MyNode> pSubscriberNode1 = nullptr;
        gc<MyNode> pSubscriberNode2 = nullptr;
        gc<MyNode> pSubscriberNode3 = nullptr;
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

TEST_CASE("broadcasters use deferred tasks to guarantee the correct call order") {
    // Imagine we have a train with a non-blocking collision node that covers
    // the train and some space around the train, and a character node that when stops
    // overlapping with this collision should be teleported back on the train (to avoid characters
    // falling off the moving train). We have a collision node with 2 broadcasters: onBeginOverlap
    // and onEndOverlap. The train subscribes to both collision events to teleport
    // players back on the train and do additional stuff. Now imagine if the character jumps and
    // starts falling off the train - this causes onEndOverlap to be broadcasted. Inside of this
    // onEndOverlap callback the train teleport the player back on the train which causes the
    // character to begin overlapping with the collision area which triggers onBeginOverlap. This
    // creates the following call stack (if we don't use deferred tasks):
    // - Train::onSomethingFinishedOverlapping
    //   we teleport the player back on the train
    // -- Train::onSomethingStartedOverlapping
    // Some other subscribers will receive notifications in an incorrect order, imagine
    // we have 3 subscribers and the train is the middle one, the call stack history will look like
    // this:
    // - subscriber1::onEndOverlap
    // - subscriber2::onEndOverlap (train)
    //   we teleport the player, onBeginOverlap is triggered
    // -- subscriber1::onBeginOverlap
    // -- subscriber2::onBeginOverlap (train)
    // -- subscriber3::onBeginOverlap
    // - subscriber3::onEndOverlap (received begin-end order when it should be end-begin).

    using namespace ne;

    static size_t iNonBroadcasterNodeDestroyedCount = 0;

    class MyNode : public Node {
    public:
        virtual ~MyNode() override {
            if (pBeginOverlapBroadcaster == nullptr || pEndOverlapBroadcaster == nullptr) {
                // Not a broadcaster node.
                REQUIRE(bBeginOverlapCalled);
                REQUIRE(bEndOverlapCalled);
                iNonBroadcasterNodeDestroyedCount += 1;
            }
        }

        void createBroadcasters() {
            pBeginOverlapBroadcaster = createNotificationBroadcaster<void()>();
            REQUIRE(pBeginOverlapBroadcaster != nullptr);

            pEndOverlapBroadcaster = createNotificationBroadcaster<void()>();
            REQUIRE(pEndOverlapBroadcaster != nullptr);
        }

        NodeFunction<void()> getOnBeginOverlapCallback() {
            return NodeFunction<void()>(getNodeId().value(), [this]() { onBeginOverlap(); });
        }

        NodeFunction<void()> getOnEndOverlapCallback() {
            return NodeFunction<void()>(getNodeId().value(), [this]() { onEndOverlap(); });
        }

        NodeFunction<void()> getOnEndOverlapCallbackThatTriggersBeginOverlap() {
            return NodeFunction<void()>(getNodeId().value(), [this]() {
                onEndOverlap();
                pBeginOverlapBroadcaster->broadcast();
            });
        }

        NodeNotificationBroadcaster<void()>* pBeginOverlapBroadcaster = nullptr;
        NodeNotificationBroadcaster<void()>* pEndOverlapBroadcaster = nullptr;

    private:
        void onBeginOverlap() {
            REQUIRE(isSpawned());

            // call order should be "end-begin" overlap.
            REQUIRE(bEndOverlapCalled);

            REQUIRE(!bBeginOverlapCalled);
            bBeginOverlapCalled = true;
        }

        void onEndOverlap() {
            REQUIRE(isSpawned());

            // call order should be "end-begin" overlap.
            REQUIRE(!bBeginOverlapCalled);

            REQUIRE(!bEndOverlapCalled);
            bEndOverlapCalled = true;
        }

        bool bBeginOverlapCalled = false;
        bool bEndOverlapCalled = false;
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
                pBroadcasterNode->createBroadcasters();
                REQUIRE(pBroadcasterNode->pBeginOverlapBroadcaster->getSubscriberCount() == 0);
                REQUIRE(pBroadcasterNode->pEndOverlapBroadcaster->getSubscriberCount() == 0);

                pSubscriberNode1 = gc_new<MyNode>();
                pSubscriberNode2 = gc_new<MyNode>();
                pSubscriberNode3 = gc_new<MyNode>();

                // only for testing purposes - don't do this in real code:
                pSubscriberNode2->pBeginOverlapBroadcaster = pBroadcasterNode->pBeginOverlapBroadcaster;

                // Now spawn nodes.
                getWorldRootNode()->addChildNode(pBroadcasterNode);
                getWorldRootNode()->addChildNode(pSubscriberNode1);
                getWorldRootNode()->addChildNode(pSubscriberNode2);
                getWorldRootNode()->addChildNode(pSubscriberNode3);

                // Subscribe to begin overlap.
                pBroadcasterNode->pBeginOverlapBroadcaster->subscribe(
                    pSubscriberNode1->getOnBeginOverlapCallback());
                pBroadcasterNode->pBeginOverlapBroadcaster->subscribe(
                    pSubscriberNode2->getOnBeginOverlapCallback());
                pBroadcasterNode->pBeginOverlapBroadcaster->subscribe(
                    pSubscriberNode3->getOnBeginOverlapCallback());
                REQUIRE(pBroadcasterNode->pBeginOverlapBroadcaster->getSubscriberCount() == 3);

                // Subscribe to end overlap.
                pBroadcasterNode->pEndOverlapBroadcaster->subscribe(
                    pSubscriberNode1->getOnEndOverlapCallback());
                pBroadcasterNode->pEndOverlapBroadcaster->subscribe(
                    pSubscriberNode2->getOnEndOverlapCallbackThatTriggersBeginOverlap());
                pBroadcasterNode->pEndOverlapBroadcaster->subscribe(
                    pSubscriberNode3->getOnEndOverlapCallback());
                REQUIRE(pBroadcasterNode->pEndOverlapBroadcaster->getSubscriberCount() == 3);

                // Broadcast end overlap (subscriber 2 should trigger begin overlap).
                pBroadcasterNode->pEndOverlapBroadcaster->broadcast(); // queues a deferred task

                // wait a few frames
                REQUIRE(iTickCount == 0);
            });
        }

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);

            iTickCount += 1;

            if (iTickCount == 3) {
                getWindow()->close();
            }
        }

    private:
        gc<MyNode> pBroadcasterNode = nullptr;
        gc<MyNode> pSubscriberNode1 = nullptr;
        gc<MyNode> pSubscriberNode2 = nullptr;
        gc<MyNode> pSubscriberNode3 = nullptr;
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

    REQUIRE(iNonBroadcasterNodeDestroyedCount == 3);
}

TEST_CASE("unsubscribe outside of a broadcast call") {
    using namespace ne;

    class MyNode : public Node {
    public:
        virtual ~MyNode() override = default;

        void createBroadcaster() {
            pBroadcaster = createNotificationBroadcaster<void()>();
            REQUIRE(pBroadcaster != nullptr);
        }

        NodeFunction<void()> getCallback() {
            return NodeFunction<void()>(getNodeId().value(), [this]() { myCallback(); });
        }

        size_t subscribe(const NodeFunction<void()>& callback) { return pBroadcaster->subscribe(callback); }

        void unsubscribe(size_t iBindingId) { pBroadcaster->unsubscribe(iBindingId); }

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
                pSubscriberNode3 = gc_new<MyNode>();

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 0);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 0);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 0);

                // Now spawn nodes.
                getWorldRootNode()->addChildNode(pBroadcasterNode);
                getWorldRootNode()->addChildNode(pSubscriberNode1);
                getWorldRootNode()->addChildNode(pSubscriberNode2);
                getWorldRootNode()->addChildNode(pSubscriberNode3);

                // Subscribe.
                const auto iSubscriber1BindingId =
                    pBroadcasterNode->subscribe(pSubscriberNode1->getCallback());
                iSubscriber2BindingId = pBroadcasterNode->subscribe(pSubscriberNode2->getCallback());
                const auto iSubscriber3BindingId =
                    pBroadcasterNode->subscribe(pSubscriberNode3->getCallback());
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3);

                REQUIRE(iSubscriber1BindingId == 0);
                REQUIRE(iSubscriber2BindingId == 1);
                REQUIRE(iSubscriber3BindingId == 2);

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
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 1);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3);

                // Now broadcast again.
                pBroadcasterNode->broadcast();

                // ... but this time unsubscribe subscriber 2
                pBroadcasterNode->unsubscribe(iSubscriber2BindingId);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);
            } else if (iTickCount == 5) {
                REQUIRE(pSubscriberNode1->iCallbackCallCount == 2);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 2);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                getWindow()->close();
            }
        }

    private:
        size_t iSubscriber2BindingId = 0;
        gc<MyNode> pBroadcasterNode = nullptr;
        gc<MyNode> pSubscriberNode1 = nullptr;
        gc<MyNode> pSubscriberNode2 = nullptr;
        gc<MyNode> pSubscriberNode3 = nullptr;
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

TEST_CASE("unsubscribe inside of a broadcast call") {
    using namespace ne;

    class MyNode : public Node {
    public:
        virtual ~MyNode() override = default;

        void createBroadcaster() {
            pBroadcaster = createNotificationBroadcaster<void()>();
            REQUIRE(pBroadcaster != nullptr);
        }

        NodeFunction<void()> getCallback() {
            return NodeFunction<void()>(getNodeId().value(), [this]() { myCallback(); });
        }

        NodeFunction<void()> getCallbackToUnsubscribe() {
            return NodeFunction<void()>(getNodeId().value(), [this]() {
                myCallback();
                pBroadcaster->unsubscribe(iBindingId);
            });
        }

        size_t subscribe(const NodeFunction<void()>& callback) { return pBroadcaster->subscribe(callback); }

        void unsubscribe(size_t iBindingId) { pBroadcaster->unsubscribe(iBindingId); }

        void broadcast() { pBroadcaster->broadcast(); }

        size_t getSubscriberCount() { return pBroadcaster->getSubscriberCount(); }

        size_t iCallbackCallCount = 0;

        size_t iBindingId = 0;
        NodeNotificationBroadcaster<void()>* pBroadcaster = nullptr;

    private:
        void myCallback() {
            REQUIRE(isSpawned());
            iCallbackCallCount += 1;
        }
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
                pSubscriberNode3 = gc_new<MyNode>();

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 0);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 0);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 0);

                // Now spawn nodes.
                getWorldRootNode()->addChildNode(pBroadcasterNode);
                getWorldRootNode()->addChildNode(pSubscriberNode1);
                getWorldRootNode()->addChildNode(pSubscriberNode2);
                getWorldRootNode()->addChildNode(pSubscriberNode3);

                // Subscribe.
                pBroadcasterNode->subscribe(pSubscriberNode1->getCallback());
                auto iSubscriber2BindingId =
                    pBroadcasterNode->subscribe(pSubscriberNode2->getCallbackToUnsubscribe());
                pBroadcasterNode->subscribe(pSubscriberNode3->getCallback());
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3);

                REQUIRE(iSubscriber2BindingId == 1); // second subscribe call

                // save data to unsubscribe
                pSubscriberNode2->iBindingId = iSubscriber2BindingId;
                // only for testing purposes - don't do this in real code:
                pSubscriberNode2->pBroadcaster = pBroadcasterNode->pBroadcaster;

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
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 1);

                // At this point, subscriber 2 should be unsubscribed.
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                // Now broadcast again.
                pBroadcasterNode->broadcast();
            } else if (iTickCount == 5) {
                REQUIRE(pSubscriberNode1->iCallbackCallCount == 2);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 2);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                getWindow()->close();
            }
        }

    private:
        gc<MyNode> pBroadcasterNode = nullptr;
        gc<MyNode> pSubscriberNode1 = nullptr;
        gc<MyNode> pSubscriberNode2 = nullptr;
        gc<MyNode> pSubscriberNode3 = nullptr;
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
