// Custom.
#include "io/Serializable.h"
#include "game/nodes/Node.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("serialize and deserialize node") {
    using namespace ne;

    const std::filesystem::path pathToFile =
        std::filesystem::temp_directory_path() /
        "TESTING_MyCoolNode_TESTING"; // not specifying ".toml" on purpose
    const std::filesystem::path fullPathToFile =
        std::filesystem::temp_directory_path() / "TESTING_MyCoolNode_TESTING.toml";

    // Remove this file if exists.
    if (std::filesystem::exists(fullPathToFile)) {
        std::filesystem::remove(fullPathToFile);
    }

    REQUIRE(!std::filesystem::exists(fullPathToFile));

    // Serialize.
    Node node("My Cool Node");
    const auto optionalError = node.serialize(pathToFile, false);
    if (optionalError.has_value()) {
        auto err = optionalError.value();
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }

    REQUIRE(std::filesystem::exists(fullPathToFile));

    // Deserialize.
    const auto result = Serializable::deserialize<Node>(pathToFile);
    if (std::holds_alternative<Error>(result)) {
        auto err = std::get<Error>(std::move(result));
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }
    const auto pDeserializedNode = std::get<std::shared_ptr<Node>>(std::move(result));

    // Check that name is the same.
    REQUIRE(pDeserializedNode->getName() == node.getName());

    // Cleanup.
    std::filesystem::remove(fullPathToFile);
}

TEST_CASE("serialize and deserialize multiple nodes") {
    using namespace ne;

    const std::filesystem::path pathToFile =
        std::filesystem::temp_directory_path() /
        "TESTING_MyCoolNode_TESTING"; // not specifying ".toml" on purpose
    const std::filesystem::path fullPathToFile =
        std::filesystem::temp_directory_path() / "TESTING_MyCoolNode_TESTING.toml";

    // Remove this file if exists.
    if (std::filesystem::exists(fullPathToFile)) {
        std::filesystem::remove(fullPathToFile);
    }

    REQUIRE(!std::filesystem::exists(fullPathToFile));

    // Serialize.
    Node node1("My Cool Node 1");
    Node node2("My Cool Node 2");
    const auto optionalError = Serializable::serialize(pathToFile, {{&node1, "0"}, {&node2, "1"}}, false);
    if (optionalError.has_value()) {
        auto err = optionalError.value();
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }

    REQUIRE(std::filesystem::exists(fullPathToFile));

    // Deserialize.
    const auto result = Serializable::deserialize(pathToFile, {"0", "1"});
    if (std::holds_alternative<Error>(result)) {
        auto err = std::get<Error>(std::move(result));
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }
    const auto vDeserializedObjects = std::get<std::vector<std::shared_ptr<Serializable>>>(std::move(result));

    // Check results.
    REQUIRE(vDeserializedObjects.size() == 2);

    const auto pNode1 = std::dynamic_pointer_cast<Node>(vDeserializedObjects[0]);
    const auto pNode2 = std::dynamic_pointer_cast<Node>(vDeserializedObjects[1]);

    REQUIRE(pNode1 != nullptr);
    REQUIRE(pNode2 != nullptr);
    REQUIRE(pNode1->getName() == node1.getName());
    REQUIRE(pNode2->getName() == node2.getName());

    // Cleanup.
    std::filesystem::remove(fullPathToFile);
}
