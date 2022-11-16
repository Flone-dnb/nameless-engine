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
