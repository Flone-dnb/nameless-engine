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
     *
     * @warning Note that class name and the namespace it's located in (if exists) are used for serialization,
     * if you change your class name or namespace its located in this will change class ID and make all
     * previously serialized versions of your class reference old class ID which will break deserialization
     * for all previously serialized versions of your class.
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
         * @remark Type will be serialized as a class ID which will change if you change your class
         * name or if your class is defined in a namespace if you change your namespace name.
         *
         * @remark Note that not all reflected fields can be serialized, only specific types can be
         * serialized. Const fields, pointer fields, lvalue references, rvalue references and C-arrays will
         * always be ignored and will not be serialized (no error returned).
         * Supported for serialization types are:
         * - bool
         * - int
         * - long long
         * - float
         * - double
         * - std::string
         * - T (where T is any type that derives from Serializable class)
         *
         * @remark You can mark reflected property as DontSerialize so it will be ignored in the serialization
         * process. Note that you don't need to mark fields of types that are always ignored (const, pointers,
         * etc.) because they will be ignored anyway. Example:
         * @code
         * NEPROPERTY(DontSerialize)
         * int iKey = 42; // will be ignored and not serialized
         * @endcode
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field.
         */
        std::optional<Error> serialize(const std::filesystem::path& pathToFile);

        /**
         * Serializes the type and all reflected fields (including inherited) into a toml value.
         *
         * This is an overloaded function. See full documentation for other overload.
         *
         * @param tomlData        Toml value to append this object to.
         * @param sEntityId       Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated.
         *
         * @warning Don't use dots in the entity ID, dots are used
         * in recursion when this function is called from this function to process reflected field (sub
         * entity).
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field, otherwise name of the section that was used to store this entity.
         */
        std::variant<std::string, Error> serialize(toml::value& tomlData, std::string sEntityId = "");

        /**
         * Deserializes the type and all reflected fields (including inherited) from a file.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, a unique pointer to deserialized entity.
         * Use a dynamic_cast to cast to a wanted type.
         */
        static std::variant<std::unique_ptr<Serializable>, Error>
        deserialize(const std::filesystem::path& pathToFile);

        /**
         * Deserializes the type and all reflected fields (including inherited) from a toml value.
         *
         * @param tomlData        Toml value to retrieve an object from.
         * @param sEntityId       Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated.
         *
         * @warning Don't use dots in the entity ID, dots are used
         * in recursion when this function is called from this function to process reflected field (sub
         * entity).
         *
         * @return Error if something went wrong, a unique pointer to deserialized entity.
         * Use a dynamic_cast to cast to wanted type.
         */
        static std::variant<std::unique_ptr<Serializable>, Error>
        deserialize(toml::value& tomlData, std::string sEntityId = "");

        /** test */
        NEPROPERTY()
        int iAnswer = 42;

    private:
        /**
         * Returns whether the specified field can be serialized or not.
         *
         * @param field Field to test.
         *
         * @return Whether the field can be serialized or not.
         */
        static bool isFieldSerializable(rfk::Field const& field);

        /**
         * Clones reflected serializable fields of one object to another.
         *
         * @param pFrom Object to clone fields from.
         * @param pTo   Object to clone fields to.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> cloneSerializableObject(Serializable* pFrom, Serializable* pTo);

        /**
         * Clones field's data from one field to another if fields' types (specified by the template argument)
         * are the same.
         *
         * @param pFrom     Object to clone field's data from.
         * @param fieldFrom Field to clone data from.
         * @param pTo       Object to clone field's data to.
         * @param fieldTo   Field to clone data to.
         *
         * @return 'true' if data was moved, 'false' if fields have different types.
         */
        template <typename T>
        static bool cloneFieldIfMatchesType(
            Serializable* pFrom, rfk::Field const& fieldFrom, Serializable* pTo, rfk::Field const* fieldTo) {
            if (fieldFrom.getType().match(rfk::getType<T>())) {
                auto value = fieldFrom.getUnsafe<T>(pFrom);
                fieldTo->setUnsafe<T>(pTo, std::move(value));
                return true;
            } else {
                return false;
            }
        }

        /** Name of the key in which to store name of the field a section represents. */
        static inline auto const sSubEntityFieldNameKey = ".field_name";

        ne_Serializable_GENERATED
    };
}; // namespace )

File_Serializable_GENERATED