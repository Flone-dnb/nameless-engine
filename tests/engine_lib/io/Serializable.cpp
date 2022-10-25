// STL.
#include <random>

// Custom.
#include "io/Serializable.h"
#include "game/nodes/Node.h"
#include "ReflectionTest.h"
#include "io/serializers/PrimitiveFieldSerializer.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("serialize and deserialize with a backup file") {
    using namespace ne;

    const std::filesystem::path fullPathToFile =
        std::filesystem::temp_directory_path() / "TESTING_ReflectionTest_TESTING.toml";

    // Serialize to file with a backup.
    {
        InventorySaveData data;
        data.addOneItem(42);
        auto optionalError = data.serialize(fullPathToFile, true);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            INFO(optionalError->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(fullPathToFile));

        // Remove the usual file.
        std::filesystem::remove(fullPathToFile);
    }

    // Try to load using the backup.
    {
        std::unordered_map<std::string, std::string> customAttributes;
        auto result = Serializable::deserialize<InventorySaveData>(fullPathToFile, customAttributes);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }

        auto pDeserialized = std::get<gc<InventorySaveData>>(result);
        REQUIRE(pDeserialized->getItemAmount(42) == 1);

        // Check that original file was restored.
        REQUIRE(std::filesystem::exists(fullPathToFile));
    }
}

