#pragma once

// External.
#if defined(WIN32)
#include "DirectXMath.h"
#else
#include "math/GLMath.hpp"
#endif

namespace ne {
    /** Represents a 3D vector. */
    class Vector {
    public:
        /** Initializes the vector with zeros. */
        inline Vector();

        /**
         * Copy constructor.
         */
        inline Vector(const Vector&) = default;

        /**
         * Copy assignment operator.
         *
         * @return Vector.
         */
        inline Vector& operator=(const Vector&) = default;

        /**
         * Initializes the vector with the given components.
         *
         * @param x X component of the vector.
         * @param y Y component of the vector.
         * @param z Z component of the vector.
         */
        inline Vector(float x, float y, float z); // NOLINT

        /**
         * Sets the X component of the vector.
         *
         * @param x X component of the vector.
         */
        constexpr inline void setX(float x); // NOLINT

        /**
         * Sets the Y component of the vector.
         *
         * @param y Y component of the vector.
         */
        constexpr inline void setY(float y); // NOLINT

        /**
         * Sets the Z component of the vector.
         *
         * @param z Z component of the vector.
         */
        constexpr inline void setZ(float z); // NOLINT

        /**
         * Normalizes the vector.
         */
        inline void normalize();

        /**
         * Rotates this vector around the given axis.
         *
         * @param axis       Axis to rotate the vector around.
         * @param angleInRad Angle of rotation in radians.
         */
        inline void rotateAroundAxis(const Vector& axis, float angleInRad);

        /**
         * Returns the result of the dot product between this vector and another one.
         *
         * @param other Other vector for the dot product.
         *
         * @return Result of the dot product.
         */
        inline float dotProduct(const Vector& other) const;

        /**
         * Calculates the cross product between this vector and another one.
         *
         * @param other Other vector.
         *
         * @return Result of the cross product.
         */
        inline Vector crossProduct(const Vector& other) const;

        /**
         * Calculates the projection of this vector onto another vector.
         *
         * @param other Vector to project onto.
         *
         * @return Projected vector.
         */
        inline Vector projectOnto(const Vector& other) const;

        /**
         * Calculates the angle in radians between this vector and the given vector.
         *
         * @param other Other vector.
         *
         * @return Angle in radians between this vector and the given vector.
         */
        inline float getAngleBetweenVectorsInRad(const Vector& other) const;

        /**
         * Calculates the angle in radians between two normalized vectors: this vector and the given vector.
         *
         * @remark Assumes both this and the other vector are normalized, otherwise
         * use @ref getAngleBetweenVectorsInRad.
         *
         * @param other Other vector.
         *
         * @return Angle in radians between this vector and the given vector.
         */
        inline float getAngleBetweenNormalizedVectorsInRad(const Vector& other) const;

        /**
         * Returns the X component of the vector.
         *
         * @return X component of the vector.
         */
        constexpr inline float getX() const;

        /**
         * Returns the Y component of the vector.
         *
         * @return Y component of the vector.
         */
        constexpr inline float getY() const;

        /**
         * Returns the Z component of the vector.
         *
         * @return Z component of the vector.
         */
        constexpr inline float getZ() const;

        /**
         * Returns the length of the vector.
         *
         * @return Length of the vector.
         */
        inline float getLength() const;

        /**
         * Vector addition operator.
         *
         * @param other Vector to append.
         *
         * @return Resulting vector.
         */
        inline Vector operator+(const Vector& other) const;

        /**
         * Vector subtraction operator.
         *
         * @param other Vector to subtract.
         *
         * @return Resulting vector.
         */
        inline Vector operator-(const Vector& other) const;

        /**
         * Vector multiplication operator.
         *
         * @param other Vector to multiply.
         *
         * @return Resulting vector.
         */
        inline Vector operator*(const Vector& other) const;

        /**
         * Vector division operator.
         *
         * @param other Vector to divide by.
         *
         * @return Resulting vector.
         */
        inline Vector operator/(const Vector& other) const;

        /**
         * Scalar vector addition operator.
         *
         * @param other Scalar to add.
         *
         * @return Resulting vector.
         */
        inline Vector operator+(float other) const;

