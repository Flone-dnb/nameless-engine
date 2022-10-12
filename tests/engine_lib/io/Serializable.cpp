// Custom.
#include "io/Serializable.h"
#include "game/nodes/Node.h"
#include "ReflectionTest.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("serialize and deserialize fields of different types") {
    using namespace ne;

    // Prepare data.
    const std::filesystem::path pathToFile =
        std::filesystem::temp_directory_path() /
        "TESTING_ReflectionTest_TESTING"; // not specifying ".toml" on purpose
    const std::filesystem::path fullPathToFile =
        std::filesystem::temp_directory_path() / "TESTING_ReflectionTest_TESTING.toml";

    // Create test instance with some fields.
    ReflectionOuterTestClass outerTestObj;
    outerTestObj.bBoolValue = true;
    {
        ReflectionTestClass testObj;
        testObj.bBoolValue = true;
        testObj.iIntValue = 42;
        testObj.iLongLongValue = INT_MAX * 10ll;
        testObj.floatValue = 3.14159f;
        testObj.doubleValue = 3.14159265358979;
        testObj.sStringValue = "Привет мир"; // using non-ASCII on purpose
        testObj.vBoolVector = {true, true, false};
        testObj.vIntVector = {42, -42, 43, -43};
        testObj.vLongLongVector = {INT_MAX * 10ll, INT_MAX * -10ll};
        testObj.vFloatVector = {3.14159f, -3.14159f};
        testObj.vDoubleVector = {3.14159265358979, -3.14159265358979};
        testObj.vStringVector = {"Привет мир", "Hello world"};

        outerTestObj.entity = testObj;
    }

    // Serialize.
    auto optionalError = outerTestObj.serialize(pathToFile, false);
    if (optionalError.has_value()) {
        auto err = optionalError.value();
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }

    REQUIRE(std::filesystem::exists(fullPathToFile));

    // Deserialize.
    std::unordered_map<std::string, std::string> customAttributes;
    auto result = Serializable::deserialize<ReflectionOuterTestClass>(pathToFile, customAttributes);
    if (std::holds_alternative<Error>(result)) {
        auto err = std::get<Error>(std::move(result));
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }

    const auto pDeserialized = std::get<gc<ReflectionOuterTestClass>>(result);

    // Compare results.

    // Primitive types + std::string.
    REQUIRE(outerTestObj.bBoolValue == pDeserialized->bBoolValue);
    REQUIRE(outerTestObj.entity.bBoolValue == pDeserialized->entity.bBoolValue);
    REQUIRE(outerTestObj.entity.iIntValue == pDeserialized->entity.iIntValue);
    REQUIRE(outerTestObj.entity.iLongLongValue == pDeserialized->entity.iLongLongValue);
    REQUIRE(fabs(outerTestObj.entity.floatValue - pDeserialized->entity.floatValue) < 0.00001f);
    REQUIRE(fabs(outerTestObj.entity.doubleValue - pDeserialized->entity.doubleValue) < 0.0000000000001);
    REQUIRE(outerTestObj.entity.sStringValue == pDeserialized->entity.sStringValue);

    // Vectors.
    REQUIRE(outerTestObj.entity.vBoolVector == pDeserialized->entity.vBoolVector);
    REQUIRE(outerTestObj.entity.vIntVector == pDeserialized->entity.vIntVector);
    REQUIRE(outerTestObj.entity.vLongLongVector == pDeserialized->entity.vLongLongVector);
    REQUIRE(outerTestObj.entity.vFloatVector.size() == pDeserialized->entity.vFloatVector.size());
    for (size_t i = 0; i < outerTestObj.entity.vFloatVector.size(); i++)
        REQUIRE(fabs(outerTestObj.entity.vFloatVector[i] - pDeserialized->entity.vFloatVector[i]) < 0.00001f);
    REQUIRE(outerTestObj.entity.vDoubleVector.size() == pDeserialized->entity.vDoubleVector.size());
    for (size_t i = 0; i < outerTestObj.entity.vDoubleVector.size(); i++)
        REQUIRE(
            fabs(outerTestObj.entity.vDoubleVector[i] - pDeserialized->entity.vDoubleVector[i]) <
            0.0000000000001);
    REQUIRE(outerTestObj.entity.vStringVector == pDeserialized->entity.vStringVector);

    // Cleanup.
    std::filesystem::remove(fullPathToFile);
}

