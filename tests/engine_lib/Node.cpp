// Custom.
#include "game/nodes/Node.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("node names should not be unique") {
    using namespace ne;

    const auto sNodeName = "Test Node Name";

    const auto pNode1 = std::make_shared<Node>(sNodeName);
    const auto pNode2 = std::make_shared<Node>(sNodeName);

    REQUIRE(pNode1->getName() == sNodeName);
    REQUIRE(pNode2->getName() == sNodeName);
}

TEST_CASE("serialize and deserialize node tree") {
    using namespace ne;

    // Prepare paths.
    const std::filesystem::path pathToFile = std::filesystem::temp_directory_path() /
                                             "TESTING_NodeTree_TESTING"; // not specifying ".toml" on purpose
    const std::filesystem::path fullPathToFile =
        std::filesystem::temp_directory_path() / "TESTING_NodeTree_TESTING.toml";

    {
        // Create nodes.
        const auto pRootNode = std::make_shared<Node>("Root Node");
        const auto pChildNode1 = std::make_shared<Node>("Child Node 1");
        const auto pChildNode2 = std::make_shared<Node>("Child Node 2");
        const auto pChildChildNode1 = std::make_shared<Node>("Child Child Node 1");

        // Build hierarchy.
        pRootNode->addChildNode(pChildNode1);
        pRootNode->addChildNode(pChildNode2);
        pChildNode1->addChildNode(pChildChildNode1);

        // Serialize.
        const auto optionalError = pRootNode->serializeNodeTree(pathToFile, false);
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addEntry();
            INFO(err.getError());
            REQUIRE(false);
        }

        REQUIRE(std::filesystem::exists(fullPathToFile));
    }

    {
        // Deserialize.
        const auto deserializeResult = Node::deserializeNodeTree(pathToFile);
        if (std::holds_alternative<Error>(deserializeResult)) {
            auto err = std::get<Error>(deserializeResult);
            err.addEntry();
            INFO(err.getError());
            REQUIRE(false);
        }
        const auto pRootNode = std::get<std::shared_ptr<Node>>(deserializeResult);

        // Check results.
        REQUIRE(pRootNode->getName() == "Root Node");
        const auto vChildNodes = pRootNode->getChildNodes();
        REQUIRE(vChildNodes.size() == 2);

        // Check child nodes.
        std::shared_ptr<Node> pChildNode1;
        std::shared_ptr<Node> pChildNode2;
        if (vChildNodes[0]->getName() == "Child Node 1") {
            REQUIRE(vChildNodes[1]->getName() == "Child Node 2");
            pChildNode1 = vChildNodes[0];
            pChildNode2 = vChildNodes[1];
        } else if (vChildNodes[0]->getName() == "Child Node 2") {
            REQUIRE(vChildNodes[1]->getName() == "Child Node 1");
            pChildNode1 = vChildNodes[1];
            pChildNode2 = vChildNodes[2];
        }

        // Check for child child nodes.
        REQUIRE(pChildNode2->getChildNodes().empty());
        const auto vChildChildNodes = pChildNode1->getChildNodes();
        REQUIRE(vChildChildNodes.size() == 1);
        REQUIRE(vChildChildNodes[0]->getChildNodes().empty());
        REQUIRE(vChildChildNodes[0]->getName() == "Child Child Node 1");
    }
}
