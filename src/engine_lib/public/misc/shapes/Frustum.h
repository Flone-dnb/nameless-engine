#pragma once

// Custom.
#include "math/GLMath.hpp"
#include "misc/shapes/Plane.h"
#include "misc/shapes/AABB.h"
#include "misc/shapes/Sphere.h"
#include "misc/shapes/Cone.h"

namespace ne {
    /** Frustum represented by 6 planes. */
    struct Frustum {
        /**
         * Tests if the specified axis-aligned bounding box is inside of the frustum or intersects it.
         *
         * @remark Does frustum/AABB intersection in world space.
         *
         * @param aabbInModelSpace Axis-aligned bounding box in model space.
         * @param worldMatrix      Matrix that transforms the specified AABB from model space to world space.
         *
         * @return `true` if the AABB is inside of the frustum or intersects it, `false` if the AABB is
         * outside of the frustum.
         */
        bool isAabbInFrustum(const AABB& aabbInModelSpace, const glm::mat4x4& worldMatrix) const;

        /**
         * Tests if the specified sphere is inside of the frustum or intersects it.
         *
         * @remark Expects that both frustum and sphere are in the same coordinate space.
         *
         * @param sphere Sphere to test.
         *
         * @return `true` if the sphere is inside of the frustum or intersects it, `false` if the sphere is
         * outside of the frustum.
         */
        bool isSphereInFrustum(const Sphere& sphere) const;

        /**
         * Tests if the specified cone is inside of the frustum or intersects it.
         *
         * @remark Expects that both frustum and cone are in the same coordinate space.
         *
         * @param cone Cone to test.
         *
         * @return `true` if the cone is inside of the frustum or intersects it, `false` if the cone is
         * outside of the frustum.
         */
        bool isConeInFrustum(const Cone& cone) const;

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