TEST_CASE("serialize and deserialize node") {
    using namespace ne;

    // Prepare data.
    const std::filesystem::path pathToFile =
        std::filesystem::temp_directory_path() /
        "TESTING_MyCoolNode_TESTING"; // not specifying ".toml" on purpose
    const std::filesystem::path fullPathToFile =
        std::filesystem::temp_directory_path() / "TESTING_MyCoolNode_TESTING.toml";
    const auto sCustomAttributeName = "Test Attribute";
    const auto sCustomAttributeValue = "142";

    // Remove this file if exists.
    if (std::filesystem::exists(fullPathToFile)) {
        std::filesystem::remove(fullPathToFile);
    }

    REQUIRE(!std::filesystem::exists(fullPathToFile));

    // Serialize.
    Node node("My Cool Node");
    std::unordered_map<std::string, std::string> serializeCustomAttributes = {
        {sCustomAttributeName, sCustomAttributeValue}};
    const auto optionalError = node.serialize(pathToFile, false, serializeCustomAttributes);
    if (optionalError.has_value()) {
        auto err = optionalError.value();
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }

    REQUIRE(std::filesystem::exists(fullPathToFile));

    // Deserialize.
    std::unordered_map<std::string, std::string> deserializeCustomAttributes;
    const auto result = Serializable::deserialize<Node>(pathToFile, deserializeCustomAttributes);
    if (std::holds_alternative<Error>(result)) {
        auto err = std::get<Error>(std::move(result));
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }
    const auto pDeserializedNode = std::get<gc<Node>>(std::move(result));

    // Check that name is the same.
    REQUIRE(pDeserializedNode->getName() == node.getName());

    // Check custom attributes.
    REQUIRE(deserializeCustomAttributes.size() == serializeCustomAttributes.size());
    REQUIRE(deserializeCustomAttributes.find(sCustomAttributeName) != deserializeCustomAttributes.end());
    REQUIRE(deserializeCustomAttributes[sCustomAttributeName] == sCustomAttributeValue);
    REQUIRE(
        deserializeCustomAttributes[sCustomAttributeName] == serializeCustomAttributes[sCustomAttributeName]);

    // Cleanup.
    std::filesystem::remove(fullPathToFile);
}

TEST_CASE("serialize and deserialize multiple nodes") {
    using namespace ne;

    // Prepare data.
    const std::filesystem::path pathToFile =
        std::filesystem::temp_directory_path() /
        "TESTING_MyCoolNode_TESTING"; // not specifying ".toml" on purpose
    const std::filesystem::path fullPathToFile =
        std::filesystem::temp_directory_path() / "TESTING_MyCoolNode_TESTING.toml";
    const auto sNode1CustomAttributeName = "node1_attribute";
    const auto sNode2CustomAttributeName = "node2_attribute";

    // Remove this file if exists.
    if (std::filesystem::exists(fullPathToFile)) {
        std::filesystem::remove(fullPathToFile);
    }

    REQUIRE(!std::filesystem::exists(fullPathToFile));

    // Serialize.
    Node node1("My Cool Node 1");
    Node node2("My Cool Node 2");
    SerializableObjectInformation node1Info(&node1, "0", {{sNode1CustomAttributeName, "1"}});
    SerializableObjectInformation node2Info(&node2, "1", {{sNode2CustomAttributeName, "2"}});
    const auto optionalError = Serializable::serialize(pathToFile, {node1Info, node2Info}, false);
    if (optionalError.has_value()) {
        auto err = optionalError.value();
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }

    REQUIRE(std::filesystem::exists(fullPathToFile));

    // Check IDs.
    const auto idResult = Serializable::getIdsFromFile(pathToFile);
    if (std::holds_alternative<Error>(idResult)) {
        auto err = std::get<Error>(std::move(idResult));
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }
    const auto ids = std::get<std::set<std::string>>(idResult);
    REQUIRE(ids.size() == 2);
    REQUIRE(ids.find("0") != ids.end());
    REQUIRE(ids.find("1") != ids.end());

    // Deserialize.
    const auto result = Serializable::deserialize(pathToFile, {"0", "1"});
    if (std::holds_alternative<Error>(result)) {
        auto err = std::get<Error>(std::move(result));
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }
    std::vector<DeserializedObjectInformation> vDeserializedObjects =
        std::get<std::vector<DeserializedObjectInformation>>(std::move(result));

    // Check results.
    REQUIRE(vDeserializedObjects.size() == 2);

    const auto pNode1 = gc<Node>(dynamic_cast<Node*>(&*vDeserializedObjects[0].pObject));
    const auto pNode2 = gc<Node>(dynamic_cast<Node*>(&*vDeserializedObjects[1].pObject));

    REQUIRE(pNode1 != nullptr);
    REQUIRE(pNode2 != nullptr);
    REQUIRE(pNode1->getName() == node1.getName());
    REQUIRE(pNode2->getName() == node2.getName());

    // Check custom attributes.
    REQUIRE(vDeserializedObjects[0].customAttributes.size() == 1);
    REQUIRE(vDeserializedObjects[1].customAttributes.size() == 1);
    REQUIRE(
        vDeserializedObjects[0].customAttributes.find(sNode1CustomAttributeName) !=
        vDeserializedObjects[0].customAttributes.end());
    REQUIRE(
        vDeserializedObjects[1].customAttributes.find(sNode2CustomAttributeName) !=
        vDeserializedObjects[1].customAttributes.end());
    REQUIRE(vDeserializedObjects[0].customAttributes[sNode1CustomAttributeName] == "1");
    REQUIRE(vDeserializedObjects[1].customAttributes[sNode2CustomAttributeName] == "2");

    // Cleanup.
    std::filesystem::remove(fullPathToFile);
}
