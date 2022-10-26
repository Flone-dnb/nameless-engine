#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>
#include <optional>
#include <set>
#include <ranges>

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"
#include "io/ConfigManager.h"
#include "io/GuidProperty.h"
#include "misc/GC.hpp"
#include "io/IFieldSerializer.hpp"

// External.
#include "Refureku/Refureku.h"
#include "Refureku/Object.h"
#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include "toml11/toml.hpp"
#include "fmt/core.h"

#include "Serializable.generated.h"

namespace ne RNAMESPACE() {
    class Serializable;

    /** Information about an object to be serialized. */
    struct SerializableObjectInformation {
    public:
        SerializableObjectInformation() = delete;

        /**
         * Initialized object information for serialization.
         *
         * @param pObject          Object to serialize.
         * @param sObjectUniqueId  Object's unique ID. Don't use dots in IDs.
         * @param customAttributes Optional. Pairs of values to serialize with this object.
         */
        SerializableObjectInformation(
            Serializable* pObject,
            const std::string& sObjectUniqueId,
            const std::unordered_map<std::string, std::string>& customAttributes = {}) {
            this->pObject = pObject;
            this->sObjectUniqueId = sObjectUniqueId;
            this->customAttributes = customAttributes;
        }

        /** Object to serialize. */
        Serializable* pObject;

        /** Unique object ID. Don't use dots in it. */
        std::string sObjectUniqueId;

        /** Map of object attributes (custom information) that will be also serialized/deserialized. */
        std::unordered_map<std::string, std::string> customAttributes;
    };

    /** Information about an object that was deserialized. */
    struct DeserializedObjectInformation {
    public:
        DeserializedObjectInformation() = delete;

        /**
         * Initialized object information after deserialization.
         *
         * @param pObject          Deserialized object.
         * @param sObjectUniqueId  Object's unique ID.
         * @param customAttributes Object's custom attributes.
         */
        DeserializedObjectInformation(
            gc<Serializable> pObject,
            std::string sObjectUniqueId,
            std::unordered_map<std::string, std::string> customAttributes) {
            this->pObject = std::move(pObject);
            this->sObjectUniqueId = sObjectUniqueId;
            this->customAttributes = customAttributes;
        }

        /** Object to serialize. */
        gc<Serializable> pObject;

        /** Unique object ID. */
        std::string sObjectUniqueId;

        /** Map of object attributes (custom information) that were deserialized. */
        std::unordered_map<std::string, std::string> customAttributes;
    };

    /**
     * Base class for making a serializable type.
     *
     * Inherit your class from this type to add a 'serialize' function which will
     * serialize the type and all reflected fields (even inherited) into a file.
     */
    class RCLASS(Guid("f5a59b47-ead8-4da4-892e-cf05abb2f3cc")) Serializable : public rfk::Object {
    public:
        Serializable() = default;
        virtual ~Serializable() override = default;

        /**
         * Serializes the object and all reflected fields (including inherited) into a file.
         * Serialized object can later be deserialized using @ref deserialize.
         *
         * @param pathToFile       File to write reflected data to. The ".toml" extension will be added
         * automatically if not specified in the path. If the specified file already exists it will be
         * overwritten.
         * @param bEnableBackup    If 'true' will also use a backup (copy) file. @ref deserialize can use
         * backup file if the original file does not exist. Generally you want to use
         * a backup file if you are saving important information, such as player progress,
         * other cases such as player game settings and etc. usually do not need a backup but
         * you can use it if you want.
         * @param customAttributes Optional. Custom pairs of values that will be saved as this object's
         * additional information and could be later retrieved in @ref deserialize.
         *
         * @remark Note that not all reflected fields can be serialized, only specific types can be
         * serialized. Const fields, pointer fields, lvalue references, rvalue references and C-arrays will
         * always be ignored and will not be serialized (no error returned).
         * Supported for serialization types are:
         * - `bool`
         * - `int`
         * - `unsigned int`
         * - `long long`
         * - `unsigned long long`
         * - `float`
         * - `double`
         * - `std::string`
         * - `std::vector<T>` and `std::unordered_map<T, T>` (where T is any type from above)
         * - `T` (where T is any type that derives from Serializable)
         * Note that `std::vector<T>` and `std::unordered_map<T, T>` where T is any type derives from
         * Serializable is not supported.
         *
         * @remark You can mark reflected property as DontSerialize so it will be ignored in the serialization
         * process. Note that you don't need to mark fields of types that are always ignored (const, pointers,
         * etc.) because they will be ignored anyway. Example:
         * @code
         * RPROPERTY(DontSerialize)
         * int iKey = 42; // will be ignored and not serialized
         * @endcode
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field.
         */
        std::optional<Error> serialize(
            const std::filesystem::path& pathToFile,
            bool bEnableBackup,
            const std::unordered_map<std::string, std::string>& customAttributes = {});

