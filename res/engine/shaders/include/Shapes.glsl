/** Plane represented by a normal and a distance. */
struct Plane {
    /** Plane's normal. */
    vec3 normal;

    /** Distance from the origin to the nearest point on the plane. */
    float distanceFromOrigin;
};

/** Sphere shape. */
struct Sphere {
    /** Location of the sphere's center point. */
    vec3 center;

    /** Sphere's radius. */
    float radius;
};

/** Cone shape. */
struct Cone {
    /** Location of cone's tip. */
    vec3 location;

    /** Height of the cone. */
    float height;

    /** Direction unit vector from cone's tip. */
    vec3 direction;

    /** Radius of the bottom part of the cone. */
    float bottomRadius;
};

/** Frustum in view space. */
struct Frustum {
    /** Left, right, top and bottom faces of the frustum. */
    Plane planes[4];
};

/**
 * Calculates a plane from 3 non-collinear points that form a triangle.
 *
 * @param point0 1st point of a triangle.
 * @param point1 2nd point of a triangle. A vector from this point to the first one is the first argument in the cross product.
 * @param point2 3rd point of a triangle. A vector from this point to the first one is the second argument in the cross product.
 *
 * @return Calculated plane.
 */
Plane calculatePlaneFromTriangle(vec3 point0, vec3 point1, vec3 point2) {
    // Prepare output variable.
    Plane plane;

    // Calculate directions to 1st and 2nd points.
    vec3 toFirst = point1 - point0;
    vec3 toSecond = point2 - point0;

    // Calculate plane normal.
    plane.normal = normalize(cross(toFirst, toSecond));

    // Calculate distance from the origin.
    plane.distanceFromOrigin = dot(plane.normal, point0);

    return plane;
}

/**
 * Constructs a plane.
 *
 * @param normal   Plane's normal (expected to be of unit length).
 * @param location Location of a point that lies on the plane.
 *
 * @return Created plane.
 */
Plane createPlane(vec3 normal, vec3 location) {
    // Prepare output variable.
    Plane plane;

    // Calculate fields.
    plane.normal = normal;
    plane.distanceFromOrigin = dot(normal, location);

    return plane;
}

/**
 * Tells if the sphere is fully behind (inside the negative halfspace of) a plane.
 *
 * @param sphere Sphere to test.
 * @param plane  Plane to test.
 *
 * @return `true` if the sphere is fully behind the plane, `false` otherwise.
 */
bool isSphereBehindPlane(Sphere sphere, Plane plane) {
    // Source: Real-time collision detection, Christer Ericson (2005).
    return dot(plane.normal, sphere.center) - plane.distanceFromOrigin < -sphere.radius;
}

/**
 * Tells if the point is fully behind (inside the negative halfspace of) a plane.
 *
 * @param pointToTest Point to test.
 * @param plane       Plane to test.
 *
 * @return `true` if the point is fully behind the plane, `false` otherwise.
 */
// apparently `point` is a keyword in HLSL so we can't use that name as arg name
bool isPointBehindPlane(vec3 pointToTest, Plane plane) {
    // Source: Real-time collision detection, Christer Ericson (2005).
    return dot(plane.normal, pointToTest) - plane.distanceFromOrigin < 0.0F;
}

/**
 * Tells if the cone is fully behind (inside the negative halfspace of) a plane.
 *
 * @param cone  Cone to test.
 * @param plane Plane to test.
 *
 * @return `true` if the cone is fully behind the plane, `false` otherwise.
 */
bool isConeBehindPlane(Cone cone, Plane plane) {
    // Source: Real-time collision detection, Christer Ericson (2005).

    // Calculate an intermediate vector which is parallel but opposite to plane's normal and perpendicular
    // to the cone's direction.
    vec3 m = cross(cross(plane.normal, cone.direction), cone.direction);

    // Calculate the point Q that is on the base (bottom) of the cone that is farthest away from the plane
    // in the direction of plane's normal.
    vec3 q = cone.location + cone.direction * cone.height - m * cone.bottomRadius;

    // The cone is behind the plane if both cone's tip and Q are behind the plane.
    return isPointBehindPlane(cone.location, plane) && isPointBehindPlane(q, plane);
}

/**
 * Tells if the specified sphere is fully inside of the specified frustum or partially contained within the frustum.
 *
 * @remark Expects that far Z points in the positive Z direction, in other words: zNear < zFar.
 *
 * @param sphere       Sphere to test.
 * @param frustum      Frustum to test.
 * @param frustumZNear Near clip plane of the frustum.
 * @param frustumZFar  Far clip plane of the frustum.
 *
 * @return `true` if the sphere is inside of the frustum or intersects some of frustum's planes, `false` otherwise.
 */
bool isSphereInsideFrustum(Sphere sphere, Frustum frustum, float frustumZNear, float frustumZFar) {
    // Prepare output variable.
    bool bIsInside = true;

    // Test sphere against frustum near/far clip planes.
    if (sphere.center.z - sphere.radius > frustumZFar || sphere.center.z + sphere.radius < frustumZNear) {
        bIsInside = false;
    }

    // Test sphere agains frustum planes.
    for (int i = 0; i < 4 && bIsInside; i++) {
        if (isSphereBehindPlane(sphere, frustum.planes[i])) {
            bIsInside = false;
        }
    }

    return bIsInside;
}

/**
 * Tells if the specified cone is fully inside of the specified frustum or partially contained within the frustum.
 *
 * @param cone         Cone to test.
 * @param frustum      Frustum to test.
 * @param frustumZNear Near clip plane of the frustum.
 * @param frustumZFar  Far clip plane of the frustum.
 *
 * @return `true` if the cone is inside of the frustum or intersects some of frustum's planes, `false` otherwise.
 */
bool isConeInsideFrustum(Cone cone, Frustum frustum, float frustumZNear, float frustumZFar) {
    // Prepare output variable.
    bool bIsInside = true;

    // Construct frustum near/far planes.
    Plane frustumNearPlane = createPlane(vec3(0.0F, 0.0F, 1.0F), vec3(0.0F, 0.0F, frustumZNear));
    Plane frustumFarPlane = createPlane(vec3(0.0F, 0.0F, -1.0F), vec3(0.0F, 0.0F, frustumZFar));

    // Test cone against frustum near/far clip planes.
    if (isConeBehindPlane(cone, frustumNearPlane) || isConeBehindPlane(cone, frustumFarPlane)) {
        bIsInside = false;
    }

    // Test sphere agains frustum planes.
    for (int i = 0; i < 4 && bIsInside; i++) {
        if (isConeBehindPlane(cone, frustum.planes[i])) {
            bIsInside = false;
        }
    }

    return bIsInside;
}