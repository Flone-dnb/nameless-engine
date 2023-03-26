#pragma once

// Standard.
#include <mutex>
#include <optional>

// Custom.
#include "math/GLMath.hpp"
#include "misc/Globals.h"

namespace ne {
    /** Defines how camera can move and rotate. */
    enum class CameraMode {
        FREE,   ///< Camera mode in which the camera can freely move and rotate without any restrictions.
        ORBITAL ///< Camera mode in which the camera is fixed and will always look at the specified point
                ///< (target point) in space. The camera can only move and rotate using spherical coordinate
                ///< system (i.e. rotates and moves around the target point).
    };

    /** Defines camera settings, base axis, location, modes, etc. */
    class CameraProperties {
        // Sets world adjustment matrix to include parent nodes.
        friend class CameraNode;

    public:
        CameraProperties();

        /** Stores internal data. */
        struct Data {
            /** Stores orbital mode specific data. */
            struct OrbitalModeData {
                /** Location of the point that the camera should look at. */
                glm::vec3 targetPointLocation = glm::vec3(0.0F, 0.0F, 0.0F);

                /** Radial distance or distance from camera to @ref targetPointLocation. */
                float distanceToTarget = 10.0F; // NOLINT: magic number

                /** Polar angle or camera's tilt relative target point. */
                float theta = 1.5f * glm::pi<float>(); // NOLINT: magic number

                /** Azimuthal angle or camera's rotation relative target point. */
                float phi = glm::pi<float>() / 4.0F; // NOLINT: magic number
            };

            /** Stores free mode specific data. */
            struct FreeModeData {
                /** Rotation in degrees where X is roll, Y is pitch and Z is yaw. */
                glm::vec3 rotation = glm::vec3(0.0F, 0.0F, 0.0F);
            };

            /** Stores data used for projection matrix. */
            struct ProjectionData {
                /**
                 * Transforms positions from view (camera) space to 2D projection window
                 * (homogeneous clip space).
                 */
                glm::mat4x4 projectionMatrix;

                /** Distance from camera (view) space origin to camera's near clip plane. */
                float nearClipPlaneDistance = 0.3F; // NOLINT: magic number

                /** Distance to camera's far clip plane. */
                float farClipPlaneDistance = 1000.0F; // NOLINT: magic number

                /** Vertical field of view. */
                unsigned int iVerticalFov = 90; // NOLINT: magic number

                /** Width of the buffer we are rendering the image to. */
                int iRenderTargetWidth = 800; // NOLINT: default value

                /** Height of the buffer we are rendering the image to. */
                int iRenderTargetHeight = 600; // NOLINT: default value

                /** Height of camera's near clip plane. */
                float nearClipPlaneHeight = 0.0F;

                /** Height of camera's far clip plane. */
                float farClipPlaneHeight = 0.0F;
            };

            /**
             * Contains a flag the indicates whether view matrix needs to be recalculated or not
             * and a matrix that transforms positions to view (camera) space.
             *
             * @remark The bool variable is used to minimize the amount of times we recalculate view matrix.
             */
            std::pair<bool, glm::mat4x4> viewMatrix = std::make_pair(true, glm::identity<glm::mat4x4>());

            /**
             * Contains a flag the indicates whether projection matrix needs to be recalculated or not
             * and a matrix that transforms positions from view (camera) space to 2D projection window
             * (homogeneous clip space).
             *
             * @remark The bool variable is used to minimize the amount of times we recalculate projection
             * matrix.
             */
            std::pair<bool, ProjectionData> projectionData = std::make_pair(true, ProjectionData());

            /** Location of the camera. */
            glm::vec3 location = glm::vec3(0.0F, 0.0F, 0.0F);

            /** Unit vector that points in camera's current up direction. */
            glm::vec3 upDirection = worldUpDirection;

            /** Unit vector that points in camera's current right direction. */
            glm::vec3 rightDirection = worldRightDirection;

            /** Unit vector that points in camera's current forward direction. */
            glm::vec3 forwardDirection = worldForwardDirection;

            /** Defines how camera can move and rotate. */
            CameraMode currentCameraMode = CameraMode::FREE;

            /** Parameters used by free camera mode. */
            FreeModeData freeModeData;

            /** Parameters used by orbital camera mode. */
            OrbitalModeData orbitalModeData;

            /** Matrix used to adjust location/axis when constructing a view matrix. */
            glm::mat4x4 worldAdjustmentMatrix = glm::identity<glm::mat4x4>();

            /** Sets whether we can flip the camera (make it upside down) during its rotation or not. */
            bool bDontFlipCamera = true;

            /** Minimum allowed value for near clip plane distance and far clip plane distance. */
            static inline const float minimumClipPlaneDistance = 0.00001F;
        };

