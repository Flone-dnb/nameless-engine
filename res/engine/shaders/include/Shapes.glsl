// Include this file if you need to do calculations on shapes (planes, frustums, etc.).

/** Plane represented by a normal and a distance. */
struct Plane{
    /** Plane's normal. */
    vec3 normal;

    /** Distance from the origin to the nearest point on the plane. */
    float distanceFromOrigin;
};

/** Frustum in view space. */
struct Frustum{
    /** Left, right, top and bottom faces of the frustum. */
    Plane planes[4];
};

/**
 * Calculates a plane from 3 non-collinear points that form a triangle.
 *
 * @remark Expects right-handed coordinate system.
 *
 * @param point0 1st point of a triangle.
 * @param point1 2nd point of a triangle. A vector from this point to the first one is the first argument in the cross product.
 * @param point2 3rd point of a triangle. A vector from this point to the first one is the second argument in the cross product.
 *
 * @return Calculated plane.
 */
Plane calculatePlaneFromTriangle(vec3 point0, vec3 point1, vec3 point2){
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