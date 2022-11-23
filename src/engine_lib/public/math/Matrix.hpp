#pragma once

// Custom.
#include "math/Vector.hpp"

// External.
#if defined(WIN32)
#include "DirectXMath.h"
#else
#include "math/GLMath.hpp"
#endif

namespace ne {
    /** Represents a 4x4 matrix. */
    class Matrix {
    public:
        /** Initializes the matrix as identity matrix. */
        inline Matrix();

        /**
         * Copy constructor.
         */
        inline Matrix(const Matrix&) = default;

        /**
         * Copy assignment operator.
         *
         * @return Matrix.
         */
        inline Matrix& operator=(const Matrix&) = default;

        /**
         * Creates a translation matrix from the specified offsets.
         *
         * @param xOffset Translation along the x-axis.
         * @param yOffset Translation along the y-axis.
         * @param zOffset Translation along the z-axis.
         *
         * @return Created translation matrix.
         */
        static inline Matrix createTranslationMatrix(float xOffset, float yOffset, float zOffset);

        /**
         * Creates a rotation matrix that rotates around an arbitrary axis.
         *
         * @param axis        Vector describing the axis of rotation.
         * @param angleInRad  Angle of rotation in radians.
         *
         * @return Created rotation matrix.
         */
        static inline Matrix createRotationMatrixAroundCustomAxis(const Vector& axis, float angleInRad);

        /**
         * Creates a rotation matrix that rotates around an arbitrary normal vector.
         *
         * @remark Assumes the rotation axis is normalized.
         *
         * @param normalizedAxis Normal vector describing the axis of rotation.
         * @param angleInRad     Angle of rotation in radians.
         *
         * @return Created rotation matrix.
         */
        static inline Matrix
        createRotationMatrixAroundCustomNormalizedAxis(const Vector& normalizedAxis, float angleInRad);

        /**
         * Creates a rotation matrix that rotates around the x-axis.
         *
         * @param angleInRad Angle of rotation in radians.
         *
         * @return Created rotation matrix.
         */
        static inline Matrix createRotationMatrixAroundXAxis(float angleInRad);

        /**
         * Creates a rotation matrix that rotates around the y-axis.
         *
         * @param angleInRad Angle of rotation in radians.
         *
         * @return Created rotation matrix.
         */
        static inline Matrix createRotationMatrixAroundYAxis(float angleInRad);

        /**
         * Creates a rotation matrix that rotates around the z-axis.
         *
         * @param angleInRad Angle of rotation in radians.
         *
         * @return Created rotation matrix.
         */
        static inline Matrix createRotationMatrixAroundZAxis(float angleInRad);

        /**
         * Creates a scaling matrix that scales along the x-axis, y-axis, and z-axis.
         *
         * @param xScale Scaling factor along the x-axis.
         * @param yScale Scaling factor along the y-axis.
         * @param zScale Scaling factor along the z-axis.
         *
         * @return Created scaling matrix.
         */
        static inline Matrix createScalingMatrix(float xScale, float yScale, float zScale);

        /**
         * Creates a "look at" view matrix for a left-handed coordinate system
         * using a point in space to look at.
         *
         * @param cameraLocation      Camera location in the world.
         * @param focusPointLocation  Location of the point in space to look at.
         * @param upDirection         Up direction of the camera.
         *
         * @return View matrix.
         */
        static inline Matrix createLookAtViewMatrix(
            const Vector& cameraLocation, const Vector& focusPointLocation, const Vector& upDirection);

        /**
         * Creates a "look to" view matrix for a left-handed coordinate system
         * using a direction to look to.
         *
         * @param cameraLocation   Camera location in the world.
         * @param lookToDirection  Direction of the camera.
         * @param upDirection      Up direction of the camera.
         *
         * @return View matrix.
         */
        static inline Matrix createLookToViewMatrix(
            const Vector& cameraLocation, const Vector& lookToDirection, const Vector& upDirection);

