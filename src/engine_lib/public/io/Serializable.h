#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"

// External.
#include "Refureku/Object.h"

#include "Serializable.generated.h"

namespace ne NENAMESPACE() {
    /**
     * Base class for making a serializable type.
     *
     * Inherit your class from this type to add a 'serialize' function which will
     * serialize the type and all reflected fields (even inherited) into a file.
     */
    class NECLASS() Serializable : public rfk::Object {
    public:
        Serializable() = default;
        virtual ~Serializable() override = default;

        /**
         * Serializes the type and all reflected fields (including inherited) into a file.
         *
         * @param pathToFile File to write reflected data to. The ".toml" extension will be added
         * automatically if not specified in the path. If the specified file already exists it will be
         * overwritten.
         *
         * @remark Note that not all reflected fields can be serialized, only specific types can be
         * serialized. If a reflected field has unsupported type it will be ignored and an error will be added
         * to the log. Const fields, pointer fields, lvalue references, rvalue references and C-arrays will
         * always be ignored and will not be serialized (no errors in the log in this case).
         * Supported for serialization types are:
         * - bool
         * - int
         * - long long
         * - float
         * - std::string
         */
        void serialize(const std::filesystem::path& pathToFile);

        /**
         * Deserializes the type and all reflected fields (including inherited) into a file.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, a unique pointer to deserialized entity.
         * Use a dynamic_cast to cast to wanted type.
         */
        static std::variant<std::unique_ptr<Serializable>, Error>
        deserialize(const std::filesystem::path& pathToFile);

        ne_Serializable_GENERATED
    };
}; // namespace )

File_Serializable_GENERATED