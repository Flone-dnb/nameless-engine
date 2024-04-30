#include "material/TextureManager.h"

// Standard.
#include <format>
#include <cstring>

// Custom.
#include "misc/ProjectPaths.h"
#include "io/Logger.h"
#include "render/general/resources/GpuResourceManager.h"
#include "render/vulkan/VulkanRenderer.h"
#include "io/TextureImporter.h"

namespace ne {

    TextureManager::TextureManager(GpuResourceManager* pResourceManager)
        : pResourceManager(pResourceManager) {
        // Initialize texture format extension.
        determineTextureFormatExtension();
    }

    TextureManager::~TextureManager() {
        std::scoped_lock guard(mtxTextureResources.first);

        // Make sure no resource is loaded.
        if (!mtxTextureResources.second.empty()) [[unlikely]] {
            // Prepare a description of all not released resources.
            std::string sLoadedTextures;
            for (const auto& [sPath, resourceInfo] : mtxTextureResources.second) {
                sLoadedTextures += std::format(
                    "- \"{}\", alive handles that reference this path: {}\n",
                    sPath,
                    resourceInfo.iActiveTextureHandleCount);
            }

            // Show error.
            Error error(std::format(
                "texture manager is being destroyed but there are still {} texture(s) "
                "loaded in the memory:\n"
                "{}",
                mtxTextureResources.second.size(),
                sLoadedTextures));
            error.showError();
            return; // don't throw in destructor, just quit
        }
    }

    size_t TextureManager::getTextureInMemoryCount() {
        std::scoped_lock guard(mtxTextureResources.first);

        return mtxTextureResources.second.size();
    }

    std::variant<std::unique_ptr<TextureHandle>, Error>
    TextureManager::getTexture(const std::string& sPathToResourceRelativeRes) {
        std::scoped_lock guard(mtxTextureResources.first);

        // See if a texture by this path is already loaded.
        const auto it = mtxTextureResources.second.find(sPathToResourceRelativeRes);
        if (it == mtxTextureResources.second.end()) {
            // Load texture and create a new handle.
            auto result = loadTextureAndCreateNewTextureHandle(sPathToResourceRelativeRes);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Return created handle.
            return std::get<std::unique_ptr<TextureHandle>>(std::move(result));
        }

        // Just create a new handle.
        return createNewTextureHandle(sPathToResourceRelativeRes);
    }

    void TextureManager::releaseTextureResourceIfNotUsed(const std::string& sPathToResourceRelativeRes) {
        std::scoped_lock guard(mtxTextureResources.first);

        // Make sure a resource by this path is actually loaded.
        const auto it = mtxTextureResources.second.find(sPathToResourceRelativeRes);
        if (it == mtxTextureResources.second.end()) [[unlikely]] {
            // This should not happen, something is wrong.
            Logger::get().error(std::format(
                "a texture handle just notified the texture manager about "
                "no longer referencing a texture resource at \"{}\" "
                "but the manager does not store resources from this path",
                sPathToResourceRelativeRes));
            return;
        }

        // Self check: make sure the handle counter is not zero.
        if (it->second.iActiveTextureHandleCount == 0) [[unlikely]] {
            Logger::get().error(std::format(
                "a texture handle just notified the texture manager "
                "about no longer referencing a texture resource at \"{}\", "
                "the manager has such a resource entry but the current "
                "handle counter is zero",
                sPathToResourceRelativeRes));
            return;
        }

        // Decrement the handle counter.
        it->second.iActiveTextureHandleCount -= 1;

        // See if no handle is referencing this resource now.
        if (it->second.iActiveTextureHandleCount == 0) {
            // Release this resource from the memory.
            mtxTextureResources.second.erase(it);

            // Log event.
            Logger::get().info(std::format(
                "released texture resource for path \"{}\" from the memory because it's no longer used, "
                "textures in memory now: {}",
                sPathToResourceRelativeRes,
                getTextureInMemoryCount()));
        }
    }

    void TextureManager::determineTextureFormatExtension() {
        if (dynamic_cast<VulkanRenderer*>(pResourceManager->getRenderer()) != nullptr) {
            sTextureFormatExtension = ".ktx";
        } else {
            sTextureFormatExtension = ".dds";
        }
    }

    std::unique_ptr<TextureHandle>
    TextureManager::createNewTextureHandle(const std::string& sPathToResourceRelativeRes) {
        std::scoped_lock guard(mtxTextureResources.first);

        // Find texture.
        const auto it = mtxTextureResources.second.find(sPathToResourceRelativeRes);
        if (it == mtxTextureResources.second.end()) [[unlikely]] {
            // This should not happen.
            Error error(std::format(
                "requested to create texture handle to not loaded path \"{}\" "
                "(this is a bug, report to developers)",
                sPathToResourceRelativeRes));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Increment texture handle count.
        it->second.iActiveTextureHandleCount += 1;

        // Self check: make sure handle counter will not hit type limit.
        if (it->second.iActiveTextureHandleCount == ULLONG_MAX) [[unlikely]] {
            Logger::get().warn(std::format(
                "texture handle counter for resource \"{}\" just hit type limit "
                "with value: {}, new texture handle for this resource will make the counter invalid",
                sPathToResourceRelativeRes,
                it->second.iActiveTextureHandleCount));
        }

        return std::unique_ptr<TextureHandle>(
            new TextureHandle(this, sPathToResourceRelativeRes, it->second.pTexture.get()));
    }

    std::variant<std::unique_ptr<TextureHandle>, Error>
    TextureManager::loadTextureAndCreateNewTextureHandle(const std::string& sPathToResourceRelativeRes) {
        std::scoped_lock guard(mtxTextureResources.first);

        // Construct path to texture.
        auto pathToResource =
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / sPathToResourceRelativeRes;

        // Make sure it's a directory.
        if (!std::filesystem::is_directory(pathToResource)) [[unlikely]] {
            return Error(
                std::format("expected the path \"{}\" to point to a directory", pathToResource.string()));
        }

        // Construct a path to the file by appending a format.
        pathToResource /= TextureImporter::getImportedFileName() + sTextureFormatExtension;

        // Load texture.
        auto result = pResourceManager->loadTextureFromDisk(
            std::format("texture \"{}\"", sPathToResourceRelativeRes), pathToResource);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        auto pTexture = std::get<std::unique_ptr<GpuResource>>(std::move(result));

        // Add new resource to be considered.
        TextureResource resourceInfo;
        resourceInfo.iActiveTextureHandleCount =
            0; // leave as 0 because `createNewTextureHandle` will increment it
        resourceInfo.pTexture = std::move(pTexture);
        mtxTextureResources.second[sPathToResourceRelativeRes] = std::move(resourceInfo);

        // Log event.
        Logger::get().info(std::format(
            "texture \"{}\" was loaded from disk into memory, textures in memory now: {}",
            sPathToResourceRelativeRes,
            getTextureInMemoryCount()));

        return createNewTextureHandle(sPathToResourceRelativeRes);
    }

    TextureHandle::TextureHandle(
        TextureManager* pTextureManager,
        const std::string& sPathToResourceRelativeRes,
        GpuResource* pTexture) {
        this->pTextureManager = pTextureManager;
        this->sPathToResourceRelativeRes = sPathToResourceRelativeRes;
        this->pTexture = pTexture;
    }

    GpuResource* TextureHandle::getResource() { return pTexture; }

    std::string TextureHandle::getPathToResourceRelativeRes() { return sPathToResourceRelativeRes; }

    TextureHandle::~TextureHandle() {
        pTextureManager->releaseTextureResourceIfNotUsed(sPathToResourceRelativeRes);
    }

}