        /**
         * Serializes multiple objects, their reflected fields (including inherited) and provided
         * custom attributes (if any) into a file.
         * Serialized objects can later be deserialized using @ref deserialize.
         *
         * This is an overloaded function. See full documentation for other overload.
         *
         * @param pathToFile    File to write reflected data to. The ".toml" extension will be added
         * automatically if not specified in the path. If the specified file already exists it will be
         * overwritten.
         * @param vObjects      Array of objects to serialize, their unique IDs
         * (so they could be differentiated in the file) and custom attributes (if any). Don't use
         * dots in the entity IDs, dots are used internally.
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
            std::vector<SerializableObjectInformation> vObjects,
            bool bEnableBackup);

        /**
         * Serializes the object and all reflected fields (including inherited) into a toml value.
         *
         * This is an overloaded function. See full documentation for other overload.
         *
         * @param tomlData          Toml value to append this object to.
         * @param sEntityId         Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated. Don't use
         * dots in the entity ID, dots are used in recursion when this function is called from this
         * function to process reflected field (sub entity).
         * @param customAttributes  Optional. Custom pairs of values that will be saved as this object's
         * additional information and could be later retrieved in @ref deserialize.
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field, otherwise name of the section that was used to store this entity.
         */
        std::variant<std::string, Error> serialize(
            toml::value& tomlData,
            std::string sEntityId = "",
            const std::unordered_map<std::string, std::string>& customAttributes = {});

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
         * Deserializes an object and all reflected fields (including inherited) from a file.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
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
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <typename T>
        requires std::derived_from<T, Serializable>
        static std::variant<gc<T>, Error> deserialize(const std::filesystem::path& pathToFile);

        /**
         * Deserializes an object and all reflected fields (including inherited) from a file.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         * @param customAttributes Pairs of values that were associated with this object.
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <typename T>
        requires std::derived_from<T, Serializable>
        static std::variant<gc<T>, Error> deserialize(
            const std::filesystem::path& pathToFile,
            std::unordered_map<std::string, std::string>& customAttributes);

        /**
         * Deserializes multiple objects and their reflected fields (including inherited) from a file.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         * @param ids        Array of object IDs (that you specified in @ref serialize) to deserialize
         * and return. You can use @ref getIdsFromFile to get IDs of all objects in the file.
         *
         * @return Error if something went wrong, otherwise an array of pointers to deserialized objects.
         */
        static std::variant<std::vector<DeserializedObjectInformation>, Error>
        deserialize(const std::filesystem::path& pathToFile, const std::set<std::string>& ids);

        /**
         * Adds a field serializer that will be automatically used in serialization/deserialization
         * to support specific field types. Use @ref getFieldSerializers to get array of added serializers.
         *
         * @remark If the serializer of the specified type was already added previously it will not be
         * added again.
         *
         * @param pFieldSerializer Field serializer to add.
         */
        static void addFieldSerializer(std::unique_ptr<IFieldSerializer> pFieldSerializer);

        /**
         * Returns available field serializers that will be automatically used in
         * serialization/deserialization.
         *
         * @return Array of available field serializers. Do not delete serializers, they are owned by the
         * Serializable object.
         */
        static std::vector<IFieldSerializer*> getFieldSerializers();

        /**
         * Deserializes an object and all reflected fields (including inherited) from a toml value.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @param tomlData         Toml value to retrieve an object from.
         * @param customAttributes Pairs of values that were associated with this object.
         * @param sEntityId        Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated.
         *
         * @warning Don't use dots in the entity ID, dots are used
         * in recursion when this function is called from this function to process reflected field (sub
         * entity).
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <typename T>
        requires std::derived_from<T, Serializable>
        static std::variant<gc<T>, Error> deserialize(
            const toml::value& tomlData,
            std::unordered_map<std::string, std::string>& customAttributes,
            std::string sEntityId = "");

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
         * Returns archetype for the specified GUID.
         *
         * @param sGuid GUID to look for.
         *
         * @return nullptr if not found, otherwise valid pointer.
         */
        static const rfk::Class* getClassForGuid(const std::string& sGuid);

