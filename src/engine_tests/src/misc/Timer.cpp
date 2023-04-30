// Standard.
#include <future>

// Custom.
#include "misc/Timer.h"
#include "game/nodes/Node.h"
#include "io/Logger.h"
#include "game/GameInstance.h"
#include "game/Window.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("measure elapsed time") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);
        };
        virtual ~MyDerivedNode() override = default;

    protected:
        virtual void onSpawning() override {
            Node::onSpawning();

            using namespace std::chrono;
            constexpr long long iDeltaInMs = 5;

            pTimer->start();

            while (!pTimer->getElapsedTimeInMs().has_value()) {
            }
            REQUIRE(pTimer->getElapsedTimeInMs().has_value());

            const auto start = steady_clock::now();
            std::this_thread::sleep_for(milliseconds(50)); // NOLINT
            const auto iElapsedInMs =
                std::chrono::duration_cast<milliseconds>(steady_clock::now() - start).count();

            const auto iTimerElapsedInMs = pTimer->getElapsedTimeInMs().value();

            pTimer->stop();

            REQUIRE(iElapsedInMs <= iTimerElapsedInMs);
            REQUIRE(iElapsedInMs > iTimerElapsedInMs - iDeltaInMs);

            std::this_thread::sleep_for(milliseconds(50)); // NOLINT

            // Make sure the time can still be queries after stopping.
            REQUIRE(pTimer->getElapsedTimeInMs().has_value());
            REQUIRE(pTimer->getElapsedTimeInMs().value() == iTimerElapsedInMs);

            getGameInstance()->getWindow()->close();
        }

    private:
        Timer* pTimer = nullptr;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                getWorldRootNode()->addChildNode(gc_new<MyDerivedNode>());
            });
        }
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("measure elapsed time of a looping timer") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);

            pTimer->setCallbackForTimeout(
                iLoopTimeInterval,
                [this]() {
                    const auto optionalTimeElapsed = pTimer->getElapsedTimeInMs();
                    REQUIRE(optionalTimeElapsed.has_value());

                    const auto iTimePassed = optionalTimeElapsed.value();
                    REQUIRE(iTimePassed <= iLoopTimeInterval * 2);

                    iCallbackCount += 1;
                },
                true);

            pTimer->start();
        }

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);

            if (iCallbackCount == iTargetLoopCount) {
                getWindow()->close();
            }
        }

    private:
        Timer* pTimer = nullptr;
        size_t iCallbackCount = 0;
        long long iLoopTimeInterval = 100;
        const size_t iTargetLoopCount = 10;
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("run callback on timeout") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);
        };
        virtual ~MyDerivedNode() override { REQUIRE(bCallbackCalled); }

    protected:
        virtual void onSpawning() override {
            Node::onSpawning();

            pTimer->setCallbackForTimeout(50, [this]() {
                REQUIRE(isSpawned());

                bCallbackCalled = true;

                pTimer->stop();

                getGameInstance()->getWindow()->close();
            });

            pTimer->start();
        }

    private:
        Timer* pTimer = nullptr;
        bool bCallbackCalled = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                getWorldRootNode()->addChildNode(gc_new<MyDerivedNode>());
            });
        }
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("check that timer is running (without callback)") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);
        };
        virtual ~MyDerivedNode() override = default;

    protected:
        virtual void onSpawning() override {
            Node::onSpawning();

            using namespace std::chrono;
            constexpr size_t iCheckIntervalTimeInMs = 15;

            pTimer->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            pTimer->stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(!pTimer->isRunning());
            REQUIRE(pTimer->isStopped());

            getGameInstance()->getWindow()->close();
        }

    private:
        Timer* pTimer = nullptr;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                getWorldRootNode()->addChildNode(gc_new<MyDerivedNode>());
            });
        }
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("check that timer is running (with callback, force stop)") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() { setIsCalledEveryFrame(true); };
        virtual ~MyDerivedNode() override = default;

    protected:
        virtual void onSpawning() override {
            Node::onSpawning();

            // Intentionally create the timer in `onSpawning` to test if that works.
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);

            constexpr long long iWaitTime = 100;
            using namespace std::chrono;
            constexpr size_t iCheckIntervalTimeInMs = 15;

            pTimer->setCallbackForTimeout(iWaitTime, []() {
                // Should not be called.
                REQUIRE(false);
            });

            pTimer->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime / 2));

            REQUIRE(pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            pTimer->stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(!pTimer->isRunning());
            REQUIRE(pTimer->isStopped());

            // Wait a few ticks to make sure that the callback will not be started.
        }

        virtual void onBeforeNewFrame(float timeSincePrevCallInSec) override {
            Node::onBeforeNewFrame(timeSincePrevCallInSec);

            iTickCount += 1;

            if (iTickCount == 10) {
                getGameInstance()->getWindow()->close();
            }
        }

    private:
        Timer* pTimer = nullptr;
        size_t iTickCount = 0;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                getWorldRootNode()->addChildNode(gc_new<MyDerivedNode>());
            });
        }
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("check that timer is running (with callback)") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() = default;
        virtual ~MyDerivedNode() override { REQUIRE(bCallbackCalled); };

    protected:
        virtual void onSpawning() override {
            Node::onSpawning();

            // Intentionally create the timer in `onSpawning` to test if that works.
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);

            constexpr long long iWaitTime = 100;
            constexpr size_t iCheckIntervalTimeInMs = 15;
            using namespace std::chrono;

            pTimer->setCallbackForTimeout(iWaitTime, [this]() {
                REQUIRE(isSpawned());

                bCallbackCalled = true;

                pTimer->stop();

                getGameInstance()->getWindow()->close();
            });

            pTimer->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime));

            REQUIRE(!pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());
        }

    private:
        Timer* pTimer = nullptr;
        bool bCallbackCalled = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                getWorldRootNode()->addChildNode(gc_new<MyDerivedNode>());
            });
        }
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("callback validator should prevent callback being executed after despawn") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);
        };
        virtual ~MyDerivedNode() override = default;

    protected:
        virtual void onSpawning() override {
            Node::onSpawning();

            constexpr long long iWaitTime = 30;
            constexpr size_t iCheckIntervalTimeInMs = 15;
            using namespace std::chrono;

            // This will queue a deferred task on timeout.
            pTimer->setCallbackForTimeout(iWaitTime, []() {
                // This should not happen in this test.
                REQUIRE(false);
            });

            pTimer->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime * 2));

            // Timeout should submit a deferred task at this point.
            REQUIRE(!pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());
        }

    private:
        Timer* pTimer = nullptr;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const auto pNode = gc_new<MyDerivedNode>();
                getWorldRootNode()->addChildNode(pNode); // do Node::onSpawning logic

                // Timeout submitted a deferred task, despawn now.
                pNode->detachFromParentAndDespawn();

                // On next game tick when deferred tasks are executed the callback should not be called.
                iTickCount = 0;
            });
        }

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);

            iTickCount += 1;

            if (iTickCount == 10) {
                // Seems like timer's callback was not called, done.
                getWindow()->close();
            }
        }

    private:
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("callback validator should prevent callback being executed after despawning and spawning again "
          "with another timer start") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);
        };
        virtual ~MyDerivedNode() override = default;

    protected:
        virtual void onSpawning() override {
            Node::onSpawning();

            constexpr long long iWaitTime = 30;
            constexpr size_t iCheckIntervalTimeInMs = 15;
            using namespace std::chrono;

            // This will queue a deferred task on timeout.
            pTimer->setCallbackForTimeout(iWaitTime, []() {
                // This should not happen in this test.
                REQUIRE(false);
            });

            pTimer->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime * 2));

            // Timeout should submit a deferred task at this point.
            REQUIRE(!pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());
        }

    private:
        Timer* pTimer = nullptr;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const auto pNode = gc_new<MyDerivedNode>();
                getWorldRootNode()->addChildNode(pNode); // do Node::onSpawning logic

                // Timeout submitted a deferred task, despawn now.
                pNode->detachFromParentAndDespawn();

                // Now spawn again.
                getWorldRootNode()->addChildNode(pNode); // run timer again

                // Despawn.
                pNode->detachFromParentAndDespawn();

                // On next game tick when deferred tasks are executed the callback should not be called.
                iTickCount = 0;
            });
        }

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);

            iTickCount += 1;

            if (iTickCount == 10) {
                // Seems like timer's callback was not called, done.
                getWindow()->close();
            }
        }

    private:
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("callback validator should prevent callback being executed after despawning and spawning again") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);
        };
        virtual ~MyDerivedNode() override = default;

        void runTimer() {
            constexpr long long iWaitTime = 30;
            constexpr size_t iCheckIntervalTimeInMs = 15;
            using namespace std::chrono;

            // This will queue a deferred task on timeout.
            pTimer->setCallbackForTimeout(iWaitTime, []() {
                // This should not happen in this test.
                REQUIRE(false);
            });

            pTimer->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime * 2));

            // Timeout should submit a deferred task at this point.
            REQUIRE(!pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());
        }

    private:
        Timer* pTimer = nullptr;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const auto pNode = gc_new<MyDerivedNode>();
                getWorldRootNode()->addChildNode(pNode);
                pNode->runTimer();

                // Timeout submitted a deferred task, despawn now.
                pNode->detachFromParentAndDespawn();

                // Now spawn again.
                getWorldRootNode()->addChildNode(pNode); // run timer again

                // Don't despawn, this time there's no timer.

                // On next game tick when deferred tasks are executed the callback should not be called.
                iTickCount = 0;
            });
        }

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);

            iTickCount += 1;

            if (iTickCount == 10) {
                // Seems like timer's callback was not called, done.
                getWindow()->close();
            }
        }

    private:
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("callback validator should prevent previous callback being executed after starting a new one") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);
        };
        virtual ~MyDerivedNode() override { REQUIRE(bExpectedCallbackWasCalled); }

        void runTimer(bool bExpectToBeCalled) {
            constexpr long long iWaitTime = 30;
            constexpr size_t iCheckIntervalTimeInMs = 15;
            using namespace std::chrono;

            // This will queue a deferred task on timeout.
            pTimer->setCallbackForTimeout(iWaitTime, [this, bExpectToBeCalled]() {
                if (bExpectToBeCalled) {
                    bExpectedCallbackWasCalled = true;
                } else {
                    REQUIRE(false);
                }
            });

            pTimer->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime * 2));

            // Timeout should submit a deferred task at this point.
            REQUIRE(!pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());
        }

    private:
        Timer* pTimer = nullptr;
        bool bExpectedCallbackWasCalled = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const auto pNode = gc_new<MyDerivedNode>();
                getWorldRootNode()->addChildNode(pNode);

                pNode->runTimer(false);
                pNode->runTimer(true); // previous callback should not be called

                iTickCount = 0;
            });
        }

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);

            iTickCount += 1;

            if (iTickCount == 10) {
                getWindow()->close();
            }
        }

    private:
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("stopping looping timer from callback") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override { REQUIRE(iCallbackCount == iTargetLoopCount); }
        virtual void onGameStarted() override {
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);

            pTimer->setCallbackForTimeout(
                iLoopTimeInterval,
                [this]() {
                    const auto optionalTimeElapsed = pTimer->getElapsedTimeInMs();
                    REQUIRE(optionalTimeElapsed.has_value());

                    const auto iTimePassed = optionalTimeElapsed.value();
                    REQUIRE(iTimePassed <= iLoopTimeInterval * 2);

                    iCallbackCount += 1;
                    REQUIRE(iCallbackCount <= iTargetLoopCount);

                    if (iCallbackCount == iTargetLoopCount) {
                        pTimer->stop();
                        timerStop = std::chrono::steady_clock::now();
                    }
                },
                true);

            pTimer->start();
        }

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);

            if (iCallbackCount == iTargetLoopCount) {
                const auto iTimePassedAfterStop = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                      std::chrono::steady_clock::now() - timerStop)
                                                      .count();

                if (iTimePassedAfterStop >= iTimeToWaitAfterTimerStop) {
                    getWindow()->close();
                    return;
                }
            }
        }

    private:
        Timer* pTimer = nullptr;
        size_t iCallbackCount = 0;
        std::chrono::steady_clock::time_point timerStop;
        long long iLoopTimeInterval = 30;
        const size_t iTargetLoopCount = 10;
        const long long iTimeToWaitAfterTimerStop = iLoopTimeInterval * 10;
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}
