// Standard.
#include <future>

// Custom.
#include "misc/Timer.h"
#include "io/Logger.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("measure elapsed time") {
    using namespace ne;
    using namespace std::chrono;
    constexpr long long iDeltaInMs = 5;

    Timer timer;
    timer.start(true);
    while (!timer.getElapsedTimeInMs().has_value()) {
    }

    const auto start = steady_clock::now();
    std::this_thread::sleep_for(milliseconds(50)); // NOLINT
    const auto iElapsedInMs = std::chrono::duration_cast<milliseconds>(steady_clock::now() - start).count();

    const auto iTimerElapsedInMs = timer.getElapsedTimeInMs().value();
    timer.stop(true);
    REQUIRE(iElapsedInMs <= iTimerElapsedInMs);
    REQUIRE(iElapsedInMs > iTimerElapsedInMs - iDeltaInMs);
    REQUIRE(!timer.getElapsedTimeInMs().has_value());
}

TEST_CASE("run callback on timeout") {
    using namespace ne;
    Timer timer;
    auto pPromiseFinish = std::make_shared<std::promise<bool>>();
    auto future = pPromiseFinish->get_future();
    constexpr auto bExpected = true;

    timer.setCallbackForTimeout(1, [bExpected, pPromiseFinish]() { pPromiseFinish->set_value(bExpected); });
    timer.start(true);

    REQUIRE(future.valid());
    REQUIRE(future.get() == bExpected);
}

TEST_CASE("check that timer is running") {
    using namespace ne;

    constexpr size_t iCheckIntervalTimeInMs = 15;
    Timer timer;

    SECTION("without callback") {
        timer.start(true);

        std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));
        REQUIRE(timer.isRunning());

        timer.stop(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));
        REQUIRE(!timer.isRunning());
    }
    SECTION("with callback (force stop)") {
        constexpr long long iWaitTime = 50;
        timer.setCallbackForTimeout(iWaitTime, []() {});
        timer.start(true);

        std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime / 2));
        REQUIRE(timer.isRunning());

        timer.stop(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));
        REQUIRE(!timer.isRunning());
    }
    SECTION("with callback") {
        constexpr long long iWaitTime = 50;
        timer.setCallbackForTimeout(iWaitTime, []() {});
        timer.start(true);

        std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime / 2));
        REQUIRE(timer.isRunning());

        std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime));
        REQUIRE(!timer.isRunning());
    }
}

TEST_CASE("wait for callback to finish on timer destruction") {
    using namespace ne;

    const auto pPromiseFinish = std::make_shared<std::promise<bool>>();
    const auto futureFinish = pPromiseFinish->get_future();

    const auto pPromiseStart = std::make_shared<std::promise<bool>>();
    auto futureStart = pPromiseStart->get_future();

    constexpr auto iCallbackSleepTimeInMs =
        30; // NOLINT: should be way bigger than iWaitTimeForCallbackToStartInMs
    constexpr long long iWaitTimeForCallbackToStartInMs =
        1; // should be way lower than iCallbackSleepTimeInMs

    {
        Timer timer(false); // ignore warning about waiting time being too long
        timer.setCallbackForTimeout(iWaitTimeForCallbackToStartInMs, [&]() {
            pPromiseStart->set_value(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(iCallbackSleepTimeInMs));
            pPromiseFinish->set_value(true);
        });
        timer.start(true);

        futureStart.get();
        // The callback is currently running...
        // The timer will wait here until the callback is finished.
    }

    // The callback should be finished now.
    REQUIRE(futureFinish.valid());
    REQUIRE(futureFinish.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
}