        /**
         * Creates an orthogonal projection matrix for a left-handed coordinate system.
         *
         * @param viewWidth   Width of the frustum at the near clipping plane.
         * @param viewHeight  Height of the frustum at the near clipping plane.
         * @param nearZ       Distance to the near clipping plane.
         * @param farZ        Distance to the far clipping plane.
         *
         * @return Projection matrix.
         */
        static inline Matrix
        createOrthographicProjectionMatrix(float viewWidth, float viewHeight, float nearZ, float farZ);

        /**
         * Creates a left-handed perspective projection matrix based on a field of view.
         *
         * @remark For typical usage, NearZ is less than FarZ.
         * However, if you flip these values so FarZ is less than NearZ, the result is
         * an inverted z buffer (also known as a "reverse z buffer") which can provide
         * increased floating-point precision.
         *
         * @param fovY         Top-down field-of-view angle in radians.
         * @param aspectRatio  Aspect ratio of view-space X:Y.
         * @param nearZ        Distance to the near clipping plane.
         * @param farZ         Distance to the far clipping plane.
         *
         * @return
         */
        static inline Matrix
        createPerspectiveProjectionMatrix(float fovY, float aspectRatio, float nearZ, float farZ);

        /**
         * Computes the transpose of a matrix.
         *
         * @param matrix Matrix to transpose.
         *
         * @return Transposed matrix.
         */
        static inline Matrix transpose(const Matrix& matrix);

        /**
         * Sets a value into a specific matrix cell.
         *
         * @param iRow    Cell's row.
         * @param iColumn Cell's column.
         * @param value   Value to set.
         */
        inline void setValue(size_t iRow, size_t iColumn, float value);

        /**
         * Returns a value from a specific matrix cell.
         *
         * @param iRow    Cell's row.
         * @param iColumn Cell's column.
         *
         * @return Cell's value.
         */
        inline float getValue(size_t iRow, size_t iColumn) const;

        /**
         * Returns matrix determinant.
         *
         * @return Matrix determinant.
         */
        inline float getDeterminant() const;

        /**
         * Matrix multiplication operator.
         *
         * @param other Matrix to multiply.
         *
         * @return Resulting matrix.
         */
        inline Matrix operator*(const Matrix& other) const;

    private:
#if defined(WIN32)
        /**
         * Constructor, initializes internal matrix.
         *
         * @param matrix Matrix to set.
         */
        Matrix(const DirectX::XMFLOAT4X4& matrix) { this->matrix = matrix; }

        /** Internal matrix data. */
        DirectX::XMFLOAT4X4 matrix;
#else
        /** Internal matrix data. */
        glm::mat4x4 matrix;
#endif
    };

#if defined(WIN32)
    // --------------------------------------------------------------------------------------------
    // DirectXMath implementation.
    // --------------------------------------------------------------------------------------------

