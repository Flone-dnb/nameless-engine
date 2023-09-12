#include "materials/TextureManager.h"

// Standard.
#include <format>
#include <cstring>

// Custom.
#include "misc/ProjectPaths.h"
#include "io/Logger.h"
#include "render/general/resources/GpuResourceManager.h"
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "compressonator.h"

namespace ne {

    /** Singleton helper class to globally initialize Compressonator. */
    class CompressonatorSingleton {
    public:
        CompressonatorSingleton(const CompressonatorSingleton&) = delete;
        CompressonatorSingleton& operator=(const CompressonatorSingleton&) = delete;

        ~CompressonatorSingleton() = default;

        /** Creates a static Profiler instance. */
        static void initialize() { static CompressonatorSingleton compressonator; }

    private:
        /** Initializes Compressonator. */
        CompressonatorSingleton() { CMP_InitFramework(); }
    };

    CMP_FORMAT textureTypeToCmpFormat(ne::TextureType type) {
        switch (type) {
        case (ne::TextureType::DIFFUSE_TEXTURE): {
            return CMP_FORMAT_BC3; // using BC3 instead of BC7 because:
                                   // 1. Compressonator compresses to BC7 with errors on Linux so to
                                   // be consistent across all platforms using BC3,
                                   // 2. BC7 takes quite more time to compress especially for high-res
                                   // textures such as 4K
            break;
        }
        case (ne::TextureType::NORMAL_TEXTURE): {
            return CMP_FORMAT_BC5;
            break;
        }
        case (ne::TextureType::HDR_TEXTURE): {
            return CMP_FORMAT_BC6H;
            break;
        }
        default: {
            Error error("unhandled case");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            break;
        }
        }
    }

