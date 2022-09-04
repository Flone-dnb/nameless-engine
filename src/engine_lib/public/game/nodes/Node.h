#pragma once

// Custom.
#include "io/Serializable.h"

#include "Node.generated.h"

namespace ne NENAMESPACE() {
    /**
     * Base Node class, allows attaching child nodes or being attached to some parent node.
     *
     * @warning If changing the class name class ID (of this class) will change and will make all previously
     * serialized instances of this class reference old (invalid) class ID. Include a backwards compatibility
     * in this case.
     */
    class NECLASS() Node : public Serializable {
    public:
        /** Creates a node with the name "Node". */
        Node();

        /**
         * Creates a node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        Node(const std::string& sNodeName);

        virtual ~Node() override = default;

        /**
         * Sets node's name.
         *
         * @param sNodeName New name of this node.
         */
        NEFUNCTION()
        void setNodeName(const std::string& sNodeName);

        /**
         * Returns node's name.
         *
         * @return Node name.
         */
        NEFUNCTION()
        std::string getNodeName() const;

    private:
        /** Node name. */
        NEPROPERTY()
        std::string sNodeName;

        ne_Node_GENERATED
    };
} // namespace )

File_Node_GENERATED