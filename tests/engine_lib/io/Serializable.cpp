// STL.
#include <random>

// Custom.
#include "misc/ProjectPaths.h"
#include "io/Serializable.h"
#include "game/nodes/Node.h"
#include "io/ConfigManager.h"
#include "ReflectionTest.h"
#include "io/serializers/PrimitiveFieldSerializer.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("make sure relative path to the file the object was deserialized from is valid") {
    const std::string sRelativePathToFile = "test/test.toml";

    // Prepare paths to the file.
    const auto pathToFileInRes =
        ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / sRelativePathToFile;
    const std::filesystem::path pathToFileInTemp =
        std::filesystem::temp_directory_path() / "TESTING_ReflectionTest_TESTING.toml";

    // Serialize into the `res` directory.
    InventorySaveData data;
    data.addOneItem(42);
    auto optionalError = data.serialize(pathToFileInRes, true);
    if (optionalError.has_value()) {
        optionalError->addEntry();
        INFO(optionalError->getError());
        REQUIRE(false);
    }

    // Additionally serialize outside of the `res` directory.
    optionalError = data.serialize(pathToFileInTemp, false);
    if (optionalError.has_value()) {
        optionalError->addEntry();
        INFO(optionalError->getError());
        REQUIRE(false);
    }

    // Check that file exists.
    REQUIRE(std::filesystem::exists(pathToFileInRes));

    // Remove the usual file to check that resulting relative path will point to the original file
    // and not the backup file.
    std::filesystem::remove(pathToFileInRes);

    // Try to load using the backup file.
    auto result = Serializable::deserialize<InventorySaveData>(pathToFileInRes);
    if (std::holds_alternative<Error>(result)) {
        auto error = std::get<Error>(result);
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    // Check that original file was restored.
    REQUIRE(std::filesystem::exists(pathToFileInRes));

    // Make sure that deserialized data is correct.
    auto pDeserialized = std::get<gc<InventorySaveData>>(result);
    REQUIRE(pDeserialized->getItemAmount(42) == 1);

    // Check that relative path exists and correct.
    const auto optionalRelativePath = pDeserialized->getPathDeserializedFromRelativeToRes();
    REQUIRE(optionalRelativePath.has_value());
    REQUIRE(optionalRelativePath.value().first == sRelativePathToFile);

    // Load the data from the temp directory.
    result = Serializable::deserialize<InventorySaveData>(pathToFileInTemp);
    if (std::holds_alternative<Error>(result)) {
        auto error = std::get<Error>(result);
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    // Make sure that deserialized data is correct.
    pDeserialized = std::get<gc<InventorySaveData>>(result);
    REQUIRE(pDeserialized->getItemAmount(42) == 1);

    // Check that relative path is empty.
    REQUIRE(!pDeserialized->getPathDeserializedFromRelativeToRes().has_value());

    // Cleanup.
    std::filesystem::remove_all(ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / "test");
    std::filesystem::remove(pathToFileInTemp);
}

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
        auto result = Serializable::deserialize<InventorySaveData>(fullPathToFile);
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

TEST_CASE("deserialize a node tree that references external node") {
    // Prepare paths.
    const std::string sNodeTreeRelativePathToFile = "test/node_tree.toml";
    const auto pathToNodeTreeFileInRes =
        ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / sNodeTreeRelativePathToFile;
    const std::string sCustomNodeRelativePathToFile = "test/custom_node.toml";
    const auto pathToCustomNodeFileInRes =
        ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / sCustomNodeRelativePathToFile;

    if (!std::filesystem::exists(ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / "test")) {
        std::filesystem::create_directory(
            ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / "test");
    }

    {
        // Say we have a custom node.
        ReflectionTestNode1 embedableNode;

        REQUIRE(!embedableNode.bBoolValue1);
        REQUIRE(!embedableNode.bBoolValue2);
        REQUIRE(!embedableNode.entity.iIntValue1);
        REQUIRE(!embedableNode.entity.iIntValue2);
        REQUIRE(embedableNode.entity.vVectorValue1.empty());

        embedableNode.bBoolValue1 = true; // change 1 field
        auto optionalError = embedableNode.serialize(pathToCustomNodeFileInRes, false);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            INFO(optionalError->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(pathToCustomNodeFileInRes));
    }

    {
        // Now let's say we are building a new node tree and want to use this custom node.
        // Deserialize this custom node.
        auto result = Serializable::deserialize<ReflectionTestNode1>(pathToCustomNodeFileInRes);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }

        auto pDeserializedNode = std::get<gc<ReflectionTestNode1>>(result);

        // Check that deserialized object now has a path relative to the `res` directory.
        REQUIRE(pDeserializedNode->getPathDeserializedFromRelativeToRes().has_value());
        REQUIRE(
            pDeserializedNode->getPathDeserializedFromRelativeToRes().value().first ==
            sCustomNodeRelativePathToFile);

        // Build a node tree.
        gc<Node> pParentNode = gc_new<Node>();
        pParentNode->addChildNode(pDeserializedNode);

        // Change some fields so that we will see it in the TOML file as changed.
        pDeserializedNode->bBoolValue2 = true;
        pDeserializedNode->entity.iIntValue2 = 42;
        pDeserializedNode->entity.vVectorValue2 = {"Hello", "World"};

        // Now serialize this node tree.
        auto optionalError = pParentNode->serializeNodeTree(pathToNodeTreeFileInRes, false);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            INFO(optionalError->getError());
            REQUIRE(false);
        }
    }

    {
        // In node tree's TOML file our custom node should be referenced as a path to the file.
        // Deserialize our node tree.
        auto result = Node::deserializeNodeTree(pathToNodeTreeFileInRes);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }

        const auto pRootNode = std::get<gc<Node>>(result);

        // Check that deserialized object now has a path relative to the `res` directory.
        REQUIRE(pRootNode->getPathDeserializedFromRelativeToRes().has_value());
        REQUIRE(
            pRootNode->getPathDeserializedFromRelativeToRes().value().first == sNodeTreeRelativePathToFile);

        // Get our child node.
        REQUIRE(pRootNode->getChildNodes()->size() == 1);
        auto pChildNode = gc_dynamic_pointer_cast<ReflectionTestNode1>(pRootNode->getChildNodes()[0]);
        REQUIRE(pChildNode);

        // Check that everything is deserialized correctly.
        REQUIRE(pChildNode->bBoolValue1);
        REQUIRE(pChildNode->bBoolValue2);
        REQUIRE(pChildNode->entity.iIntValue1 == 0);
        REQUIRE(pChildNode->entity.iIntValue2 == 42);
        REQUIRE(pChildNode->entity.vVectorValue1.empty());
        REQUIRE(pChildNode->entity.vVectorValue2 == std::vector<std::string>{"Hello", "World"});

        // Now look at the TOML file of our node tree and make sure that only changed
        // fields were written in it.
        ConfigManager configManager;
        auto optionalError = configManager.loadFile(pathToNodeTreeFileInRes);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }

        // Find a section that starts with "1."
        const auto vSectionNames = configManager.getAllSections();
        REQUIRE(vSectionNames.size() == 3);
        std::string sTargetSectionName;
        for (const auto& sSectionName : vSectionNames) {
            if (sSectionName.starts_with("1.")) {
                sTargetSectionName = sSectionName;
                break;
            }
        }

        REQUIRE(!sTargetSectionName.empty());

        // Check that this section has changed field.
        constexpr auto sFirstFieldName = "bBoolValue1";
        constexpr auto sSecondFieldName = "bBoolValue2";
        constexpr auto sPathKeyName = ".path_relative_to_res";

        const auto bBoolValue2 = configManager.getValue(sTargetSectionName, sSecondFieldName, false);
        REQUIRE(bBoolValue2);

        // And check that this section does not have unchanged field.
        const auto bBoolValue1 = configManager.getValue(sTargetSectionName, sFirstFieldName, false);
        REQUIRE(!bBoolValue1);

        // Double check this and check that path to original node is there.
        auto keysResult = configManager.getAllKeysOfSection(sTargetSectionName);
        if (std::holds_alternative<Error>(keysResult)) {
            auto error = std::get<Error>(keysResult);
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }
        auto vKeys = std::get<std::vector<std::string>>(std::move(keysResult));

        bool bFoundFirstField = false;
        bool bFoundSecondField = false;
        bool bFoundPathToNode = false;
        for (const auto& sKey : vKeys) {
            if (sKey == sFirstFieldName) {
                bFoundFirstField = true;
            } else if (sKey == sSecondFieldName) {
                bFoundSecondField = true;
            } else if (sKey == sPathKeyName) {
                bFoundPathToNode = true;
            }
        }

        REQUIRE(bFoundSecondField);
        REQUIRE(!bFoundFirstField);
        REQUIRE(bFoundPathToNode);

        // Compare path to original node.
        const auto sPathToNodeRelativeToRes =
            configManager.getValue<std::string>(sTargetSectionName, sPathKeyName, "");
        REQUIRE(sCustomNodeRelativePathToFile == sPathToNodeRelativeToRes);

        // Find section that starts with "1.0.".
        sTargetSectionName = "";
        for (const auto& sSectionName : vSectionNames) {
            if (sSectionName.starts_with("1.0.")) {
                sTargetSectionName = sSectionName;
                break;
            }
        }

        REQUIRE(!sTargetSectionName.empty());

        // Check changed fields.
        constexpr auto sIntValue2FieldName = "iIntValue2";
        constexpr auto sVectorValue2FieldName = "vVectorValue2";

        const auto iIntValue2 = configManager.getValue(sTargetSectionName, sIntValue2FieldName, 0);
        REQUIRE(iIntValue2 == 42);

        const auto vVectorValue2 =
            configManager.getValue<std::vector<std::string>>(sTargetSectionName, sVectorValue2FieldName, {});
        REQUIRE(vVectorValue2 == std::vector<std::string>{"Hello", "World"});

        // Check changed field count.
        keysResult = configManager.getAllKeysOfSection(sTargetSectionName);
        if (std::holds_alternative<Error>(keysResult)) {
            auto error = std::get<Error>(keysResult);
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }
        vKeys = std::get<std::vector<std::string>>(std::move(keysResult));

        size_t iChangedFieldsCount = 0;
        for (const auto& sKeyName : vKeys) {
            if (sKeyName.starts_with("."))
                continue;

            iChangedFieldsCount += 1;
        }

        REQUIRE(iChangedFieldsCount == 2);
    }

    // Cleanup.
    std::filesystem::remove_all(ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / "test");
    gc_collector()->collect();
    REQUIRE(Node::getAliveNodeCount() == 0);
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("deserialize a node tree that references external node tree") {
    // Prepare paths.
    const std::string sNodeTreeRelativePathToFile = "test/node_tree.toml";
    const auto pathToNodeTreeFileInRes =
        ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / sNodeTreeRelativePathToFile;
    const std::string sCustomNodeTreeRelativePathToFile = "test/custom_node_tree.toml";
    const auto pathToCustomNodeTreeFileInRes =
        ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / sCustomNodeTreeRelativePathToFile;

    if (!std::filesystem::exists(ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / "test")) {
        std::filesystem::create_directory(
            ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / "test");
    }

    {
        // Say we have a custom node tree.
        gc<Node> pRootNode = gc_new<Node>();
        auto pChildNode = gc_new<ReflectionTestNode1>();

        // Build node tree.
        pRootNode->addChildNode(pChildNode);

        REQUIRE(!pChildNode->bBoolValue1);
        REQUIRE(!pChildNode->bBoolValue2);
        REQUIRE(!pChildNode->entity.iIntValue1);
        REQUIRE(!pChildNode->entity.iIntValue2);
        REQUIRE(pChildNode->entity.vVectorValue1.empty());

        pChildNode->bBoolValue1 = true; // change 1 field

        auto optionalError = pRootNode->serializeNodeTree(pathToCustomNodeTreeFileInRes, false);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            INFO(optionalError->getError());
            REQUIRE(false);
        }

        // Check that file exists.
        REQUIRE(std::filesystem::exists(pathToCustomNodeTreeFileInRes));
    }

    {
        // Now let's say we are building a new node tree and want to use this custom node tree.
        // Deserialize this custom node tree.
        auto result = Node::deserializeNodeTree(pathToCustomNodeTreeFileInRes);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }

        auto pDeserializedRootNode = std::get<gc<Node>>(result);

        // Check that deserialized object now has a path relative to the `res` directory.
        REQUIRE(pDeserializedRootNode->getPathDeserializedFromRelativeToRes().has_value());
        REQUIRE(
            pDeserializedRootNode->getPathDeserializedFromRelativeToRes().value().first ==
            sCustomNodeTreeRelativePathToFile);

        // Check children.
        REQUIRE(pDeserializedRootNode->getChildNodes()->size() == 1);
        const auto pChildNode =
            gc_dynamic_pointer_cast<ReflectionTestNode1>(pDeserializedRootNode->getChildNodes()[0]);
        REQUIRE(pChildNode);
        REQUIRE(pChildNode->bBoolValue1);
        REQUIRE(!pChildNode->bBoolValue2);
        REQUIRE(pChildNode->getPathDeserializedFromRelativeToRes().has_value());
        REQUIRE(
            pChildNode->getPathDeserializedFromRelativeToRes().value().first ==
            sCustomNodeTreeRelativePathToFile);

        // Build a new node tree and reference our custom node tree.
        gc<Node> pParentNode = gc_new<Node>();
        pParentNode->addChildNode(pDeserializedRootNode);

        // Change some fields, we will not see them in the TOML file because
        // when referencing a node tree, only root node will save it's changed values.
        pChildNode->bBoolValue2 = true;
        pChildNode->entity.iIntValue2 = 42;
        pChildNode->entity.vVectorValue2 = {"Hello", "World"};

        // Change external root node's name (we will see this in the TOML file).
        pDeserializedRootNode->setName("External Root Node");

        // Now serialize this node tree.
        auto optionalError = pParentNode->serializeNodeTree(pathToNodeTreeFileInRes, false);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            INFO(optionalError->getError());
            REQUIRE(false);
        }
    }

    {
        // In node tree's TOML file our custom node tree should be referenced as a path to the file.
        // Deserialize our node tree.
        auto result = Node::deserializeNodeTree(pathToNodeTreeFileInRes);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addEntry();
            INFO(error.getError());
            REQUIRE(false);
        }

        const auto pRootNode = std::get<gc<Node>>(result);

        // Get our child node.
        REQUIRE(pRootNode->getChildNodes()->size() == 1);
        REQUIRE(pRootNode->getPathDeserializedFromRelativeToRes().has_value());
        REQUIRE(
            pRootNode->getPathDeserializedFromRelativeToRes().value().first == sNodeTreeRelativePathToFile);

        auto pChildNode = pRootNode->getChildNodes()[0];
        REQUIRE(pChildNode->getPathDeserializedFromRelativeToRes().has_value());
        REQUIRE(
            pChildNode->getPathDeserializedFromRelativeToRes().value().first == sNodeTreeRelativePathToFile);
        REQUIRE(!pChildNode->getPathDeserializedFromRelativeToRes().value().second.starts_with("0."));
        REQUIRE(pChildNode->getName() == "External Root Node");

        // Get child child nodes.
        REQUIRE(pChildNode->getChildNodes()->size() == 1);
        auto pChildChildNode = gc_dynamic_pointer_cast<ReflectionTestNode1>(pChildNode->getChildNodes()[0]);
        REQUIRE(pChildChildNode);

        // Check that everything is deserialized correctly.
        REQUIRE(pChildChildNode->bBoolValue1);
        REQUIRE(pChildChildNode->bBoolValue2 == false);
        REQUIRE(pChildChildNode->entity.iIntValue1 == 0);
        REQUIRE(pChildChildNode->entity.iIntValue2 == 0);
        REQUIRE(pChildChildNode->entity.vVectorValue1.empty());
        REQUIRE(pChildChildNode->entity.vVectorValue2.empty());
    }

    // Cleanup.
    std::filesystem::remove_all(ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / "test");
    gc_collector()->collect();
    REQUIRE(Node::getAliveNodeCount() == 0);
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
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
    auto result = Serializable::deserialize<ReflectionOuterTestClass>(pathToFile);
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
        bool bContinue = false;
        do {
            bContinue = false;
            sNewProfileFilename = std::to_string(uid(gen));
            for (const auto& sProfile : vExistingProfiles) {
                if (sProfile == sNewProfileFilename) {
                    // This profile name is already used, generate another one.
                    bContinue = true;
                    break;
                }
            }
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
