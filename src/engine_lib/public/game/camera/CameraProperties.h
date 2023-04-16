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
        // Only camera node or transient camera can control internal data.
        friend class CameraNode;
        friend class TransientCamera;

    public:
        CameraProperties() = default;

        /** Stores internal data. */
        struct Data {
            Data() = default;

            /** Stores orbital mode specific data. */
            struct OrbitalModeData {
                OrbitalModeData() = default;

                /** Radial distance or distance from camera to target point (look target). */
                float distanceToTarget = 10.0F; // NOLINT: magic number

                /** Polar angle (in degrees). */
                float theta = 0.0F; // NOLINT: magic number

                /** Azimuthal angle (in degrees). */
                float phi = 0.0F; // NOLINT: magic number
            };

            /** Stores data used for view matrix. */
            struct ViewData {
                ViewData() = default;

                /** Matrix that transforms positions to view (camera) space. */
                glm::mat4x4 viewMatrix = glm::identity<glm::mat4x4>();

                /** Whether @ref viewMatrix needs to be recalculated or not. */
                bool bViewMatrixNeedsUpdate = true;

                /** Location of the camera in world space. */
                glm::vec3 worldLocation = glm::vec3(0.0F, 0.0F, 0.0F);

                /** Unit vector that points in camera's current up direction in world space. */
                glm::vec3 worldUpDirection = Globals::WorldDirection::up;

                /** Location of the point in world space that the camera should look at. */
                glm::vec3 targetPointWorldLocation = glm::vec3(1.0F, 0.0F, 0.0F);
            };

            /** Stores data used for projection matrix. */
            struct ProjectionData {
                ProjectionData() = default;

                /**
                 * Transforms positions from view (camera) space to 2D projection window
                 * (homogeneous clip space).
                 */
                glm::mat4x4 projectionMatrix = glm::identity<glm::mat4x4>();

                /** Whether @ref projectionMatrix needs to be recalculated or not. */
                bool bProjectionMatrixNeedsUpdate = true;

                /** Distance from camera (view) space origin to camera's near clip plane. */
                float nearClipPlaneDistance = 0.3F; // NOLINT: magic number

                /** Distance to camera's far clip plane. */
                float farClipPlaneDistance = 1000.0F; // NOLINT: magic number

                /** Vertical field of view. */
                unsigned int iVerticalFov = 90; // NOLINT: magic number

                /** Width of the buffer we are rendering the image to. */
                unsigned int iRenderTargetWidth = 800; // NOLINT: default value

                /** Height of the buffer we are rendering the image to. */
                unsigned int iRenderTargetHeight = 600; // NOLINT: default value

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
            ViewData viewData;

            /**
             * Contains a flag the indicates whether projection matrix needs to be recalculated or not
             * and a matrix that transforms positions from view (camera) space to 2D projection window
             * (homogeneous clip space).
             *
             * @remark The bool variable is used to minimize the amount of times we recalculate projection
             * matrix.
             */
            ProjectionData projectionData;

            /** Defines how camera can move and rotate. */
            CameraMode currentCameraMode = CameraMode::FREE;

            /** Parameters used by orbital camera mode. */
            OrbitalModeData orbitalModeData;

            /** Minimum allowed value for near clip plane distance and far clip plane distance. */
            static inline const float minimumClipPlaneDistance = 0.00001F;
        };

        /**
         * Sets camera's vertical field of view.
         *
         * @param iVerticalFov Vertical field of view.
         */
        void setFov(unsigned int iVerticalFov);

        /**
         * Sets camera's aspect ratio.
         *
         * @remark Does nothing if the specified aspect ratio is equal to the current aspect ratio.
         *
         * @param iRenderTargetWidth  Width of the buffer we are rendering the image to.
         * @param iRenderTargetHeight Height of the buffer we are rendering the image to.
         */
        void setAspectRatio(unsigned int iRenderTargetWidth, unsigned int iRenderTargetHeight);

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
         * Returns vertical field of view of the camera.
         *
         * @return Vertical field of view.
         */
        unsigned int getVerticalFov();

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
         * Returns width (component of aspect ratio) of the target we are presenting the camera.
         *
         * @return Camera's aspect ratio's width.
         */
        unsigned int getRenderTargetWidth();

        /**
         * Returns height (component of aspect ratio) of the target we are presenting the camera.
         *
         * @return Camera's aspect ratio's height.
         */
        unsigned int getRenderTargetHeight();

        /**
         * Returns orbital camera properties.
         *
         * @return Orbital camera properties.
         */
        Data::OrbitalModeData getOrbitalModeProperties();

        /**
         * Returns camera's world location.
         *
         * @return Location in world space.
         */
        glm::vec3 getWorldLocation();

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

        /** Internal properties. */
        std::pair<std::recursive_mutex, Data> mtxData{};

        /** Delta to compare input to zero. */
        static inline constexpr float floatDelta = 0.00001F;

        /** Name of the category used for logging. */
        static inline const auto sCameraPropertiesLogCategory = "Camera Properties";
    };
} // namespace ne
