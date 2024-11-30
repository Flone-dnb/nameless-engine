// Custom.
#include "game/node/Node.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/node/callback/NodeNotificationBroadcaster.hpp"

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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pBroadcasterNode = sgc::makeGc<MyNode>();
                pBroadcasterNode->createBroadcaster();
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 0);

                auto pSubscriberNode1 = sgc::makeGc<MyNode>();
                auto pSubscriberNode2 = sgc::makeGc<MyNode>();
                auto pSubscriberNode3 = sgc::makeGc<MyNode>();

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

                // Make sure callbacks were triggered.
                REQUIRE(pSubscriberNode1->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 1);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3);

                // Now despawn node 2.
                pSubscriberNode2->detachFromParentAndDespawn();
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3); // not detected yet

                // ... and broadcast again.
                pBroadcasterNode->broadcast();

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 2);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 2);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                getWindow()->close();
            });
        }
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pBroadcasterNode = sgc::makeGc<MyNode>();
                pBroadcasterNode->createBroadcaster();
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 0);

                auto pSubscriberNode1 = sgc::makeGc<MyNode>();
                auto pSubscriberNode2 = sgc::makeGc<MyNode>();
                auto pSubscriberNode3 = sgc::makeGc<MyNode>();

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
                const auto iSubscriber2BindingId =
                    pBroadcasterNode->subscribe(pSubscriberNode2->getCallback());
                const auto iSubscriber3BindingId =
                    pBroadcasterNode->subscribe(pSubscriberNode3->getCallback());
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3);

                REQUIRE(iSubscriber1BindingId == 0);
                REQUIRE(iSubscriber2BindingId == 1);
                REQUIRE(iSubscriber3BindingId == 2);

                // Broadcast.
                pBroadcasterNode->broadcast();

                // Make sure callbacks were triggered.
                REQUIRE(pSubscriberNode1->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 1);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 3);

                // Unsubscribe subscriber 2.
                pBroadcasterNode->unsubscribe(iSubscriber2BindingId);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                // Now broadcast again.
                pBroadcasterNode->broadcast();

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 2);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 2);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                getWindow()->close();
            });
        }
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pBroadcasterNode = sgc::makeGc<MyNode>();
                pBroadcasterNode->createBroadcaster();
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 0);

                auto pSubscriberNode1 = sgc::makeGc<MyNode>();
                auto pSubscriberNode2 = sgc::makeGc<MyNode>();
                auto pSubscriberNode3 = sgc::makeGc<MyNode>();

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
                const auto iSubscriber2BindingId =
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

                // Make sure callbacks were triggered.
                REQUIRE(pSubscriberNode1->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 1);

                // At this point, subscriber 2 should be unsubscribed.
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                // Now broadcast again.
                pBroadcasterNode->broadcast();

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 2);
                REQUIRE(pSubscriberNode2->iCallbackCallCount == 1);
                REQUIRE(pSubscriberNode3->iCallbackCallCount == 2);
                REQUIRE(pBroadcasterNode->getSubscriberCount() == 2);

                getWindow()->close();
            });
        }
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

TEST_CASE("pass GC pointer as a broadcast argument") {
    using namespace ne;

    class MyNode : public Node {
    public:
        virtual ~MyNode() override = default;

        void createBroadcaster() {
            pBroadcaster = createNotificationBroadcaster<void(const sgc::GcPtr<MyNode>&)>();
            REQUIRE(pBroadcaster != nullptr);
        }

        NodeFunction<void(const sgc::GcPtr<MyNode>&)> getCallback() {
            return NodeFunction<void(const sgc::GcPtr<MyNode>&)>(
                getNodeId().value(), [this](const sgc::GcPtr<MyNode>& pSomeNode) { myCallback(pSomeNode); });
        }

        size_t iCallbackCallCount = 0;

        NodeNotificationBroadcaster<void(const sgc::GcPtr<MyNode>&)>* pBroadcaster = nullptr;

        void useNode() {
            REQUIRE(pSomeNode != nullptr);
            pSomeNode->sSomePrivateString = "It seems to work.";
        }

    private:
        void myCallback(const sgc::GcPtr<MyNode>& pSomeNode) {
            REQUIRE(isSpawned());
            iCallbackCallCount += 1;

            this->pSomeNode = pSomeNode;
        }

        sgc::GcPtr<MyNode> pSomeNode = nullptr;

        std::string sSomePrivateString = "Hello";
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pBroadcasterNode = sgc::makeGc<MyNode>();
                pBroadcasterNode->createBroadcaster();

                auto pSubscriberNode1 = sgc::makeGc<MyNode>();

                // Now spawn nodes.
                getWorldRootNode()->addChildNode(pBroadcasterNode);
                getWorldRootNode()->addChildNode(pSubscriberNode1);

                // Subscribe.
                pBroadcasterNode->pBroadcaster->subscribe(pSubscriberNode1->getCallback());

                // Broadcast.
                pBroadcasterNode->pBroadcaster->broadcast(pBroadcasterNode);

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 1);
                pSubscriberNode1->useNode();

                getWindow()->close();
            });
        }
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

TEST_CASE("call broadcast inside of another broadcast call") {
    using namespace ne;

    class MyNode : public Node {
    public:
        virtual ~MyNode() override = default;

        void createBroadcaster() {
            pBroadcaster = createNotificationBroadcaster<void(const sgc::GcPtr<MyNode>&)>();
            REQUIRE(pBroadcaster != nullptr);
        }

        NodeFunction<void(const sgc::GcPtr<MyNode>&)> getCallback() {
            return NodeFunction<void(const sgc::GcPtr<MyNode>&)>(
                getNodeId().value(), [this](const sgc::GcPtr<MyNode>& pSomeNode) { myCallback(pSomeNode); });
        }

        size_t iCallbackCallCount = 0;

        NodeNotificationBroadcaster<void(const sgc::GcPtr<MyNode>&)>* pBroadcaster = nullptr;

    private:
        void myCallback(const sgc::GcPtr<MyNode>& pSomeNode) {
            REQUIRE(isSpawned());
            iCallbackCallCount += 1;

            if (iCallbackCallCount == 1) {
                pBroadcaster->broadcast(pSomeNode);
            }
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pBroadcasterNode = sgc::makeGc<MyNode>();
                pBroadcasterNode->createBroadcaster();

                auto pSubscriberNode1 = sgc::makeGc<MyNode>();

                // Now spawn nodes.
                getWorldRootNode()->addChildNode(pBroadcasterNode);
                getWorldRootNode()->addChildNode(pSubscriberNode1);

                // Subscribe.
                pBroadcasterNode->pBroadcaster->subscribe(pSubscriberNode1->getCallback());

                // only for testing purposes - don't do this in real code:
                pSubscriberNode1->pBroadcaster = pBroadcasterNode->pBroadcaster;

                // Broadcast.
                pBroadcasterNode->pBroadcaster->broadcast(pBroadcasterNode);

                REQUIRE(pSubscriberNode1->iCallbackCallCount == 2);

                getWindow()->close();
            });
        }
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
