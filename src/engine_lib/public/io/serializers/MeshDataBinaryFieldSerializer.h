#pragma once

// Custom.
#include "io/serializers/IBinaryFieldSerializer.hpp"

namespace ne {
    /** Serializer for fields of `MeshData` type. */
    class MeshDataBinaryFieldSerializer : public IBinaryFieldSerializer {
    public:
        MeshDataBinaryFieldSerializer() = default;
        virtual ~MeshDataBinaryFieldSerializer() override = default;

        /**
         * Tests if this serializer supports serialization/deserialization of this field.
         *
         * @param pField Field to test for serialization/deserialization support.
         *
         * @return `true` if this serializer can be used to serialize this field, `false` otherwise.
         */
        virtual bool isFieldTypeSupported(const rfk::Field* pField) override;

        /**
         * Serializes the specified field into a binary file.
         *
         * @param pathToOutputDirectory     Path to the directory where the resulting file will be located.
         * @param sFilenameWithoutExtension Name of the resulting file without extension.
         * @param pFieldOwner               Field's owner.
         * @param pField                    Field to serialize.
         *
         * @return Error if something went wrong, otherwise full name of the resulting file (including file
         * extension).
         */
        [[nodiscard]] virtual std::variant<std::string, Error> serializeField(
            const std::filesystem::path& pathToOutputDirectory,
            const std::string& sFilenameWithoutExtension,
            Serializable* pFieldOwner,
            const rfk::Field* pField) override;

        /**
         * Deserializes data from a binary file into the specified field.
         *
         * @param pathToBinaryFile  Path to the binary file to deserialize.
         * @param pFieldOwner       Field's owner.
         * @param pField            Field to deserialize TOML value to.
         *
         * @return Error if something went wrong, empty otherwise.
         */
        [[nodiscard]] virtual std::optional<Error> deserializeField(
            const std::filesystem::path& pathToBinaryFile,
            Serializable* pFieldOwner,
            const rfk::Field* pField) override;

    private:
        /** File extension for the file that is used to store mesh data. */
        static constexpr auto pMeshDataFileExtension = ".mbin";
    };
}
