#include "game/nodes/Node.h"

namespace ne {
    Node::Node() { sName = "Node"; }

    Node::Node(const std::string& sName) { this->sName = sName; }

    void Node::setName(const std::string& sName) { this->sName = sName; }

    std::string Node::getName() const { return sName; }
} // namespace ne