    Matrix::Matrix() { DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity()); }

    Matrix Matrix::createTranslationMatrix(float xOffset, float yOffset, float zOffset) {
        const auto result = DirectX::XMMatrixTranslation(xOffset, yOffset, zOffset);
        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix, result);

        return Matrix(matrix);
    }

    Matrix Matrix::createRotationMatrixAroundCustomAxis(const Vector& axis, float angleInRad) {
        const auto result = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&axis.vector), angleInRad);
        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix, result);

        return Matrix(matrix);
    }

    Matrix
    Matrix::createRotationMatrixAroundCustomNormalizedAxis(const Vector& normalizedAxis, float angleInRad) {
        const auto result =
            DirectX::XMMatrixRotationNormal(DirectX::XMLoadFloat3(&normalizedAxis.vector), angleInRad);
        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix, result);

        return Matrix(matrix);
    }

    Matrix Matrix::createRotationMatrixAroundXAxis(float angleInRad) {
        const auto result = DirectX::XMMatrixRotationX(angleInRad);
        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix, result);

        return Matrix(matrix);
    }

    Matrix Matrix::createRotationMatrixAroundYAxis(float angleInRad) {
        const auto result = DirectX::XMMatrixRotationY(angleInRad);
        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix, result);

        return Matrix(matrix);
    }

    Matrix Matrix::createRotationMatrixAroundZAxis(float angleInRad) {
        const auto result = DirectX::XMMatrixRotationZ(angleInRad);
        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix, result);

        return Matrix(matrix);
    }

    Matrix Matrix::createScalingMatrix(float xScale, float yScale, float zScale) {
        const auto result = DirectX::XMMatrixScaling(xScale, yScale, zScale);
        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix, result);

        return Matrix(matrix);
    }

    void Matrix::setValue(size_t iRow, size_t iColumn, float value) { matrix.m[iRow][iColumn] = value; }

    float Matrix::getValue(size_t iRow, size_t iColumn) const { return matrix.m[iRow][iColumn]; }

    Matrix Matrix::operator*(const Matrix& other) const {
        const auto result = DirectX::XMMatrixMultiply(
            DirectX::XMLoadFloat4x4(&matrix), DirectX::XMLoadFloat4x4(&other.matrix));
        DirectX::XMFLOAT4X4 output;
        DirectX::XMStoreFloat4x4(&output, result);
        return Matrix(output);
    }

    float Matrix::getDeterminant() const {
        const auto result = DirectX::XMMatrixDeterminant(DirectX::XMLoadFloat4x4(&matrix));
        DirectX::XMFLOAT3 output;
        DirectX::XMStoreFloat3(&output, result);
        return output.x;
    }

    Matrix Matrix::transpose(const Matrix& matrix) {
        const auto result = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&matrix.matrix));
        DirectX::XMFLOAT4X4 output;
        DirectX::XMStoreFloat4x4(&output, result);
        return Matrix(output);
    }

    Matrix Matrix::createLookAtViewMatrix(
        const Vector& cameraLocation, const Vector& focusPointLocation, const Vector& upDirection) {
        const auto result = DirectX::XMMatrixLookAtLH(
            DirectX::XMLoadFloat3(&cameraLocation.vector),
            DirectX::XMLoadFloat3(&focusPointLocation.vector),
            DirectX::XMLoadFloat3(&upDirection.vector));
        DirectX::XMFLOAT4X4 output;
        DirectX::XMStoreFloat4x4(&output, result);
        return Matrix(output);
    }

    Matrix Matrix::createLookToViewMatrix(
        const Vector& cameraLocation, const Vector& lookToDirection, const Vector& upDirection) {
        const auto result = DirectX::XMMatrixLookToLH(
            DirectX::XMLoadFloat3(&cameraLocation.vector),
            DirectX::XMLoadFloat3(&lookToDirection.vector),
            DirectX::XMLoadFloat3(&upDirection.vector));
        DirectX::XMFLOAT4X4 output;
        DirectX::XMStoreFloat4x4(&output, result);
        return Matrix(output);
    }

    Matrix
    Matrix::createOrthographicProjectionMatrix(float viewWidth, float viewHeight, float nearZ, float farZ) {
        const auto result = DirectX::XMMatrixOrthographicLH(viewWidth, viewHeight, nearZ, farZ);
        DirectX::XMFLOAT4X4 output;
        DirectX::XMStoreFloat4x4(&output, result);
        return Matrix(output);
    }

    Matrix Matrix::createPerspectiveProjectionMatrix(float fovY, float aspectRatio, float nearZ, float farZ) {
        const auto result = DirectX::XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearZ, farZ);
        DirectX::XMFLOAT4X4 output;
        DirectX::XMStoreFloat4x4(&output, result);
        return Matrix(output);
    }

#else
    // --------------------------------------------------------------------------------------------
    // GLM implementation.
    // --------------------------------------------------------------------------------------------

#endif
} // namespace ne