        /**
         * Sets how the camera can move and rotate.
         *
         * @param cameraMode New mode.
         */
        void setCameraMode(CameraMode cameraMode);

        /**
         * Sets camera's location.
         *
         * @param location Camera's new location.
         */
        void setCameraLocation(const glm::vec3& location);

        /**
         * Moves free camera according to its forward direction (see @ref getForwardDirection).
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param distance Distance to move the camera. Specify a negative value to move backward.
         */
        void moveFreeCameraForward(float distance);

        /**
         * Moves free camera according to its right direction (see @ref getRightDirection).
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param distance Distance to move the camera. Specify a negative value to move left.
         */
        void moveFreeCameraRight(float distance);

        /**
         * Moves free camera according to its up direction (see @ref getUpDirection).
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param distance Distance to move the camera. Specify a negative value to move down.
         */
        void moveFreeCameraUp(float distance);

        /**
         * Moves free camera according to world's up direction.
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param distance Distance to move the camera.
         */
        void moveFreeCameraWorldUp(float distance);

        /**
         * Sets free camera's pitch (angle in degrees) by rotating the camera around its
         * right direction.
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @remark The specified pitch may be clamped if "don't flip the camera" is enabled
         * (see @ref setDontFlipCamera).
         *
         * @param pitch Angle (in degrees).
         */
        void setFreeCameraPitch(float pitch);

        /**
         * Sets free camera's yaw (angle in degrees) by rotating the camera around its
         * up direction.
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param yaw Angle (in degrees).
         */
        void setFreeCameraYaw(float yaw);

        /**
         * Sets free camera's roll (angle in degrees) by rotating the camera around its
         * forward direction.
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @remark Only works if "don't flip the camera" is disabled (see @ref setDontFlipCamera),
         * otherwise logs a warning.
         *
         * @param roll Angle (in degrees).
         */
        void setFreeCameraRoll(float roll);

        /**
         * Makes free camera to look at the specified location.
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param targetLocation Location to look at.
         */
        void makeFreeCameraLookAt(const glm::vec3& targetLocation);

        /**
         * Sets a point location that orbital camera should look at.
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param targetPointLocation Location to look at.
         */
        void setOrbitalCameraTargetPoint(const glm::vec3& targetPointLocation);

        /**
         * Sets camera's radial distance or distance from camera to camera's target point
         * (see @ref setOrbitalCameraTargetPoint).
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param distanceToTarget Radial distance or distance from camera to camera's target point.
         */
        void setOrbitalCameraDistanceToTarget(float distanceToTarget);

        /**
         * Sets camera's rotation by specifying tilt and rotation around camera's target point
         * (see @ref setOrbitalCameraTargetPoint).
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param phi   Azimuthal angle or camera's rotation relative target point.
         * @param theta Polar angle or camera's tilt relative target point.
         */
        void setOrbitalCameraRotation(float phi, float theta);

        /**
         * Sets whether we can flip the camera (make it upside down) during its rotation or not.
         *
         * @param bDontFlipCamera `true` to don't flip the camera, `false` to allow flipping the camera.
         */
        void setDontFlipCamera(bool bDontFlipCamera);

        /**
         * Sets camera's vertical field of view.
         *
         * @param iVerticalFov Vertical field of view.
         */
        void setFov(unsigned int iVerticalFov);

        /**
         * Sets camera's aspect ratio.
         *
         * @param iRenderTargetWidth  Width of the buffer we are rendering the image to.
         * @param iRenderTargetHeight Height of the buffer we are rendering the image to.
         */
        void setAspectRatio(int iRenderTargetWidth, int iRenderTargetHeight);

        /**
         * Sets distance from camera (view) space origin to camera's near clip plane.
         *
         * @param nearClipPlaneDistance Near Z distance. Should be a positive value.
         */
        void setNearClipPlaneDistance(float nearClipPlaneDistance);

        /**
         * Sets distance from camera (view) space origin to camera's far clip plane.
         *
         * @param farClipPlaneDistance Far Z distance. Should be a positive value.
         */
        void setFarClipPlaneDistance(float farClipPlaneDistance);

        /**
         * Returns location of the camera.
         *
         * @param bInWorldSpace Specify `true` to get camera's location in world space (includes
         * world adjustment matrix), `false` to get camera's location in its "local" space
         * (which could be the same as in world space or not).
         *
         * @return Camera's location.
         */
        glm::vec3 getLocation(bool bInWorldSpace);

        /**
         * Returns unit vector that points in camera's current forward direction.
         *
         * @param bInWorldSpace Specify `true` to get camera's forward direction in world space (includes
         * world adjustment matrix), `false` to get camera's forward direction in its "local" space
         * (which could be the same as in world space or not).
         *
         * @return Camera's forward direction.
         */
        glm::vec3 getForwardDirection(bool bInWorldSpace);