TEST_CASE("attempting to add a serializer that was previously added does nothing") {
    const auto iFieldSerializers = Serializable::getFieldSerializers().size();

    // Add already existing serializer again.
    Serializable::addFieldSerializer(std::make_unique<PrimitiveFieldSerializer>());
    REQUIRE(Serializable::getFieldSerializers().size() == iFieldSerializers);
}

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
    outerTestObj.iIntNotSerialized = 42;
    {
        ReflectionTestStruct testObj;

        testObj.bBoolValue = true;
        testObj.iIntValue = 42;
        testObj.iUnsignedIntValue = UINT_MAX;
        testObj.iLongLongValue = INT_MAX * 10ll;
        testObj.iUnsignedLongLongValue = ULLONG_MAX;
        testObj.floatValue = 3.14159f;
        testObj.doubleValue = 3.14159265358979;

        testObj.sStringValue = "Привет \"мир\""; // using non-ASCII on purpose

        testObj.vBoolVector = {true, true, false};
        testObj.vIntVector = {42, -42, 43, -43};
        testObj.vUnsignedIntVector = {UINT_MAX, INT_MAX + 1u};
        testObj.vLongLongVector = {INT_MAX * 10ll, INT_MIN * 10ll};
        testObj.vUnsignedLongLongVector = {ULLONG_MAX, ULLONG_MAX - 1};
        testObj.vFloatVector = {3.14159f, -3.14159f};
        testObj.vDoubleVector = {3.14159265358979, -3.14159265358979};
        testObj.vStringVector = {"Привет \"мир\"", "Hello \"world\""};

        testObj.mapBoolBool = {{false, false}, {true, true}};
        testObj.mapBoolInt = {{false, -1}, {true, 42}};
        testObj.mapBoolUnsignedInt = {{false, UINT_MAX}, {true, INT_MAX + 1u}};
        testObj.mapBoolLongLong = {{false, INT_MIN * 10ll}, {true, INT_MAX * 10ll}};
        testObj.mapBoolUnsignedLongLong = {{false, ULLONG_MAX}, {true, ULLONG_MAX - 1}};
        testObj.mapBoolFloat = {{false, -3.14159f}, {true, 3.14159f}};
        testObj.mapBoolDouble = {{false, -3.14159265358979}, {true, 3.14159265358979}};
        testObj.mapBoolString = {{false, "Привет \"мир\""}, {true, "Hello \"world\""}};
        testObj.mapIntBool = {{-1, false}, {42, true}};
        testObj.mapUnsignedIntBool = {{UINT_MAX, false}, {INT_MAX + 1u, true}};
        testObj.mapLongLongBool = {{INT_MIN * 10ll, false}, {INT_MAX * 10ll, true}};
        testObj.mapUnsignedLongLongBool = {{ULLONG_MAX, false}, {ULLONG_MAX - 1, true}};
        testObj.mapFloatBool = {{-3.14159f, false}, {3.14159f, true}};
        testObj.mapDoubleBool = {{-3.14159265358979, false}, {3.14159265358979, true}};
        testObj.mapStringBool = {{"Привет \"мир\"", false}, {"Hello \"world\"", true}};

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

    // Check IDs.
    const auto idResult = Serializable::getIdsFromFile(pathToFile);
    if (std::holds_alternative<Error>(idResult)) {
        auto err = std::get<Error>(std::move(idResult));
        err.addEntry();
        INFO(err.getError());
        REQUIRE(false);
    }
    const auto ids = std::get<std::set<std::string>>(idResult);
    REQUIRE(ids.size() == 1);
    REQUIRE(ids.find("0") != ids.end());

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

    constexpr auto floatDelta = 0.00001f;
    constexpr auto doubleDelta = 0.0000000000001;

    // Primitive types + std::string.
    REQUIRE(outerTestObj.iIntNotSerialized != 0);
    REQUIRE(pDeserialized->iIntNotSerialized == 0);
    REQUIRE(outerTestObj.bBoolValue == pDeserialized->bBoolValue);
    REQUIRE(outerTestObj.entity.bBoolValue == pDeserialized->entity.bBoolValue);
    REQUIRE(outerTestObj.entity.iIntValue == pDeserialized->entity.iIntValue);
    REQUIRE(outerTestObj.entity.iUnsignedIntValue == pDeserialized->entity.iUnsignedIntValue);
    REQUIRE(outerTestObj.entity.iLongLongValue == pDeserialized->entity.iLongLongValue);
    REQUIRE(outerTestObj.entity.iUnsignedLongLongValue == pDeserialized->entity.iUnsignedLongLongValue);
    REQUIRE(fabs(outerTestObj.entity.floatValue - pDeserialized->entity.floatValue) < floatDelta);
    REQUIRE(fabs(outerTestObj.entity.doubleValue - pDeserialized->entity.doubleValue) < doubleDelta);
    REQUIRE(outerTestObj.entity.sStringValue == pDeserialized->entity.sStringValue);

    // Vectors.
    REQUIRE(!outerTestObj.entity.vBoolVector.empty());
    REQUIRE(outerTestObj.entity.vBoolVector == pDeserialized->entity.vBoolVector);

    REQUIRE(!outerTestObj.entity.vIntVector.empty());
    REQUIRE(outerTestObj.entity.vIntVector == pDeserialized->entity.vIntVector);

    REQUIRE(!outerTestObj.entity.vUnsignedIntVector.empty());
    REQUIRE(outerTestObj.entity.vUnsignedIntVector == pDeserialized->entity.vUnsignedIntVector);

    REQUIRE(!outerTestObj.entity.vLongLongVector.empty());
    REQUIRE(outerTestObj.entity.vLongLongVector == pDeserialized->entity.vLongLongVector);

    REQUIRE(!outerTestObj.entity.vUnsignedLongLongVector.empty());
    REQUIRE(outerTestObj.entity.vUnsignedLongLongVector == pDeserialized->entity.vUnsignedLongLongVector);

    REQUIRE(!outerTestObj.entity.vFloatVector.empty());
    REQUIRE(outerTestObj.entity.vFloatVector.size() == pDeserialized->entity.vFloatVector.size());
    for (size_t i = 0; i < outerTestObj.entity.vFloatVector.size(); i++)
        REQUIRE(
            fabs(outerTestObj.entity.vFloatVector[i] - pDeserialized->entity.vFloatVector[i]) < floatDelta);

    REQUIRE(!outerTestObj.entity.vDoubleVector.empty());
    REQUIRE(outerTestObj.entity.vDoubleVector.size() == pDeserialized->entity.vDoubleVector.size());
    for (size_t i = 0; i < outerTestObj.entity.vDoubleVector.size(); i++)
        REQUIRE(
            fabs(outerTestObj.entity.vDoubleVector[i] - pDeserialized->entity.vDoubleVector[i]) <
            doubleDelta);

    REQUIRE(!outerTestObj.entity.vStringVector.empty());
    REQUIRE(outerTestObj.entity.vStringVector == pDeserialized->entity.vStringVector);

    REQUIRE(outerTestObj.entity.vEmpty.empty());
    REQUIRE(outerTestObj.entity.vEmpty == pDeserialized->entity.vEmpty);

    // Unordered maps.
    REQUIRE(!outerTestObj.entity.mapBoolBool.empty());
    REQUIRE(outerTestObj.entity.mapBoolBool == pDeserialized->entity.mapBoolBool);

    REQUIRE(!outerTestObj.entity.mapBoolInt.empty());
    REQUIRE(outerTestObj.entity.mapBoolInt == pDeserialized->entity.mapBoolInt);

    REQUIRE(!outerTestObj.entity.mapBoolUnsignedInt.empty());
    REQUIRE(outerTestObj.entity.mapBoolUnsignedInt == pDeserialized->entity.mapBoolUnsignedInt);

    REQUIRE(!outerTestObj.entity.mapBoolLongLong.empty());
    REQUIRE(outerTestObj.entity.mapBoolLongLong == pDeserialized->entity.mapBoolLongLong);

    REQUIRE(!outerTestObj.entity.mapBoolUnsignedLongLong.empty());
    REQUIRE(outerTestObj.entity.mapBoolUnsignedLongLong == pDeserialized->entity.mapBoolUnsignedLongLong);

    REQUIRE(!outerTestObj.entity.mapBoolFloat.empty());
    REQUIRE(outerTestObj.entity.mapBoolFloat.size() == pDeserialized->entity.mapBoolFloat.size());
    for (const auto& [key, value] : outerTestObj.entity.mapBoolFloat) {
        const auto it = pDeserialized->entity.mapBoolFloat.find(key);
        REQUIRE(it != pDeserialized->entity.mapBoolFloat.end());
        REQUIRE(key == it->first);
        REQUIRE(fabs(value - it->second) < floatDelta);
    }

    REQUIRE(!outerTestObj.entity.mapBoolDouble.empty());
    REQUIRE(outerTestObj.entity.mapBoolDouble.size() == pDeserialized->entity.mapBoolDouble.size());
    for (const auto& [key, value] : outerTestObj.entity.mapBoolDouble) {
        const auto it = pDeserialized->entity.mapBoolDouble.find(key);
        REQUIRE(it != pDeserialized->entity.mapBoolDouble.end());
        REQUIRE(key == it->first);
        REQUIRE(fabs(value - it->second) < doubleDelta);
    }

    REQUIRE(!outerTestObj.entity.mapBoolString.empty());
    REQUIRE(outerTestObj.entity.mapBoolString == pDeserialized->entity.mapBoolString);

    REQUIRE(!outerTestObj.entity.mapIntBool.empty());
    REQUIRE(outerTestObj.entity.mapIntBool == pDeserialized->entity.mapIntBool);

    REQUIRE(!outerTestObj.entity.mapUnsignedIntBool.empty());
    REQUIRE(outerTestObj.entity.mapUnsignedIntBool == pDeserialized->entity.mapUnsignedIntBool);

    REQUIRE(!outerTestObj.entity.mapLongLongBool.empty());
    REQUIRE(outerTestObj.entity.mapLongLongBool == pDeserialized->entity.mapLongLongBool);

    REQUIRE(!outerTestObj.entity.mapUnsignedLongLongBool.empty());
    REQUIRE(outerTestObj.entity.mapUnsignedLongLongBool == pDeserialized->entity.mapUnsignedLongLongBool);

    REQUIRE(!outerTestObj.entity.mapFloatBool.empty());
    REQUIRE(outerTestObj.entity.mapFloatBool.size() == pDeserialized->entity.mapFloatBool.size());
    for (const auto& [key, value] : outerTestObj.entity.mapFloatBool) {
        bool bFound = false;
        for (const auto& [otherKey, otherValue] : pDeserialized->entity.mapFloatBool) {
            if (fabs(key - otherKey) < floatDelta) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }

    REQUIRE(!outerTestObj.entity.mapDoubleBool.empty());
    REQUIRE(outerTestObj.entity.mapDoubleBool.size() == pDeserialized->entity.mapDoubleBool.size());
    for (const auto& [key, value] : outerTestObj.entity.mapDoubleBool) {
        bool bFound = false;
        for (const auto& [otherKey, otherValue] : pDeserialized->entity.mapDoubleBool) {
            if (fabs(key - otherKey) < doubleDelta) {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }

    REQUIRE(!outerTestObj.entity.mapStringBool.empty());
    REQUIRE(outerTestObj.entity.mapStringBool == pDeserialized->entity.mapStringBool);

    REQUIRE(outerTestObj.entity.mapEmpty.empty());
    REQUIRE(outerTestObj.entity.mapEmpty == pDeserialized->entity.mapEmpty);

    // Cleanup.
    std::filesystem::remove(fullPathToFile);
}

TEST_CASE("serialize and deserialize sample player save data") {
    {
        // Somewhere in the game code.
        gc<PlayerSaveData> pPlayerSaveData;

        // ... if the user creates a new player profile ...
        pPlayerSaveData = gc_new<PlayerSaveData>();

        // Fill save data with some information.
        pPlayerSaveData->sCharacterName = "Player 1";
        pPlayerSaveData->iCharacterLevel = 42;
        pPlayerSaveData->iExperiencePoints = 200;
        pPlayerSaveData->vAbilities = {241, 3122, 22};
        pPlayerSaveData->inventory.addOneItem(42);
        pPlayerSaveData->inventory.addOneItem(42); // now have two items with ID "42"
        pPlayerSaveData->inventory.addOneItem(102);

        // Prepare new profile file name.
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<unsigned int> uid(0, UINT_MAX);
        std::string sNewProfileFilename;
        const auto vExistingProfiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
        bool bContinue = true;
        do {
            sNewProfileFilename = std::to_string(uid(gen));
            for (const auto& sProfile : vExistingProfiles) {
                if (sProfile == sNewProfileFilename) {
                    continue;
                }
            }
            bContinue = false;
        } while (bContinue);

        // Serialize.
        const auto pathToFile =
            ConfigManager::getCategoryDirectory(ConfigCategory::PROGRESS) / sNewProfileFilename;
        const auto optionalError = pPlayerSaveData->serialize(pathToFile, true);
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addEntry();
            INFO(err.getError());
            REQUIRE(false);
        }

        REQUIRE(std::filesystem::exists(pathToFile.string() + ".toml"));
    }

    // ... when the game is started next time ...

    {
        // Get all save files.
        const auto vProfiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
        REQUIRE(!vProfiles.empty());

        // ... say the user picks the first profile ...
        const auto sProfileName = vProfiles[0];

        // Deserialize.
        const auto pathToFile = ConfigManager::getCategoryDirectory(ConfigCategory::PROGRESS) / sProfileName;
        std::unordered_map<std::string, std::string> foundCustomAttributes;
        const auto result = Serializable::deserialize<PlayerSaveData>(pathToFile, foundCustomAttributes);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }

        gc<PlayerSaveData> pPlayerSaveData = std::get<gc<PlayerSaveData>>(result);

        REQUIRE(pPlayerSaveData->sCharacterName == "Player 1");
        REQUIRE(pPlayerSaveData->iCharacterLevel == 42);
        REQUIRE(pPlayerSaveData->iExperiencePoints == 200);
        REQUIRE(pPlayerSaveData->vAbilities == std::vector<unsigned long long>{241, 3122, 22});
        REQUIRE(pPlayerSaveData->inventory.getItemAmount(42) == 2);
        REQUIRE(pPlayerSaveData->inventory.getItemAmount(102) == 1);
    }
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
