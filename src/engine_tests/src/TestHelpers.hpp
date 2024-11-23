#pragma once

// Custom.
#include "game/nodes/CameraNode.h"
#include "game/camera/CameraManager.h"
#include "io/TextureImporter.h"

// External.
#include "GcPtr.h"

namespace ne {
    /** Provides static helper functions for tests. */
    class TestHelpers {
    public:
        TestHelpers() = delete;

        /**
         * Creates a new camera node, configures it to don't affect the game world and makes it active.
         *
         * @param pWorldRootNode
         */
        static inline sgc::GcPtr<CameraNode>
        createAndSpawnActiveCamera(const sgc::GcPtr<Node>& pWorldRootNode, CameraManager* pCameraManager) {
            // Create camera.
            auto pCamera = sgc::makeGc<CameraNode>();

            // Configure.
            pCamera->setSerialize(false);

            // Spawn.
            pWorldRootNode->addChildNode(pCamera);

            // Make active.
            pCameraManager->setActiveCamera(pCamera);

            return pCamera;
        }

        /** If not an error returns paths relative to the `res` directory to imported diffuse textures. */
        static inline std::variant<Error, std::array<std::string, 2>> prepareDiffuseTextures() {
            using namespace ne;

            // Prepare some paths.
            static constexpr auto pImportedTexture1DirectoryName = "imported1";
            static constexpr auto pImportedTexture2DirectoryName = "imported2";

            const auto pathToImportexTexture1Dir =
                ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                pImportedTexture1DirectoryName;
            const auto pathToImportexTexture2Dir =
                ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                pImportedTexture2DirectoryName;

            // Delete previously imported texture (if exists).
            if (std::filesystem::exists(pathToImportexTexture1Dir)) {
                std::filesystem::remove_all(pathToImportexTexture1Dir);
            }
            if (std::filesystem::exists(pathToImportexTexture2Dir)) {
                std::filesystem::remove_all(pathToImportexTexture2Dir);
            }

            // Import sample texture.
            auto optionalError = TextureImporter::importTexture(
                ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "texture.png",
                TextureImportFormat::RGB,
                "test/temp",
                pImportedTexture1DirectoryName);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            optionalError = TextureImporter::importTexture(
                ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "texture.png",
                TextureImportFormat::RGB,
                "test/temp",
                pImportedTexture2DirectoryName);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            return std::array{
                std::string("test/temp/") + pImportedTexture1DirectoryName,
                std::string("test/temp/") + pImportedTexture2DirectoryName};
        }
    };
}