        /**
         * Scalar vector subtraction operator.
         *
         * @param other Scalar to subtract.
         *
         * @return Resulting vector.
         */
        inline Vector operator-(float other) const;

        /**
         * Scalar vector multiplication operator.
         *
         * @param other Scalar to multiply.
         *
         * @return Resulting vector.
         */
        inline Vector operator*(float other) const;

        /**
         * Scalar vector division operator.
         *
         * @param other Scalar to divide by.
         *
         * @return Resulting vector.
         */
        inline Vector operator/(float other) const;

    private:
        friend class Matrix;

#if defined(WIN32)
        /** Internal vector data. */
        DirectX::XMFLOAT3 vector;
#else
        /**
         * Constructor, initializes internal vector with GLM vector.
         *
         * @param vector GLM vector.
         */
        inline Vector(glm::vec3 vector) { this->vector = vector; }

        /** Internal vector data. */
        glm::vec3 vector;
#endif
    };

    Vector Vector::projectOnto(const Vector& other) const {
        const float otherLength = other.getLength();
        return other * (dotProduct(other) / (otherLength * otherLength));
    }

#if defined(WIN32)
    // --------------------------------------------------------------------------------------------
    // DirectXMath implementation.
    // --------------------------------------------------------------------------------------------

    Vector::Vector() { DirectX::XMStoreFloat3(&vector, DirectX::XMVectorZero()); } // NOLINT

    Vector::Vector(float x, float y, float z) { // NOLINT
        vector.x = x;
        vector.y = y;
        vector.z = z;
    }

    constexpr float Vector::getX() const { return vector.x; }

    constexpr float Vector::getY() const { return vector.y; }

    constexpr float Vector::getZ() const { return vector.z; }

    constexpr void Vector::setX(float x) { // NOLINT
        vector.x = x;
    }

    constexpr void Vector::setY(float y) { // NOLINT
        vector.y = y;
    }

    constexpr void Vector::setZ(float z) { // NOLINT
        vector.z = z;
    }

    void Vector::normalize() {
        const auto result = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&vector));
        DirectX::XMStoreFloat3(&vector, result);
    }

    float Vector::getLength() const {
        const auto result = DirectX::XMVector3Length(DirectX::XMLoadFloat3(&vector));
        DirectX::XMFLOAT3 output;
        DirectX::XMStoreFloat3(&output, result);

        return output.x;
    }

    float Vector::dotProduct(const Vector& other) const {
        const auto result =
            DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&vector), DirectX::XMLoadFloat3(&other.vector));

        DirectX::XMFLOAT3 output;
        DirectX::XMStoreFloat3(&output, result);

        return output.x;
    }

    Vector Vector::crossProduct(const Vector& other) const {
        const auto result =
            DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&vector), DirectX::XMLoadFloat3(&other.vector));

        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);

        return output;
    }

    float Vector::getAngleBetweenVectorsInRad(const Vector& other) const {
        const auto result = DirectX::XMVector3AngleBetweenVectors(
            DirectX::XMLoadFloat3(&vector), DirectX::XMLoadFloat3(&other.vector));
        DirectX::XMFLOAT3 output;
        DirectX::XMStoreFloat3(&output, result);

        return output.x;
    }

    float Vector::getAngleBetweenNormalizedVectorsInRad(const Vector& other) const {
        const auto result = DirectX::XMVector3AngleBetweenNormals(
            DirectX::XMLoadFloat3(&vector), DirectX::XMLoadFloat3(&other.vector));
        DirectX::XMFLOAT3 output;
        DirectX::XMStoreFloat3(&output, result);

        return output.x;
    }

    void Vector::rotateAroundAxis(const Vector& axis, float angleInRad) {
        const auto rotationMatrix =
            DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&axis.vector), angleInRad);
        const auto result = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&vector), rotationMatrix);

        DirectX::XMStoreFloat3(&vector, result);
    }

    Vector Vector::operator+(const Vector& other) const {
        DirectX::XMVECTOR result =
            DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&vector), DirectX::XMLoadFloat3(&other.vector));
        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);
        return output;
    }

    Vector Vector::operator-(const Vector& other) const {
        DirectX::XMVECTOR result =
            DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&vector), DirectX::XMLoadFloat3(&other.vector));
        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);
        return output;
    }

    Vector Vector::operator*(const Vector& other) const {
        DirectX::XMVECTOR result =
            DirectX::XMVectorMultiply(DirectX::XMLoadFloat3(&vector), DirectX::XMLoadFloat3(&other.vector));
        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);
        return output;
    }

    Vector Vector::operator/(const Vector& other) const {
        DirectX::XMVECTOR result =
            DirectX::XMVectorDivide(DirectX::XMLoadFloat3(&vector), DirectX::XMLoadFloat3(&other.vector));
        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);
        return output;
    }

    Vector Vector::operator+(float other) const {
        DirectX::XMVECTOR result = DirectX::XMVectorAdd(
            DirectX::XMLoadFloat3(&vector), DirectX::XMVectorSet(other, other, other, other));
        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);
        return output;
    }

    Vector Vector::operator-(float other) const {
        DirectX::XMVECTOR result = DirectX::XMVectorSubtract(
            DirectX::XMLoadFloat3(&vector), DirectX::XMVectorSet(other, other, other, other));
        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);
        return output;
    }

    Vector Vector::operator*(float other) const {
        DirectX::XMVECTOR result = DirectX::XMVectorMultiply(
            DirectX::XMLoadFloat3(&vector), DirectX::XMVectorSet(other, other, other, other));
        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);
        return output;
    }

    Vector Vector::operator/(float other) const {
        DirectX::XMVECTOR result = DirectX::XMVectorDivide(
            DirectX::XMLoadFloat3(&vector), DirectX::XMVectorSet(other, other, other, other));
        Vector output;
        DirectX::XMStoreFloat3(&output.vector, result);
        return output;
    }
