#pragma once

// Custom.
#include "game/node/Node.h"

namespace ne {
    /** Helper tools for creation nodes in the editor. */
    class EditorNodeCreationHelpers {
    public:
        EditorNodeCreationHelpers() = delete;

        /**
         * Creates an editor-owned node (not a game-owned node) with specific node settings like
         * no node serialization and etc. so that it won't affect the game.
         *
         * @param sNodeName Name of the node.
         *
         * @return Created node.
         */
        template <typename NodeType>
            requires std::derived_from<NodeType, Node>
        static inline sgc::GcPtr<NodeType> createEditorNode(const std::string& sNodeName) {
            // Create node.
            auto pCreatedNode = sgc::makeGc<NodeType>(sNodeName);

            // Disable serialization so that it won't be serialized as part of the game world.
            pCreatedNode->setSerialize(false);

            return pCreatedNode;
        }
    };
}
