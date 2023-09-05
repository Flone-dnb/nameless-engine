#include "materials/TextureManager.h"

// Standard.
#include <format>
#include <cstring>

// Custom.
#include "misc/ProjectPaths.h"

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

}
