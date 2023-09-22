#pragma once

// Standard.
#include <optional>
#include <filesystem>
#include <string>
#include <variant>

// Custom.
#include "misc/Error.h"

// External.
#include "Refureku/Refureku.h"

namespace ne {
    class Serializable;

    /**
     * Interface for implementing support for binary serialization of new field types.
     *
     * @remark Same as IFieldSerializer but stores data in binary form providing smaller size, faster
     * deserialization but sacrificing readability of the file.
     */
    class IBinaryFieldSerializer {
    public:
        IBinaryFieldSerializer() = default;
        virtual ~IBinaryFieldSerializer() = default;

        /**
         * Tests if this serializer supports serialization/deserialization of this field.
         *
         * @param pField Field to test for serialization/deserialization support.
         *
         * @return `true` if this serializer can be used to serialize this field, `false` otherwise.
         */
        virtual bool isFieldTypeSupported(const rfk::Field* pField) = 0;

        /**
         * Serializes the specified field into a binary file.
         *
         * @param pathToOutputDirectory     Path to the directory where the resulting file will be located.
         * @param sFilenameWithoutExtension Name of the resulting file without extension.
         * @param pFieldOwner               Field's owner.
         * @param pField                    Field to serialize.
         *
         * @return Error if something went wrong, otherwise file extension with a starting dot, for example:
         * `.meshbin`.
         */
        [[nodiscard]] virtual std::variant<std::string, Error> serializeField(
            const std::filesystem::path& pathToOutputDirectory,
            const std::string& sFilenameWithoutExtension,
            Serializable* pFieldOwner,
            const rfk::Field* pField) = 0;

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
            const rfk::Field* pField) = 0;
    };
} // namespace ne
