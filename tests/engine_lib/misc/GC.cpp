// Custom.
#include "misc/GC.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("moving gc pointers does not cause leaks") {
    class Collected {};

    // No object should exist.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

    {
        auto pPointer1 = gc_new<Collected>();
        gc<Collected> pPointer2;

        // Only 1 object exist.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 1);

        pPointer2 = std::move(pPointer1);

        // Still 1 object exist.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 1);
    }

    gc_collector()->fullCollect();

    // No object should exist now.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE(
    "storing an outer object not wrapped into a gc pointer that stores inner object with a gc field does not "
    "cause leaks") {
    class Collected {};

    class Inner {
    public:
        gc<Collected> pCollected;
    };

    class Outer {
    public:
        Inner inner;
    };

    // Make sure no GC object exists.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

    {
        Outer outer;
        outer.inner.pCollected = gc_new<Collected>();

        // 1 object should exist now.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 1);

        gc_collector()->fullCollect();

        // Still 1 object should exist.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 1);
    }

    gc_collector()->fullCollect();

    // No object should exist now.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE(
    "storing an outer object wrapped into a shared pointer that stores inner object with a gc field does not "
    "cause leaks") {
    class Collected {};

    class Inner {
    public:
        gc<Collected> pCollected;
    };

    class Outer {
    public:
        Inner inner;
    };

    // Make sure no GC object exists.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

    {
        const auto pOuter = std::make_shared<Outer>();
        pOuter->inner.pCollected = gc_new<Collected>();

        // 1 object should exist now.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 1);

        gc_collector()->fullCollect();

        // Still 1 object should exist.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 1);
    }

    gc_collector()->fullCollect();

    // No object should exist now.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE(
    "storing an outer object wrapped into a unique pointer that stores inner object with a gc field does not "
    "cause leaks") {
    class Collected {};

    class Inner {
    public:
        gc<Collected> pCollected;
    };

    class Outer {
    public:
        Inner inner;
    };

    // Make sure no GC object exists.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

    {
        const auto pOuter = std::make_unique<Outer>();
        pOuter->inner.pCollected = gc_new<Collected>();

        // 1 object should exist now.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 1);

        gc_collector()->fullCollect();

        // Still 1 object should exist.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 1);
    }

    gc_collector()->fullCollect();

    // No object should exist now.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("storing an std::vector of objects that have gc fields does not "
          "cause leaks") {
    class Collected {};

    struct MyData {
        void foo() { pCollected = gc_new<Collected>(); }

    private:
        gc<Collected> pCollected;
    };

    // Make sure no GC object exists.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

    {
        std::vector<MyData> vMyData; // intentionally not using `gc_vector`

        // Make sure no GC object exists.
        REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

        constexpr size_t iDataSize = 10;
        for (size_t i = 0; i < iDataSize; i++) {
            MyData data;
            data.foo();
            vMyData.push_back(std::move(data));
        }

        // Objects should exist now.
        REQUIRE(gc_collector()->getAliveObjectsCount() == iDataSize);

        gc_collector()->fullCollect();

        // Still the same.
        REQUIRE(gc_collector()->getAliveObjectsCount() == iDataSize);
    }

    gc_collector()->fullCollect();

    // No object should exist now.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("storing an std::vector of objects that have gc fields and another std::vector of the same objects "
          "that reference first array does not cause leaks") {
    class Collected {};

    struct MyData {
        gc<Collected> pCollected;
    };

    // Make sure no GC object exists.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

    {
        constexpr size_t iDataSize = 5;

        std::vector<MyData> vMyDataRef; // intentionally not using `gc_vector`

        {
            std::vector<MyData> vMyDataOriginal; // intentionally not using `gc_vector`

            // Make sure no GC object exists.
            REQUIRE(gc_collector()->getAliveObjectsCount() == 0);

            for (size_t i = 0; i < iDataSize; i++) {
                MyData data1;
                data1.pCollected = gc_new<Collected>();
                MyData data2;
                data2.pCollected = data1.pCollected;

                vMyDataOriginal.push_back(std::move(data1));
                vMyDataRef.push_back(std::move(data2));
            }

            REQUIRE(gc_collector()->getAliveObjectsCount() == iDataSize);
        }

        gc_collector()->fullCollect();

        REQUIRE(gc_collector()->getAliveObjectsCount() == iDataSize);
    }

    gc_collector()->fullCollect();

    // No object should exist now.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}
