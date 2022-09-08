﻿#include "game/nodes/Node.h"

namespace ne {
    Node::Node() { sNodeName = "Node"; }

    Node::Node(const std::string& sNodeName) { this->sNodeName = sNodeName; }

    void Node::setNodeName(const std::string& sNodeName) {
        this->sNodeName = sNodeName;
        // TODO
    }

    std::string Node::getNodeName() const { return sNodeName; }
} // namespace ne