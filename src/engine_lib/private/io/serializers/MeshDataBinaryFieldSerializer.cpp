#include "io/serializers/MeshDataBinaryFieldSerializer.h"

// Standard.
#include <fstream>
#include <limits>

// Custom.
#include "game/node/MeshNode.h"
#include "io/serializers/SerializableObjectFieldSerializer.h"

namespace ne {

    bool MeshDataBinaryFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        // Get field type.
        const auto& fieldType = pField->getType();
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        // Check if we support this type.
        return fieldType.getArchetype() != nullptr &&
               SerializableObjectFieldSerializer::isDerivedFromSerializable(fieldType.getArchetype()) &&
               sFieldCanonicalTypeName == "ne::MeshData";
    }

    // Prepare type to store "size" values in the file.
    using length_t = unsigned int;
    static constexpr size_t iLengthMax = std::numeric_limits<length_t>::max();

    std::variant<std::string, Error> MeshDataBinaryFieldSerializer::serializeField(
        const std::filesystem::path& pathToOutputDirectory,
        const std::string& sFilenameWithoutExtension,
        Serializable* pFieldOwner,
        const rfk::Field* pField) {
        // Make sure the specified directory exists.
        if (!std::filesystem::exists(pathToOutputDirectory)) [[unlikely]] {
            return Error(
                std::format("the specified directory \"{}\" does not exist", pathToOutputDirectory.string()));
        }

        // Make sure it's indeed a directory.
        if (!std::filesystem::is_directory(pathToOutputDirectory)) [[unlikely]] {
            return Error(std::format(
                "expected the specified path \"{}\" to point to a directory",
                pathToOutputDirectory.string()));
        }

        // Construct resulting path.
        std::string sFilename = sFilenameWithoutExtension + pMeshDataFileExtension;
        const auto pathToOutputFile = pathToOutputDirectory / sFilename;

        // Get pointer to field data.
        const auto pSerializable = static_cast<Serializable*>(pField->getPtrUnsafe(pFieldOwner));
        const auto pMeshData = dynamic_cast<MeshData*>(pSerializable);
        if (pMeshData == nullptr) [[unlikely]] {
            return Error("failed to cast field object to MeshData");
        }

#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(MeshData) == 160, "update serialization"); // NOLINT
#elif defined(DEBUG)
        static_assert(sizeof(MeshData) == 128, "update serialization"); // NOLINT
#endif

        // Create the resulting file.
        std::ofstream file(pathToOutputFile, std::ios::binary);
        if (!file.is_open()) [[unlikely]] {
            return Error(std::format("failed to create/overwrite a file at {}", pathToOutputFile.string()));
        }

        // Get vertices.
        const auto pVertices = pMeshData->getVertices();

        // Make sure vertex count will not exceed used type limit.
        if (pVertices->size() > iLengthMax) [[unlikely]] {
            return Error(std::format(
                "mesh vertex count {} exceeds used type limit of {}", pVertices->size(), iLengthMax));
        }

        // Specify how much vertices we have.
        const length_t iVertexCount = static_cast<length_t>(pVertices->size());
        file.write(reinterpret_cast<const char*>(&iVertexCount), sizeof(iVertexCount));

        // Write vertices.
#if defined(DEBUG)
        static_assert(sizeof(MeshVertex) == 32, "update vertex serialization");
#endif
        file.write(reinterpret_cast<const char*>(pVertices->data()), iVertexCount * sizeof(MeshVertex));

        // Get indices.
        const auto pIndices = pMeshData->getIndices();

        // Make sure material slot count will not exceed used type limit.
        if (pIndices->size() > iLengthMax) [[unlikely]] {
            return Error(std::format(
                "material slot count {} exceeds used type limit of {}", pIndices->size(), iLengthMax));
        }

        // Specify how much material slots we have.
        const length_t iMaterialSlotCount = static_cast<length_t>(pIndices->size());
        file.write(reinterpret_cast<const char*>(&iMaterialSlotCount), sizeof(iMaterialSlotCount));

        // Write indices.
        for (const auto& vIndices : *pIndices) {
            // Make sure index count will not exceed used type limit.
            if (vIndices.size() > iLengthMax) [[unlikely]] {
                return Error(
                    std::format("index count {} exceeds used type limit of {}", vIndices.size(), iLengthMax));
            }

            // Specify how much indices we have in this slot.
            const length_t iIndexCount = static_cast<length_t>(vIndices.size());
            file.write(reinterpret_cast<const char*>(&iIndexCount), sizeof(iIndexCount));

            // Write indices.
            file.write(reinterpret_cast<const char*>(vIndices.data()), iIndexCount * sizeof(vIndices[0]));
        }

        // Finished with the file.
        file.close();

        return sFilename;
    }

    std::optional<Error> MeshDataBinaryFieldSerializer::deserializeField(
        const std::filesystem::path& pathToBinaryFile, Serializable* pFieldOwner, const rfk::Field* pField) {
        // Make sure the specified file exists.
        if (!std::filesystem::exists(pathToBinaryFile)) [[unlikely]] {
            return Error(std::format("the specified file \"{}\" does not exist", pathToBinaryFile.string()));
        }

        // Make sure it's indeed a file.
        if (std::filesystem::is_directory(pathToBinaryFile)) [[unlikely]] {
            return Error(std::format(
                "expected the specified path \"{}\" to point to a file", pathToBinaryFile.string()));
        }

        // Get pointer to field data.
        const auto pSerializable = static_cast<Serializable*>(pField->getPtrUnsafe(pFieldOwner));
        const auto pMeshData = dynamic_cast<MeshData*>(pSerializable);
        if (pMeshData == nullptr) [[unlikely]] {
            return Error("failed to cast field object to MeshData");
        }

#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(MeshData) == 160, "update deserialization"); // NOLINT
#elif defined(DEBUG)
        static_assert(sizeof(MeshData) == 128, "update deserialization"); // NOLINT
#endif

        // Clear any existing data.
        pMeshData->getVertices()->clear();
        pMeshData->getIndices()->clear();

        // Open the file.
        std::ifstream file(pathToBinaryFile, std::ios::binary);
        if (!file.is_open()) [[unlikely]] {
            return Error(std::format("failed to open the file at {}", pathToBinaryFile.string()));
        }

        // Get vertices.
        const auto pVertices = pMeshData->getVertices();

        // Read how much vertices we have.
        length_t iVertexCount = 0;
        file.read(reinterpret_cast<char*>(&iVertexCount), sizeof(iVertexCount));

        // Allocate vertices.
        pVertices->resize(iVertexCount);

        // Read vertices.
#if defined(DEBUG)
        static_assert(sizeof(MeshVertex) == 32, "update vertex deserialization");
#endif
        file.read(reinterpret_cast<char*>(pVertices->data()), iVertexCount * sizeof(MeshVertex));

        // Get indices.
        const auto pIndices = pMeshData->getIndices();

        // Read how much material slots we have.
        length_t iMaterialSlotCount = 0;
        file.read(reinterpret_cast<char*>(&iMaterialSlotCount), sizeof(iMaterialSlotCount));

        // Allocate material slots.
        pIndices->resize(iMaterialSlotCount);

        // Read indices.
        for (auto& vIndices : *pIndices) {
            // Read how much indices we have in this slot.
            length_t iIndexCount = 0;
            file.read(reinterpret_cast<char*>(&iIndexCount), sizeof(iIndexCount));

            // Allocate indices.
            vIndices.resize(iIndexCount);

            // Read indices.
            file.read(reinterpret_cast<char*>(vIndices.data()), iIndexCount * sizeof(vIndices[0]));
        }

        // Finished with the file.
        file.close();

        return {};
    }

}
