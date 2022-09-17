#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>
#include <set>
#include <ranges>

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"
#include "io/ConfigManager.h"
#include "io/GuidProperty.h"

// External.
#include "Refureku/Refureku.h"
#include "Refureku/Object.h"
#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include "toml11/toml.hpp"
#include "fmt/core.h"

#include "Serializable.generated.h"

namespace ne NENAMESPACE() {
    /**
     * Base class for making a serializable type.
     *
     * Inherit your class from this type to add a 'serialize' function which will
     * serialize the type and all reflected fields (even inherited) into a file.
     */
    class NECLASS(Guid("f5a59b47-ead8-4da4-892e-cf05abb2f3cc")) Serializable : public rfk::Object {
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
         * Serializes multiple objects and their reflected fields (including inherited) into a file.
         * Serialized entities can later be deserialized using @ref deserialize.
         *
         * This is an overloaded function. See full documentation for other overload.
         *
         * @param pathToFile    File to write reflected data to. The ".toml" extension will be added
         * automatically if not specified in the path. If the specified file already exists it will be
         * overwritten.
         * @param vObjects      Array of pairs of objects to serialize and their unique IDs
         * (so they could be differentiated in the file). Don't use
         * dots in the entity ID, dots are used internally.
         * @param bEnableBackup If 'true' will also use a backup (copy) file. @ref deserialize can use
         * backup file if the original file does not exist. Generally you want to use
         * a backup file if you are saving important information, such as player progress,
         * other cases such as player game settings and etc. usually do not need a backup but
         * you can use it if you want.
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field.
         */
        static std::optional<Error> serialize(
            const std::filesystem::path& pathToFile,
            std::vector<std::pair<Serializable*, std::string>> vObjects,
            bool bEnableBackup);

        /**
         * Analyzes the file for serialized objects, gathers and returns unique IDs of those objects.
         *
         * @param pathToFile File to read serialized data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, otherwise array of unique IDs of objects that exist
         * in the specified file.
         */
        static std::variant<std::set<std::string>, Error>
        getIdsFromFile(const std::filesystem::path& pathToFile);

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
         * auto pMyCoolNode = std::get<std::shared_ptr<ne::Node>>(std::move(result));
         * @endcode
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized entity.
         */
        template <typename T>
        requires std::derived_from<T, Serializable>
        static std::variant<std::shared_ptr<T>, Error> deserialize(const std::filesystem::path& pathToFile);

        /**
         * Deserializes multiple objects and their reflected fields (including inherited) from a file.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         * @param ids        Array of object IDs (that you specified in @ref serialize) to deserialize
         * and return. Don't use dots in the entity ID, dots are used internally.
         *
         * @return Error if something went wrong, otherwise an array of pointers to deserialized entities.
         */
        static std::variant<std::vector<std::shared_ptr<Serializable>>, Error>
        deserialize(const std::filesystem::path& pathToFile, std::set<std::string> ids);

#if defined(DEBUG)
        /**
         * Checks that all classes/structs that inherit from Serializable have correct and unique GUIDs.
         *
         * Automatically called by the Game object (object that owns GameInstance) and has no point in being
         * called from your game's code.
         */
        static void checkGuidUniqueness();
#endif

    private:
#if defined(DEBUG)
        /**
         * Collects GUIDs of children of the specified type.
         *
         * @param pArchetypeToAnalyze Type which children to analyze.
         * @param vAllGuids           Map of already collected GUIDs to check for uniqueness.
         */
        static void collectGuids(
            const rfk::Struct* pArchetypeToAnalyze, std::unordered_map<std::string, std::string>& vAllGuids);
#endif