    std::optional<Error> TextureManager::importTexture(
        const std::filesystem::path& pathToTexture,
        TextureType textureType,
        const std::string& sPathToOutputDirRelativeRes,
        const std::string& sOutputDirectoryName,
        ImportTextureCallback pCompressionStateCallback) {
        // Make sure the specified path to the texture exists.
        if (!std::filesystem::exists(pathToTexture)) [[unlikely]] {
            return Error(std::format("the specified path \"{}\" does not exists", pathToTexture.string()));
        }

        // Construct an absolute path to the output directory.
        const auto pathToOutputDirectoryParent =
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / sPathToOutputDirRelativeRes;

        // Make sure the path to output directory exists.
        if (!std::filesystem::exists(pathToOutputDirectoryParent)) [[unlikely]] {
            return Error(std::format(
                "the specified path \"{}\" does not exists", pathToOutputDirectoryParent.string()));
        }

        // Make sure the specified directory name is not very long.
        static constexpr size_t iMaxOutputDirectoryNameLength = 10; // NOLINT
        if (sOutputDirectoryName.size() > iMaxOutputDirectoryNameLength) [[unlikely]] {
            return Error(std::format(
                "the specified name \"{}\" is too long (only {} characters allowed)",
                sOutputDirectoryName,
                iMaxOutputDirectoryNameLength));
        }

        // Make sure the specified directory name is valid (A-z, 0-9).
        for (const auto& character : sOutputDirectoryName) {
            const auto iAsciiCode = static_cast<int>(character);
            if (iAsciiCode < 48 || (iAsciiCode > 57 && iAsciiCode < 65) ||               // NOLINT
                (iAsciiCode > 90 && iAsciiCode < 97) || iAsciiCode > 122) [[unlikely]] { // NOLINT
                return Error(std::format(
                    "character \"{}\" in the name \"{}\" is forbidden and cannon be used",
                    character,
                    sOutputDirectoryName));
            }
        }

        // Make sure the specified resulting directory does not exists yet.
        const auto pathToOutputDirectory = pathToOutputDirectoryParent / sOutputDirectoryName;
        if (std::filesystem::exists(pathToOutputDirectory)) [[unlikely]] {
            return Error(std::format(
                "expected the resulting directory \"{}\" to not exist", pathToOutputDirectory.string()));
        }

        // Initialize compressonator.
        CompressonatorSingleton::initialize();

        // Load the texture.
        CMP_MipSet sourceTextureMipSet;
        std::memset(&sourceTextureMipSet, 0, sizeof(CMP_MipSet));
        auto result = CMP_LoadTexture(pathToTexture.string().c_str(), &sourceTextureMipSet);
        if (result != CMP_OK) {
            return Error(std::format(
                "failed to load the texture at \"{}\", error code: {}",
                pathToTexture.string(),
                static_cast<int>(result)));
        }

        // Make sure texture width/height is a multiple of 4 since texture loaders will require that
        // if BC3 or a similar format is used.
        if (sourceTextureMipSet.m_nWidth % 4 != 0 || sourceTextureMipSet.m_nHeight % 4 != 0) [[unlikely]] {
            return Error(std::format(
                "width and height of the specified texture at \"{}\" should be a multiple of 4",
                pathToTexture.string()));
        }

        // Generate mip levels.
        static constexpr CMP_INT iMinMipLevelResolutionInPixels = 32; // NOLINT
        CMP_GenerateMIPLevels(&sourceTextureMipSet, iMinMipLevelResolutionInPixels);

        // Prepare compression options.
        KernelOptions kernelOptions;
        std::memset(&kernelOptions, 0, sizeof(KernelOptions));
        kernelOptions.format = textureTypeToCmpFormat(textureType);
        kernelOptions.fquality = 1.0F; // use highest quality
        kernelOptions.threads = 0;     // means automatically determine

        // Compress the texture.
        CMP_MipSet compressedTextureMipSet;
        std::memset(&compressedTextureMipSet, 0, sizeof(CMP_MipSet));
        result = CMP_ProcessTexture(
            &sourceTextureMipSet, &compressedTextureMipSet, kernelOptions, pCompressionStateCallback);
        if (result != CMP_OK) {
            // Free source image.
            CMP_FreeMipSet(&sourceTextureMipSet);

            // Check if the operation failed or was canceled.
            if (result != CMP_ABORTED) {
                return Error(std::format(
                    "failed to compress the texture at \"{}\", error code: {}",
                    pathToTexture.string(),
                    static_cast<int>(result)));
            }

            return Error(std::format(
                "compression of the texture at \"{}\" was canceled by the user", pathToTexture.string()));
        }

        // Create output directory.
        std::filesystem::create_directory(pathToOutputDirectory);

        // Build paths to resulting files.
        const auto pathToDds = pathToOutputDirectory / "0.dds";
        const auto pathToKxt = pathToOutputDirectory / "0.ktx";

        // Save compressed texture.
        auto ddsSaveResult = CMP_SaveTexture(pathToDds.string().c_str(), &compressedTextureMipSet);
        auto kxtSaveResult = CMP_SaveTexture(pathToKxt.string().c_str(), &compressedTextureMipSet);
        // check result later

        // Free images.
        CMP_FreeMipSet(&sourceTextureMipSet);
        CMP_FreeMipSet(&compressedTextureMipSet);

        // Check final result.
        if (ddsSaveResult != CMP_OK) {
            return Error(std::format(
                "failed to save the resulting texture at \"{}\", error code: {}",
                pathToDds.string(),
                static_cast<int>(ddsSaveResult)));
        }
        if (kxtSaveResult != CMP_OK) {
            return Error(std::format(
                "failed to save the resulting texture at \"{}\", error code: {}",
                pathToKxt.string(),
                static_cast<int>(kxtSaveResult)));
        }

        return {};
    }

    TextureManager::TextureManager(GpuResourceManager* pResourceManager) {
        // Save resource manager.
        this->pResourceManager = pResourceManager;

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
                "textures in memory now: {}, currently used VRAM by renderer: {} MB",
                sPathToResourceRelativeRes,
                getTextureInMemoryCount(),
                pResourceManager->getUsedVideoMemoryInMb()));
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

        // Append file name.
        pathToResource /= "0" + sTextureFormatExtension;

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
            "texture \"{}\" was loaded from disk into memory, textures in memory now: {}, currently used "
            "VRAM by renderer: {} MB",
            sPathToResourceRelativeRes,
            getTextureInMemoryCount(),
            pResourceManager->getUsedVideoMemoryInMb()));

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
