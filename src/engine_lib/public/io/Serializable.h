#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"
#include "io/DontSerializeProperty.h"

// External.
#include "Refureku/Object.h"
#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include "toml11/toml.hpp"

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
         * - double
         * - std::string
         *
         * @remark You can mark reflected property as DontSerialize so it will be ignored in the serialization
         * process. Note that you don't need to mark fields of types that are always ignored (const, pointers,
         * etc.) because they will be ignored anyway. Example:
         * @code
         * NEPROPERTY(DontSerialize)
         * int iKey = 42; // will be ignored and not serialized
         * @endcode
         */
        void serialize(const std::filesystem::path& pathToFile);

        /**
         * Serializes the type and all reflected fields (including inherited) into a toml value.
         *
         * This is an overloaded function. See full documentation for other overload.
         *
         * @param tomlData        Toml value to append this object to.
         * @param iEntityUniqueId Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated.
         */
        void serialize(toml::value& tomlData, size_t iEntityUniqueId = 0);

        /**
         * Deserializes the type and all reflected fields (including inherited) from a file.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, a unique pointer to deserialized entity.
         * Use a dynamic_cast to cast to wanted type.
         */
        static std::variant<std::unique_ptr<Serializable>, Error>
        deserialize(const std::filesystem::path& pathToFile);

        /**
         * Deserializes the type and all reflected fields (including inherited) from a toml value.
         *
         * @param tomlData        Toml value to retrieve an object from.
         * @param iEntityUniqueId Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated.
         *
         * @return Error if something went wrong, a unique pointer to deserialized entity.
         * Use a dynamic_cast to cast to wanted type.
         */
        static std::variant<std::unique_ptr<Serializable>, Error>
        deserialize(toml::value& tomlData, size_t iEntityUniqueId = 0);

        ne_Serializable_GENERATED
    };
}; // namespace )

File_Serializable_GENERATED