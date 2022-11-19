#pragma once

// External.
#if defined(WIN32)
#include "DirectXMath.h"
#endif

namespace ne {
    /** Represents a 3D vector. */
    class Vector {
    public:
        /** Initializes the vector with zeros. */
        inline Vector();

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
        inline void setX(float x); // NOLINT

        /**
         * Sets the Y component of the vector.
         *
         * @param y Y component of the vector.
         */
        inline void setY(float y); // NOLINT

        /**
         * Sets the Z component of the vector.
         *
         * @param z Z component of the vector.
         */
        inline void setZ(float z); // NOLINT

        /**
         * Normalizes the vector.
         */
        inline void normalize();

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
         * Returns the X component of the vector.
         *
         * @return X component of the vector.
         */
        inline float getX() const;

        /**
         * Returns the Y component of the vector.
         *
         * @return Y component of the vector.
         */
        inline float getY() const;

        /**
         * Returns the Z component of the vector.
         *
         * @return Z component of the vector.
         */
        inline float getZ() const;

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
#if defined(WIN32)
        /** Internal vector data. */
        DirectX::XMFLOAT3 vector;
#endif

        /** Float comparison delta/tolerance. */
        static inline constexpr float floatEpsilon = 0.00001F;
    };

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

    float Vector::getX() const { return vector.x; }

    float Vector::getY() const { return vector.y; }

    float Vector::getZ() const { return vector.z; }

    void Vector::setX(float x) { // NOLINT
        vector.x = x;
    }

    void Vector::setY(float y) { // NOLINT
        vector.y = y;
    }

    void Vector::setZ(float z) { // NOLINT
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

    Vector Vector::projectOnto(const Vector& other) const {
        const float otherLength = other.getLength();
        return other * (dotProduct(other) / (otherLength * otherLength));
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
#endif
} // namespace ne