#else
    // --------------------------------------------------------------------------------------------
    // GLM implementation.
    // --------------------------------------------------------------------------------------------

    Vector::Vector() { vector = glm::vec3(0.0f, 0.0f, 0.0f); }

    Vector::Vector(float x, float y, float z) { // NOLINT
        vector.x = x;
        vector.y = y;
        vector.z = z;
    }

    constexpr float Vector::getX() const { return vector.x; }

    constexpr float Vector::getY() const { return vector.y; }

    constexpr float Vector::getZ() const { return vector.z; }

    constexpr void Vector::setX(float x) { // NOLINT
        vector.x = x;
    }

    constexpr void Vector::setY(float y) { // NOLINT
        vector.y = y;
    }

    constexpr void Vector::setZ(float z) { // NOLINT
        vector.z = z;
    }

    void Vector::normalize() { vector = glm::normalize(vector); }

    float Vector::getLength() const { return glm::length(vector); }

    float Vector::dotProduct(const Vector& other) const { return glm::dot(vector, other.vector); }

    Vector Vector::crossProduct(const Vector& other) const {
        Vector output;
        output.vector = glm::cross(vector, other.vector);

        return output;
    }

    float Vector::getAngleBetweenVectorsInRad(const Vector& other) const {
        return glm::angle(glm::normalize(vector), glm::normalize(other.vector));
    }

    float Vector::getAngleBetweenNormalizedVectorsInRad(const Vector& other) const {
        return glm::angle(vector, other.vector);
    }

    void Vector::rotateAroundAxis(const Vector& axis, float angleInRad) {
        const glm::mat4 rotationMatrix = glm::rotate(angleInRad, axis.vector);
        vector = glm::vec4(vector.x, vector.y, vector.z, 0.0f) * rotationMatrix;
    }

    Vector Vector::operator+(const Vector& other) const { return Vector(vector + other.vector); }

    Vector Vector::operator-(const Vector& other) const { return Vector(vector - other.vector); }

    Vector Vector::operator*(const Vector& other) const { return Vector(vector * other.vector); }

    Vector Vector::operator/(const Vector& other) const { return Vector(vector / other.vector); }

    Vector Vector::operator+(float other) const { return Vector(vector + other); }

    Vector Vector::operator-(float other) const { return Vector(vector - other); }

    Vector Vector::operator*(float other) const { return Vector(vector * other); }

    Vector Vector::operator/(float other) const { return Vector(vector / other); }
#endif
} // namespace ne