        /**
         * Looks for all children of the specified archetype to find a type that
         * has the specified GUID.
         *
         * @param pArchetypeToAnalyze Type which children to analyze.
         * @param sGuid               GUID to look for.
         *
         * @return nullptr if not found, otherwise valid pointer.
         */
        static const rfk::Struct*
        getClassForGuid(const rfk::Struct* pArchetypeToAnalyze, const std::string& sGuid);

        /** Serializers used to serialize/deserialize fields. */
        static inline std::pair<std::mutex, std::vector<std::unique_ptr<IFieldSerializer>>>
            mtxFieldSerializers;

        /** Name of the key in which to store name of the field a section represents. */
        static inline const auto sSubEntityFieldNameKey = ".field_name";

        /** Name of the key which we use when there is nothing to serialize. */
        static inline const auto sNothingToSerializeKey = ".none";

        ne_Serializable_GENERATED
    };

    template <typename T>
    requires std::derived_from<T, Serializable> std::variant<gc<T>, Error> Serializable::deserialize(
        const std::filesystem::path& pathToFile) {
        std::unordered_map<std::string, std::string> foundCustomAttributes;
        return deserialize<T>(pathToFile, foundCustomAttributes);
    }

    template <typename T>
    requires std::derived_from<T, Serializable> std::variant<gc<T>, Error> Serializable::deserialize(
        const std::filesystem::path& pathToFile,
        std::unordered_map<std::string, std::string>& customAttributes) {
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
        auto result = deserialize<T>(tomlData, customAttributes);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        } else {
            return result;
        }
    }

    template <typename T>
    requires std::derived_from<T, Serializable> std::variant<gc<T>, Error> Serializable::deserialize(
        const toml::value& tomlData,
        std::unordered_map<std::string, std::string>& customAttributes,
        std::string sEntityId) {
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

        // Collect keys.
        const auto& sectionTable = section.as_table();
        std::vector<std::string> vKeys;
        for (const auto& [key, value] : sectionTable) {
            if (key == sNothingToSerializeKey) {
                continue;
            } else if (key.starts_with("..")) {
                // Custom attribute.
                if (!value.is_string()) {
                    return Error(fmt::format("found custom attribute \"{}\" is not a string", key));
                }
                customAttributes[key.substr(2)] = value.as_string().str;
            } else {
                vKeys.push_back(key);
            }
        }

        // Get archetype for found GUID.
        auto pType = getClassForGuid(sTypeGuid);
        if (!pType) {
            return Error(fmt::format("no type found for GUID {}", sTypeGuid));
        }
        if (!isDerivedFromSerializable(pType)) {
            return Error(fmt::format(
                "deserialized class with GUID {} does not derive from {}",
                sTypeGuid,
                staticGetArchetype().getName()));
        }

        // Create instance.
        gc<T> pGcInstance;
        {
            // this section is a temporary solution until we will add a `gc_new` method
            // to `rfk::Struct`
            std::unique_ptr<T> pInstance = pType->makeUniqueInstance<T>();
            if (!pInstance) {
                return Error(fmt::format(
                    "unable to make an object of type \"{}\" using type's default constructor "
                    "(does type \"{}\" has a default constructor?)",
                    pType->getName(),
                    pType->getName()));
            }
            gc<rfk::Object> pParentGcInstance = pInstance->gc_new();
            pGcInstance = gc_dynamic_pointer_cast<T>(pParentGcInstance);
            if (!pGcInstance) {
                return Error(fmt::format(
                    "dynamic cast failed to cast the type \"{}\" to the specified template argument "
                    "(are you trying to deserialize into a wrong type?)",
                    pParentGcInstance->getArchetype().getName()));
            }
        }

        const auto vFieldSerializers = getFieldSerializers();

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
                pType->getFieldByName(sFieldName.c_str(), rfk::EFieldFlags::Default, true);
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
            const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

            if (!isFieldSerializable(*pField))
                continue;

            // Deserialize field value.
            for (const auto& pSerializer : vFieldSerializers) {
                if (pSerializer->isFieldTypeSupported(pField)) {
                    auto optionalError = pSerializer->deserializeField(
                        &tomlData,
                        &value,
                        &*pGcInstance,
                        pField,
                        sTargetSection,
                        sEntityId,
                        customAttributes);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addEntry();
                        return error;
                    }
                }
            }
        }

        return pGcInstance;
    }
}; // namespace )

File_Serializable_GENERATED