        /**
         * Returns unit vector that points in camera's current right direction.
         *
         * @param bInWorldSpace Specify `true` to get camera's right direction in world space (includes
         * world adjustment matrix), `false` to get camera's right direction in its "local" space
         * (which could be the same as in world space or not).
         *
         * @return Camera's right direction.
         */
        glm::vec3 getRightDirection(bool bInWorldSpace);

        /**
         * Returns unit vector that points in camera's current up direction.
         *
         * @param bInWorldSpace Specify `true` to get camera's up direction in world space (includes
         * world adjustment matrix), `false` to get camera's up direction in its "local" space
         * (which could be the same as in world space or not).
         *
         * @return Camera's up direction.
         */
        glm::vec3 getUpDirection(bool bInWorldSpace);

        /**
         * Returns a location that orbital camera looks at.
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param bInWorldSpace Specify `true` to get camera's up direction in world space (includes
         * world adjustment matrix), `false` to get camera's up direction in its "local" space
         * (which could be the same as in world space or not).
         *
         * @return Location the camera looks at.
         */
        glm::vec3 getOrbitalCameraTargetLocation(bool bInWorldSpace);

        /**
         * Returns rotation (in degrees) around camera's right direction.
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @return Camera's pitch.
         */
        float getFreeCameraPitch();

        /**
         * Returns rotation (in degrees) around camera's up direction.
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @return Camera's yaw.
         */
        float getFreeCameraYaw();

        /**
         * Returns rotation (in degrees) around camera's forward direction.
         *
         * @remark Only works if the current camera mode is free (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @return Camera's roll.
         */
        float getFreeCameraRoll();

        /**
         * Returns vertical field of view of the camera.
         *
         * @return Vertical field of view.
         */
        unsigned int getVerticalFov();

        /**
         * Returns radial distance or distance from camera to camera's target point location.
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @return Distance to view target.
         */
        float getOrbitalCameraDistanceToTarget();

        /**
         * Returns polar angle or camera's tilt relative target point.
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @return Camera's tilt.
         */
        float getOrbitalCameraTheta();

        /**
         * Returns azimuthal angle or camera's rotation relative target point.
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @return Camera's rotation.
         */
        float getOrbitalCameraPhi();

        /**
         * Returns distance from camera (view) space origin to camera's near clip plane.
         *
         * @return Distance to near clip plane.
         */
        float getNearClipPlaneDistance();

        /**
         * Returns distance from camera (view) space origin to camera's far clip plane.
         *
         * @return Distance to far clip plane.
         */
        float getFarClipPlaneDistance();

        /**
         * Returns camera's aspect ratio.
         *
         * @return Camera's aspect ratio.
         */
        float getAspectRatio();

        /**
         * Returns a matrix that transforms positions to view (camera) space.
         *
         * @return View matrix.
         */
        glm::mat4x4 getViewMatrix();

        /**
         * Returns a matrix that transforms positions from view (camera) space to 2D projection window
         * (homogeneous clip space).
         *
         * @return Projection matrix.
         */
        glm::mat4x4 getProjectionMatrix();

    private:
        /**
         * Recalculates camera's view matrix if it needs to be updated.
         *
         * @remark This function can ignore the call if there's no need to recalculate view matrix.
         */
        void makeSureViewMatrixIsUpToDate();

        /**
         * Recalculates camera's projection matrix and clip plane heights if it needs to be updated.
         *
         * @remark This function can ignore the call if there's no need to recalculate projection matrix.
         */
        void makeSureProjectionMatrixAndClipPlanesAreUpToDate();

        /**
         * Recalculates camera's forward/right/up directions based on orbital camera's location and
         * camera's target point location.
         */
        inline void recalculateBaseVectorsForOrbitalCamera() {
            mtxData.second.forwardDirection =
                glm::normalize(mtxData.second.orbitalModeData.targetPointLocation - mtxData.second.location);
            mtxData.second.rightDirection =
                glm::normalize(glm::cross(mtxData.second.forwardDirection, worldUpDirection));
            mtxData.second.upDirection = glm::cross(mtxData.second.rightDirection, worldUpDirection);
        }

        /**
         * Sets world adjustment matrix that is used to adjust location/axis when constructing a view matrix.
         *
         * @param adjustmentMatrix Matrix to use.
         */
        void setWorldAdjustmentMatrix(const glm::mat4x4& adjustmentMatrix);

        /** Internal properties. */
        std::pair<std::recursive_mutex, Data> mtxData;

        /** Delta to compare input to zero. */
        static inline constexpr float floatDelta = 0.00001F;

        /** Name of the category used for logging. */
        static inline const auto sCameraPropertiesLogCategory = "Camera Properties";
    };
} // namespace ne
