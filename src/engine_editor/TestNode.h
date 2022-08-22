#pragma once

#include "game/nodes/Node.h"

#include "TestNode.generated.h"

/** Test node. */
class NECLASS() TestNode : public ne::Node {
public:
    TestNode() = default;
    virtual ~TestNode() override = default;

    /** Test field. */
    NEPROPERTY()
    Node myNode;

    TestNode_GENERATED
};

File_TestNode_GENERATED