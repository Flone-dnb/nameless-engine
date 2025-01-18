#include "io/TextureImporter.h"

// Standard.
#include <cstring>

// Custom.
#include "io/Logger.h"
#include "misc/ProjectPaths.h"
#include "io/ConfigManager.h"

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

    CMP_FORMAT textureImportFormatToCmpFormat(TextureImportFormat format) {
        switch (format) {
        case (TextureImportFormat::R): {
            return CMP_FORMAT_BC4;
            break;
        }
        case (TextureImportFormat::RG): {
            return CMP_FORMAT_BC5;
            break;
        }
        case (TextureImportFormat::RGB): {
            return CMP_FORMAT_BC1;
            break;
        }
        case (TextureImportFormat::RGB_1BIT_A): {
            return CMP_FORMAT_BC1;
            break;
        }
        case (TextureImportFormat::RGB_8BIT_A): {
            return CMP_FORMAT_BC3;
            break;
        }
        case (TextureImportFormat::HDR): {
            return CMP_FORMAT_BC6H;
            break;
        }
        case (TextureImportFormat::RGB_HIGH_QUALITY): {
            return CMP_FORMAT_BC7;
            break;
        }
        case (TextureImportFormat::RGBA_HIGH_QUALITY): {
            return CMP_FORMAT_BC7;
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

    std::optional<Error> TextureImporter::importTexture(
        const std::filesystem::path& pathToTexture,
        TextureImportFormat textureImportFormat,
        const std::string& sPathToOutputDirRelativeRes,
        const std::string& sOutputDirectoryName,
        TextureFilteringPreference filteringPreference) {
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

        // Make sure it's indeed a directory.
        if (!std::filesystem::is_directory(pathToOutputDirectoryParent)) [[unlikely]] {
            return Error(std::format(
                "expected the specified path \"{}\" to point to a directory",
                pathToOutputDirectoryParent.string()));
        }

        // Make sure the specified directory name is not very long
        // to avoid creating long paths which might be an issue under Windows.
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

        // Make sure texture width/height is a multiple of 4 since block compression requires that.
        if (sourceTextureMipSet.m_nWidth % 4 != 0 || sourceTextureMipSet.m_nHeight % 4 != 0) [[unlikely]] {
            return Error(std::format(
                "width and height of the specified texture at \"{}\" should be a multiple of 4",
                pathToTexture.string()));
        }

        // Generate mipmaps.
        CMP_GenerateMIPLevels(&sourceTextureMipSet, 1);

        // Prepare compression options.
        KernelOptions kernelOptions;
        std::memset(&kernelOptions, 0, sizeof(KernelOptions));
        kernelOptions.format = textureImportFormatToCmpFormat(textureImportFormat);
        kernelOptions.fquality = 1.0F; // use highest quality
        kernelOptions.threads = 0;     // means automatically determine

        // Prepare import callback.
        const auto importCallback = []
#if defined(WIN32)
            (float progress, unsigned long long iNotUsed1, unsigned long long iNotUsed2)
#else
            (float progress, int* pNotUsed1, int* pNotUsed2)
#endif
        {
            Logger::get().info(std::format("texture import progress: {:.1F}", progress));
            return false;
        };

        // Compress the texture.
        CMP_MipSet compressedTextureMipSet;
        std::memset(&compressedTextureMipSet, 0, sizeof(CMP_MipSet));
        result =
            CMP_ProcessTexture(&sourceTextureMipSet, &compressedTextureMipSet, kernelOptions, importCallback);
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

        // Save filtering preference.
        ConfigManager config;
        serializeTextureFilteringPreference(config, filteringPreference);
        auto optionalError = config.saveFile(pathToOutputDirectory / sImportedTextureSettingsFileName, false);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return *optionalError;
        }

        // Build paths to resulting files.
        const auto pathToDds = pathToOutputDirectory / (std::string(sImportedFileName) + ".dds");
        const auto pathToKxt = pathToOutputDirectory / (std::string(sImportedFileName) + ".ktx");

        // Save compressed texture.
        auto ddsSaveResult = CMP_SaveTexture(pathToDds.string().c_str(), &compressedTextureMipSet);
        auto kxtSaveResult = CMP_SaveTexture(pathToKxt.string().c_str(), &compressedTextureMipSet);
        // check result later...

        // Free images.
        CMP_FreeMipSet(&sourceTextureMipSet);
        CMP_FreeMipSet(&compressedTextureMipSet);

        // Check DDS result.
        if (ddsSaveResult != CMP_OK) {
            return Error(std::format(
                "failed to save the resulting texture at \"{}\", error code: {}",
                pathToDds.string(),
                static_cast<int>(ddsSaveResult)));
        }

        // Check KTX result.
        if (kxtSaveResult != CMP_OK) {
            return Error(std::format(
                "failed to save the resulting texture at \"{}\", error code: {}",
                pathToKxt.string(),
                static_cast<int>(kxtSaveResult)));
        }

        return {};
    }

}
