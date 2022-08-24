#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>
#include <ranges>

// Custom.
#include "misc/Error.h"
#include "io/DontSerializeProperty.h"
#include "io/Logger.h"
#include "io/ConfigManager.h"

// External.
#include "Refureku/Refureku.h"
#include "Refureku/Object.h"
#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include "toml11/toml.hpp"

#include "Reflection.h"

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
         * Serialized entity can later be deserialized using @ref deserialize.
         *
         * @param pathToFile    File to write reflected data to. The ".toml" extension will be added
         * automatically if not specified in the path. If the specified file already exists it will be
         * overwritten.
         * @param bEnableBackup If 'true' will also use a backup (copy) file. @ref deserialize can use
         * backup file if the original file does not exist. Generally you want to use
         * a backup file if you are saving important information, such as player progress,
         * other cases such as player game settings and etc. usually do not need a backup but
         * you can use it if you want.
         *
         * @warning Type will be serialized as a class ID which will change if you change your class
         * name or if your class is defined in a namespace if you change your namespace name. If your class ID
         * was changed this means that all previously serialized files will reference invalid class ID and you
         * won't be able to deserialize them (unless you manually change class ID in previously serialized
         * files).
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
        std::optional<Error> serialize(const std::filesystem::path& pathToFile, bool bEnableBackup);

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
         * Specify the type of the entity (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * Example:
         * @code
         * ne::Node myCoolNode("My Cool Node");
         * auto optionalError = myCoolNode.serialize(pathToFile, false);
         * // ...
         * auto result = Serializable::deserialize<ne::Node>(pathToFile);
         * if (std::holds_alternative<ne::Error>(result)){
         *     // process error here
         * }
         * auto pMyCoolNode = std::get<std::unique_ptr<ne::Node>>(std::move(result));
         * @endcode
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, a unique pointer to deserialized entity.
         */
        template <typename T>
        requires std::derived_from<T, Serializable>
        static std::variant<std::unique_ptr<T>, Error> deserialize(const std::filesystem::path& pathToFile);

        /**
         * Deserializes the type and all reflected fields (including inherited) from a toml value.
         * Specify the type of the entity (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
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
        template <typename T>
        requires std::derived_from<T, Serializable>
        static std::variant<std::unique_ptr<T>, Error>
        deserialize(toml::value& tomlData, std::string sEntityId = "");

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
         * Tests whether the specified archetype is Serializable or derives at some point from Serializable
         * class.
         *
         * @param pArchetype Archetype to test.
         *
         * @return Whether the specified archetype is derived from Serializable or not.
         */
        static bool isDerivedFromSerializable(rfk::Archetype const* pArchetype);

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

    template <typename T>
    requires std::derived_from<T, Serializable> std::variant<std::unique_ptr<T>, Error>
    Serializable::deserialize(const std::filesystem::path& pathToFile) {
        // Add TOML extension to file.
        auto fixedPath = pathToFile;
        if (!fixedPath.string().ends_with(".toml")) {
            fixedPath += ".toml";
        }

        std::filesystem::path backupFile = fixedPath;
        backupFile += ConfigManager::getBackupFileExtension();

        if (!std::filesystem::exists(fixedPath)) {
            // Check if backup file exists.
            if (std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(backupFile, fixedPath);
            } else {
                return Error("file or backup file do not exist");
            }
        }

        // Load file.
        toml::value tomlData;
        try {
            tomlData = toml::parse(fixedPath);
        } catch (std::exception& exception) {
            return Error(
                std::format("failed to load file \"{}\", error: {}", fixedPath.string(), exception.what()));
        }

        // Deserialize.
        auto result = deserialize<T>(tomlData);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        } else {
            return result;
        }
    }

    template <typename T>
    requires std::derived_from<T, Serializable> std::variant<std::unique_ptr<T>, Error>
    Serializable::deserialize(toml::value & tomlData, std::string sEntityId) {
        if (sEntityId.empty()) {
            // Put something as entity ID so it would not look weird.
            sEntityId = "0";
        }

        // Read all sections.
        std::vector<std::string> vSections;
        const auto fileTable = tomlData.as_table();
        for (const auto& [key, value] : fileTable) {
            if (value.is_table()) {
                vSections.push_back(key);
            }
        }

        // Check that we have at least one section.
        if (vSections.empty()) {
            return Error("provided toml value has zero sections while expected at least one section");
        }

        // Find a section that starts with the specified entity ID.
        // Look for a value between dots.
        // We can't just use sSectionName.starts_with(sEntityId) because we might make a mistake in the
        // following situation: [100.10.1014674670888563010] with entity ID equal to "10".
        std::string sTargetSection;
        size_t iClassId = 0;
        // Each entity section has the following format: [entityId.classId]
        // For sub entities (field with reflected type) format: [parentEntityId.childEntityId.childClassId]
        for (const auto& sSectionName : vSections) {
            const auto iIdEndDotPos = sSectionName.rfind('.');
            if (iIdEndDotPos == std::string::npos) [[unlikely]] {
                return Error("provided toml value does not contain entity ID");
            }
            if (iIdEndDotPos + 1 == sSectionName.size()) [[unlikely]] {
                return Error(std::format("section name \"{}\" does not have a class ID", sSectionName));
            }
            if (iIdEndDotPos == 0) [[unlikely]] {
                return Error(std::format("section \"{}\" is not full", sSectionName));
            }

            // Get ID chain (either entity ID or something like "parentEntityId.childEntityId").
            const auto sIdChain = sSectionName.substr(0, iIdEndDotPos);
            if (sIdChain == sEntityId) {
                sTargetSection = sSectionName;

                // Get this entity's class ID.
                try {
                    iClassId = std::stoull(sSectionName.substr(iIdEndDotPos + 1));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "failed to convert string to unsigned long long when retrieving class ID for section "
                        "\"{}\": {}",
                        sSectionName,
                        ex.what()));
                }
                break;
            }
        }

        // Check if anything was found.
        if (sTargetSection.empty()) {
            return Error(std::format("could not find entity with ID \"{}\"", sEntityId));
        }

        // Get all keys (field names) from this section.
        toml::value section;
        try {
            section = toml::find(tomlData, sTargetSection);
        } catch (std::exception& ex) {
            return Error(std::format("no section \"{}\" was found ({})", sTargetSection, ex.what()));
        }

        if (!section.is_table()) {
            return Error(std::format("found \"{}\" section is not a section", sTargetSection));
        }

        const auto sectionTable = section.as_table();
        std::vector<std::string> vKeys;
        for (const auto& key : sectionTable | std::views::keys) {
            vKeys.push_back(key);
        }

        rfk::Class const* pClass = rfk::getDatabase().getClassById(iClassId);
        if (!pClass) {
            return Error(std::format("no class found in the reflection database by ID {}", iClassId));
        }
        if (!isDerivedFromSerializable(pClass)) {
            return Error(std::format(
                "deserialized class with ID {} does not derive from {}",
                iClassId,
                staticGetArchetype().getName()));
        }

        auto pInstance = pClass->makeUniqueInstance<T>();
        if (!pInstance) {
            return Error(
                std::format("unable to make a Serializable object from class \"{}\"", pClass->getName()));
        }

        for (auto& sFieldName : vKeys) {
            if (sFieldName == sSubEntityFieldNameKey) {
                // This field is used as section metadata and tells us what field of parent entity
                // this section describes.
                continue;
            }

            // Read value from TOML.
            toml::value value;
            try {
                value = toml::find(section, sFieldName);
            } catch (std::exception& exception) {
                return Error(std::format(
                    "field \"{}\" was not found in the specified toml value: {}",
                    sFieldName,
                    exception.what()));
            }

            // Get field by name.
            rfk::Field const* pField =
                pClass->getFieldByName(sFieldName.c_str(), rfk::EFieldFlags::Default, true);
            if (!pField) {
                Logger::get().warn(
                    std::format(
                        "field name \"{}\" exists in the specified toml value but does not exist in the "
                        "actual object (if you removed this reflected field from your "
                        "class - ignore this warning)",
                        sFieldName),
                    "");
                continue;
            }
            const auto& fieldType = pField->getType();

            if (!isFieldSerializable(*pField))
                continue;

            // Set field value depending on field type.
            if (fieldType.match(rfk::getType<bool>()) && value.is_boolean()) {
                auto fieldValue = value.as_boolean();
                pField->setUnsafe<bool>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<int>()) && value.is_integer()) {
                auto fieldValue = static_cast<int>(value.as_integer());
                pField->setUnsafe<int>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<long long>()) && value.is_integer()) {
                long long fieldValue = value.as_integer();
                pField->setUnsafe<long long>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<float>()) && value.is_floating()) {
                auto fieldValue = static_cast<float>(value.as_floating());
                pField->setUnsafe<float>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<double>()) && value.is_string()) {
                // Double is stored as a string for better precision.
                try {
                    double fieldValue = std::stod(value.as_string().str);
                    pField->setUnsafe<double>(pInstance.get(), std::move(fieldValue));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "failed to convert string to double for field \"{}\": {}", sFieldName, ex.what()));
                }
            } else if (fieldType.match(rfk::getType<std::string>()) && value.is_string()) {
                auto fieldValue = value.as_string().str;
                pField->setUnsafe<std::string>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.getArchetype()) {
                // Field with a reflected type.
                // Find a section that has the key ".field_name = *our field name*".
                std::string sSectionNameForField;
                // This will have minimum value of 1 where the dot separates IDs from class ID.
                const auto iNumberOfDotsInTargetSectionName =
                    std::ranges::count(sTargetSection.begin(), sTargetSection.end(), '.');
                for (const auto& sSectionName : vSections) {
                    if (sSectionName == sTargetSection)
                        continue; // skip self

                    const auto iNumberOfDotsInSectionName =
                        std::ranges::count(sSectionName.begin(), sSectionName.end(), '.');

                    // Look for a section that has 1 more dot than our section. Example:
                    // Our section: ["0.3056171360419407975"]
                    // Child section that we are looking for: ["0.1.4321359943817265529"]
                    if (iNumberOfDotsInSectionName != iNumberOfDotsInTargetSectionName + 1) {
                        continue;
                    }

                    // Here we might get into the following situation:
                    // Our section: "0.3056171360419407975"
                    // Current section: "1.0.3056171360419407975" - first field of some OTHER entity.

                    // Get entity ID chain.
                    auto iLastDotPos = sSectionName.rfind('.');
                    if (iLastDotPos == std::string::npos) {
                        return Error(std::format("section name \"{}\" is corrupted", sSectionName));
                    }
                    // Will be something like: "entityId.subEntityId", "entityId.subEntityId.subSubEntityId"
                    // or etc.
                    auto sEntityIdChain = sSectionName.substr(0, iLastDotPos);

                    // Remove last entity id from chain because we don't know it.
                    iLastDotPos = sEntityIdChain.rfind('.');
                    if (iLastDotPos == std::string::npos) {
                        return Error(std::format("section name \"{}\" is corrupted", sSectionName));
                    }
                    sEntityIdChain = sEntityIdChain.substr(0, iLastDotPos);

                    // Check that this is indeed our sub entity.
                    if (sEntityIdChain != sEntityId) {
                        continue;
                    }

                    // Get this section.
                    toml::value entitySection;
                    try {
                        entitySection = toml::find(tomlData, sSectionName);
                    } catch (std::exception& ex) {
                        return Error(
                            std::format("no section \"{}\" was found ({})", sTargetSection, ex.what()));
                    }

                    if (!section.is_table()) {
                        return Error(std::format("found \"{}\" section is not a section", sTargetSection));
                    }

                    // Look for a key that holds field name.
                    toml::value fieldKey;
                    try {
                        fieldKey = toml::find(entitySection, sSubEntityFieldNameKey);
                    } catch (...) {
                        // Not found, go to next section.
                        continue;
                    }

                    if (!fieldKey.is_string()) {
                        return Error(std::format(
                            "found field name key \"{}\" is not a string", sSubEntityFieldNameKey));
                    }

                    if (fieldKey.as_string() == sFieldName) {
                        sSectionNameForField = sSectionName;
                        break;
                    }
                }

                if (sSectionNameForField.empty()) {
                    return Error(
                        std::format("could not find a section that represents field \"{}\"", sFieldName));
                }

                // Cut field's class ID from the section name.
                // The section name could look something like this: [entityId.subEntityId.subEntityClassId].
                const auto iSubEntityClassIdDotPos = sSectionNameForField.rfind('.');
                if (iSubEntityClassIdDotPos == std::string::npos) [[unlikely]] {
                    return Error(std::format(
                        "sub entity does not have a class ID (section: \"{}\")", sSectionNameForField));
                }
                if (iSubEntityClassIdDotPos + 1 == sSectionNameForField.size()) [[unlikely]] {
                    return Error(
                        std::format("section name \"{}\" does not have a class ID", sSectionNameForField));
                }
                if (iSubEntityClassIdDotPos == 0) [[unlikely]] {
                    return Error(std::format("section \"{}\" is not full", sSectionNameForField));
                }
                const auto sSubEntityId = sSectionNameForField.substr(0, iSubEntityClassIdDotPos);

                // Deserialize section into an object.
                auto result = deserialize<Serializable>(tomlData, sSubEntityId);
                if (std::holds_alternative<Error>(result)) {
                    auto err = std::get<Error>(std::move(result));
                    err.addEntry();
                    return err;
                }
                auto pSubEntity = std::get<std::unique_ptr<Serializable>>(std::move(result));

                // Move object to field.
                cloneSerializableObject(
                    pSubEntity.get(), static_cast<Serializable*>(pField->getPtrUnsafe(pInstance.get())));
            } else {
                return Error(std::format("field \"{}\" has unknown type", sFieldName));
            }
        }

        return pInstance;
    }
}; // namespace )

File_Serializable_GENERATED