        /**
         * Serializes the type and all reflected fields (including inherited) into a toml value.
         *
         * This is an overloaded function. See full documentation for other overload.
         *
         * @param tomlData        Toml value to append this object to.
         * @param sEntityId       Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated. Don't use
         * dots in the entity ID, dots are used in recursion when this function is called from this
         * function to process reflected field (sub entity).
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field, otherwise name of the section that was used to store this entity.
         */
        std::variant<std::string, Error> serialize(toml::value& tomlData, std::string sEntityId = "");

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
         * @return Error if something went wrong, otherwise a pointer to deserialized entity.
         */
        template <typename T>
        requires std::derived_from<T, Serializable>
        static std::variant<std::shared_ptr<T>, Error>
        deserialize(toml::value& tomlData, std::string sEntityId = "");

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
         * Returns archetype for the specified GUID.
         *
         * @param sGuid GUID to look for.
         *
         * @return nullptr if not found, otherwise valid pointer.
         */
        static const rfk::Class* getClassForGuid(const std::string& sGuid);

        /**
         * Looks for all childred of the specified archetype to find a type that
         * has the specified GUID.
         *
         * @param pArchetypeToAnalyze Type which children to analyze.
         * @param sGuid               GUID to look for.
         *
         * @return nullptr if not found, otherwise valid pointer.
         */
        static const rfk::Struct*
        getClassForGuid(const rfk::Struct* pArchetypeToAnalyze, const std::string& sGuid);

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

        /** Name of the key which we use when there is nothing to serialize. */
        static inline auto const sNothingToSerializeKey = ".none";

        ne_Serializable_GENERATED
    };

