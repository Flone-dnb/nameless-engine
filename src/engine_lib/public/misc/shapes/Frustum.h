#pragma once

// Custom.
#include "math/GLMath.hpp"
#include "misc/shapes/Plane.h"
#include "misc/shapes/AABB.h"

namespace ne {
    /** Frustum represented by 6 planes. */
    struct Frustum {
        /**
         * Tests if the specified axis-aligned bounding box is inside of the frustum or intersects it.
         *
         * @param aabbInModelSpace Axis-aligned bounding box in model space.
         * @param worldMatrix      Matrix that transforms the specified AABB from model space to world space.
         *
         * @return `true` if AABB is inside of the frustum or intersects it, `false` if AABB is outside
         * of the frustum.
         */
        bool isAabbInFrustum(const AABB& aabbInModelSpace, const glm::mat4x4& worldMatrix) const;

        /** Top face of the frustum that points inside of the frustum volume. */
        Plane topFace;

        /** Bottom face of the frustum that points inside of the frustum volume. */
        Plane bottomFace;

        /** Right face of the frustum that points inside of the frustum volume. */
        Plane rightFace;

        /** Left face of the frustum that points inside of the frustum volume. */
        Plane leftFace;

        /** Near face of the frustum that points inside of the frustum volume. */
        Plane nearFace;

        /** Far face of the frustum that points inside of the frustum volume. */
        Plane farFace;
    };
}
