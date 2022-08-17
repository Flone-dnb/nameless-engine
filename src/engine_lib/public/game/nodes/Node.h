#pragma once

#include "io/Serializable.h"

#include "Node.generated.h"

namespace ne NENAMESPACE() {
    /**
     * Base Node class, allows attaching child nodes or being attached to some parent node.
     */
    class NECLASS() Node : public Serializable {
    public:
        /** Creates a node with a name "Node". */
        Node();

        /**
         * Creates a node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        Node(const std::string& sNodeName);

        virtual ~Node() override = default;

        /**
         * Returns node's name.
         *
         * @return Node name.
         */
        std::string getNodeName() const;

    private:
        NEPROPERTY()
        int iNodeProperty = 43;

        NEPROPERTY()
        std::string sNodeName;

        ne_Node_GENERATED
    };
} // namespace )

File_Node_GENERATED