    template <typename T>
    requires std::derived_from<T, Serializable> std::variant<std::shared_ptr<T>, Error>
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
                fmt::format("failed to load file \"{}\", error: {}", fixedPath.string(), exception.what()));
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
    requires std::derived_from<T, Serializable> std::variant<std::shared_ptr<T>, Error>
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
            return Error("provided toml value has 0 sections while expected at least 1 section");
        }

        // Find a section that starts with the specified entity ID.
        // Look for a value between dots.
        // We can't just use sSectionName.starts_with(sEntityId) because we might make a mistake in the
        // following situation: [100.10.1014674670888563010] with entity ID equal to "10".
        std::string sTargetSection;
        std::string sTypeGuid;
        // Each entity section has the following format: [entityId.GUID]
        // For sub entities (field with reflected type) format: [parentEntityId.childEntityId.childGUID]
        for (const auto& sSectionName : vSections) {
            const auto iIdEndDotPos = sSectionName.rfind('.');
            if (iIdEndDotPos == std::string::npos) [[unlikely]] {
                return Error("provided toml value does not contain entity ID");
            }
            if (iIdEndDotPos + 1 == sSectionName.size()) [[unlikely]] {
                return Error(fmt::format("section name \"{}\" does not have a GUID", sSectionName));
            }
            if (iIdEndDotPos == 0) [[unlikely]] {
                return Error(fmt::format("section \"{}\" is not full", sSectionName));
            }

            // Get ID chain (either entity ID or something like "parentEntityId.childEntityId").
            const auto sIdChain = sSectionName.substr(0, iIdEndDotPos);
            if (sIdChain == sEntityId) {
                sTargetSection = sSectionName;

                // Get this entity's GUID.
                sTypeGuid = sSectionName.substr(iIdEndDotPos + 1);
                break;
            }
        }

        // Check if anything was found.
        if (sTargetSection.empty()) {
            return Error(fmt::format("could not find entity with ID \"{}\"", sEntityId));
        }

        // Get all keys (field names) from this section.
        toml::value section;
        try {
            section = toml::find(tomlData, sTargetSection);
        } catch (std::exception& ex) {
            return Error(fmt::format("no section \"{}\" was found ({})", sTargetSection, ex.what()));
        }

        if (!section.is_table()) {
            return Error(fmt::format("found \"{}\" section is not a section", sTargetSection));
        }

        const auto& sectionTable = section.as_table();
        std::vector<std::string> vKeys;
        for (const auto& [key, value] : sectionTable) {
            if (key == sNothingToSerializeKey)
                continue;
            vKeys.push_back(key);
        }

        // Get archetype for found GUID.
        auto pClass = getClassForGuid(sTypeGuid);
        if (!pClass) {
            return Error(fmt::format("no type found for GUID {}", sTypeGuid));
        }
        if (!isDerivedFromSerializable(pClass)) {
            return Error(fmt::format(
                "deserialized class with GUID {} does not derive from {}",
                sTypeGuid,
                staticGetArchetype().getName()));
        }

        // Create instance.
        auto pInstance = pClass->makeSharedInstance<T>();
        if (!pInstance) {
            return Error(fmt::format(
                "unable to make an object of type \"{}\" using type's default constructor "
                "(does type \"{}\" has a default constructor?)",
                pClass->getName(),
                pClass->getName()));
        }

        // Deserialize fields.
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
                return Error(fmt::format(
                    "field \"{}\" was not found in the specified toml value: {}",
                    sFieldName,
                    exception.what()));
            }

            // Get field by name.
            rfk::Field const* pField =
                pClass->getFieldByName(sFieldName.c_str(), rfk::EFieldFlags::Default, true);
            if (!pField) {
                Logger::get().warn(
                    fmt::format(
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
                    return Error(fmt::format(
                        "failed to convert string to double for field \"{}\": {}", sFieldName, ex.what()));
                }
            } else if (fieldType.match(rfk::getType<std::string>()) && value.is_string()) {
                auto fieldValue = value.as_string().str;
                pField->setUnsafe<std::string>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.getArchetype()) {
                // Field with a reflected type.
                // Find a section that has the key ".field_name = *our field name*".
                std::string sSectionNameForField;
                // This will have minimum value of 1 where the dot separates IDs from GUID.
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
                        return Error(fmt::format("section name \"{}\" is corrupted", sSectionName));
                    }
                    // Will be something like: "entityId.subEntityId", "entityId.subEntityId.subSubEntityId"
                    // or etc.
                    auto sEntityIdChain = sSectionName.substr(0, iLastDotPos);

                    // Remove last entity id from chain because we don't know it.
                    iLastDotPos = sEntityIdChain.rfind('.');
                    if (iLastDotPos == std::string::npos) {
                        return Error(fmt::format("section name \"{}\" is corrupted", sSectionName));
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
                            fmt::format("no section \"{}\" was found ({})", sTargetSection, ex.what()));
                    }

                    if (!section.is_table()) {
                        return Error(fmt::format("found \"{}\" section is not a section", sTargetSection));
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
                        return Error(fmt::format(
                            "found field name key \"{}\" is not a string", sSubEntityFieldNameKey));
                    }

                    if (fieldKey.as_string() == sFieldName) {
                        sSectionNameForField = sSectionName;
                        break;
                    }
                }

                if (sSectionNameForField.empty()) {
                    return Error(
                        fmt::format("could not find a section that represents field \"{}\"", sFieldName));
                }

                // Cut field's GUID from the section name.
                // The section name could look something like this: [entityId.subEntityId.subEntityGUID].
                const auto iSubEntityGuidDotPos = sSectionNameForField.rfind('.');
                if (iSubEntityGuidDotPos == std::string::npos) [[unlikely]] {
                    return Error(fmt::format(
                        "sub entity does not have a GUID (section: \"{}\")", sSectionNameForField));
                }
                if (iSubEntityGuidDotPos + 1 == sSectionNameForField.size()) [[unlikely]] {
                    return Error(
                        fmt::format("section name \"{}\" does not have a GUID", sSectionNameForField));
                }
                if (iSubEntityGuidDotPos == 0) [[unlikely]] {
                    return Error(fmt::format("section \"{}\" is not full", sSectionNameForField));
                }
                const auto sSubEntityId = sSectionNameForField.substr(0, iSubEntityGuidDotPos);

                // Deserialize section into an object.
                auto result = deserialize<Serializable>(tomlData, sSubEntityId);
                if (std::holds_alternative<Error>(result)) {
                    auto err = std::get<Error>(std::move(result));
                    err.addEntry();
                    return err;
                }
                auto pSubEntity = std::get<std::shared_ptr<Serializable>>(std::move(result));

                // Move object to field.
                cloneSerializableObject(
                    pSubEntity.get(), static_cast<Serializable*>(pField->getPtrUnsafe(pInstance.get())));
            } else {
                return Error(fmt::format("field \"{}\" has unknown type", sFieldName));
            }
        }

        return pInstance;
    }
}; // namespace )

File_Serializable_GENERATED
