#pragma once

// Standard.
#include <filesystem>
#include <memory>
#include <variant>
#include <optional>
#include <set>
#include <ranges>

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"
#include "misc/ProjectPaths.h"
#include "io/ConfigManager.h"
#include "io/properties/GuidProperty.h"
#include "misc/GC.hpp"
#include "io/serializers/IFieldSerializer.hpp"
#include "io/properties/SerializeProperty.h"
#include "io/properties/SerializeAsExternalProperty.h"
#include "io/serializers/SerializableObjectFieldSerializer.h"

// External.
#include "Refureku/Refureku.h"
#include "Refureku/Object.h"
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
         * @param pOriginalObject  Optional. Use if the object was previously deserialized and
         * you now want to only serialize changed fields of this object and additionally store
         * the path to the original file (to deserialize unchanged fields).
         */
        SerializableObjectInformation(
            Serializable* pObject,
            const std::string& sObjectUniqueId,
            const std::unordered_map<std::string, std::string>& customAttributes = {},
            Serializable* pOriginalObject = nullptr) {
            this->pObject = pObject;
            this->sObjectUniqueId = sObjectUniqueId;
            this->customAttributes = customAttributes;
            this->pOriginalObject = pOriginalObject;
        }

        /** Object to serialize. */
        Serializable* pObject = nullptr;

        /**
         * Use if @ref pObject was previously deserialized and you now want to only serialize
         * changed fields of this object and additionally store the path to the original file
         * (to deserialize unchanged fields).
         */
        Serializable* pOriginalObject = nullptr;

        /** Unique object ID. Don't use dots in it. */
        std::string sObjectUniqueId;

        /** Map of object attributes (custom information) that will be also serialized/deserialized. */
        std::unordered_map<std::string, std::string> customAttributes;
    };

    /** Information about an object that was deserialized. */
    template <template <typename> class SmartPointer>
        requires std::same_as<SmartPointer<Serializable>, std::shared_ptr<Serializable>> ||
                 std::same_as<SmartPointer<Serializable>, gc<Serializable>>
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
            SmartPointer<Serializable> pObject,
            std::string sObjectUniqueId,
            std::unordered_map<std::string, std::string> customAttributes) {
            this->pObject = std::move(pObject);
            this->sObjectUniqueId = sObjectUniqueId;
            this->customAttributes = customAttributes;
        }

        /** Object to serialize. */
        SmartPointer<Serializable> pObject;

        /** Unique object ID. */
        std::string sObjectUniqueId;

        /** Map of object attributes (custom information) that were deserialized. */
        std::unordered_map<std::string, std::string> customAttributes;
    };

    /**
     * Adds support for serialization/deserialization for your reflected type.
     *
     * Inherit your class/struct from this class to add functions which will
     * serialize the type and all reflected fields (even inherited).
     */
    class RCLASS(Guid("f5a59b47-ead8-4da4-892e-cf05abb2f3cc")) Serializable : public rfk::Object {
    public:
        Serializable() = default;
        virtual ~Serializable() override = default;

        /**
         * Serializes the object and all reflected fields (including inherited) that
         * are marked with `ne::Serialize` property into a file.
         * Serialized object can later be deserialized using @ref deserialize.
         *
         * @param pathToFile       File to write reflected data to. The ".toml" extension will be added
         * automatically if not specified in the path. If the specified file already exists it will be
         * overwritten. If the directories of the specified file do not exist they will be recursively
         * created.
         * @param bEnableBackup    If 'true' will also use a backup (copy) file. @ref deserialize can use
         * backup file if the original file does not exist. Generally you want to use
         * a backup file if you are saving important information, such as player progress,
         * other cases such as player game settings and etc. usually do not need a backup but
         * you can use it if you want.
         * @param customAttributes Optional. Custom pairs of values that will be saved as this object's
         * additional information and could be later retrieved in @ref deserialize.
         *
         * @remark If file's parent directories do no exist they will be created.
         *
         * @remark In order for a field to be serialized with the object, you need to mark it with
         * `ne::Serialize` property like so:
         * @code
         * using namespace ne;
         * RPROPERTY(Serialize)
         * int iMyValue = 0;
         * @endcode
         * Note that not all reflected fields can be serialized, only specific types can be
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
         * - and more, see `io/serializers` directory for available field serializers.
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field.
         */
        [[nodiscard]] std::optional<Error> serialize(
            std::filesystem::path pathToFile,
            bool bEnableBackup,
            const std::unordered_map<std::string, std::string>& customAttributes = {});

        /**
         * Serializes multiple objects, their reflected fields (including inherited) and provided
         * custom attributes (if any) into a file.
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
        [[nodiscard]] static std::optional<Error> serializeMultiple(
            std::filesystem::path pathToFile,
            std::vector<SerializableObjectInformation> vObjects,
            bool bEnableBackup);

        /**
         * Serializes the object and all reflected fields (including inherited) into a toml value.
         *
         * @remark This is an overloaded function. See full documentation for other overload.
         *
         * @param tomlData           Toml value to append this object to.
         * @param sEntityId          Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated. Don't use
         * dots in the entity ID, dots are used in recursion when this function is called from this
         * function to process reflected field (sub entity).
         * @param customAttributes   Optional. Custom pairs of values that will be saved as this object's
         * additional information and could be later retrieved in @ref deserialize.
         * @param optionalPathToFile Optional. Path to the file that this TOML data will be serialized.
         * Used for fields marked as `SerializeAsExternal`.
         * @param bEnableBackup      Optional. If this TOML data will be serialized to file whether
         * the backup file is needed or not. Used for fields marked as `SerializeAsExternal`.
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field, otherwise name of the section that was used to store this entity.
         */
        [[nodiscard]] std::variant<std::string, Error> serialize(
            toml::value& tomlData,
            const std::string& sEntityId = "",
            const std::unordered_map<std::string, std::string>& customAttributes = {},
            const std::optional<std::filesystem::path>& optionalPathToFile = {},
            bool bEnableBackup = false);

        /**
         * Serializes the object and all reflected fields (including inherited) into a toml value.
         *
         * @remark This is an overloaded function that takes an original object to serialize only changed
         * values. See full documentation for other overload.
         *
         * @param tomlData           Toml value to append this object to.
         * @param pOriginalObject    Optional. Original object of the same type as the object being
         * serialized, this object is a deserialized version of the object being serialized,
         * used to compare serializable fields' values and only serialize changed values.
         * @param sEntityId          Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated. Don't use
         * dots in the entity ID, dots are used in recursion when this function is called from this
         * function to process reflected field (sub entity).
         * @param customAttributes   Optional. Custom pairs of values that will be saved as this object's
         * additional information and could be later retrieved in @ref deserialize.
         * @param optionalPathToFile Optional. Path to the file that this TOML data will be serialized.
         * Used for fields marked as `SerializeAsExternal`.
         * @param bEnableBackup      Optional. If this TOML data will be serialized to file whether
         * the backup file is needed or not. Used for fields marked as `SerializeAsExternal`.
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field, otherwise name of the section that was used to store this entity.
         */
        [[nodiscard]] std::variant<std::string, Error> serialize(
            toml::value& tomlData,
            Serializable* pOriginalObject,
            std::string sEntityId = "",
            const std::unordered_map<std::string, std::string>& customAttributes = {},
            const std::optional<std::filesystem::path>& optionalPathToFile = {},
            bool bEnableBackup = false);

        /**
         * Analyzes the file for serialized objects, gathers and returns unique IDs of those objects.
         *
         * @param pathToFile File to read serialized data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, otherwise array of unique IDs of objects that exist
         * in the specified file.
         */
        static std::variant<std::set<std::string>, Error> getIdsFromFile(std::filesystem::path pathToFile);

        /**
         * Deserializes an object and all reflected fields (including inherited) from a file.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @remark You can use either `gc` or `std::shared_ptr` as a smart pointer for deserialized object.
         *
         * Example:
         * @code
         * const auto pMyCoolNode = gc_new<ne::Node>("My Cool Node");
         * // ... do some changes to the node ...
         * auto optionalError = pMyCoolNode->serialize(pathToFile, false);
         * // ...
         * auto result = Serializable::deserialize<gc, ne::Node>(pathToFile);
         * if (std::holds_alternative<ne::Error>(result)){
         *     // process error here
         * }
         * auto pMyCoolDeserializedNode = std::get<gc<ne::Node>>(std::move(result));
         * @endcode
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <template <typename> class SmartPointer, typename T>
            requires std::derived_from<T, Serializable> &&
                     (std::same_as<SmartPointer<T>, std::shared_ptr<T>> ||
                      std::same_as<SmartPointer<T>, gc<T>>)
        static std::variant<SmartPointer<T>, Error> deserialize(const std::filesystem::path& pathToFile);

        /**
         * Deserializes an object and all reflected fields (including inherited) from a file.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @remark This is an overloaded function, see a more detailed documentation for the other overload.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         * @param customAttributes Pairs of values that were associated with this object.
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <template <typename> class SmartPointer, typename T>
            requires std::derived_from<T, Serializable> &&
                     (std::same_as<SmartPointer<T>, std::shared_ptr<T>> ||
                      std::same_as<SmartPointer<T>, gc<T>>)
        static std::variant<SmartPointer<T>, Error> deserialize(
            const std::filesystem::path& pathToFile,
            std::unordered_map<std::string, std::string>& customAttributes);

        /**
         * Deserializes an object and all reflected fields (including inherited) from a file.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @remark This is an overloaded function, see a more detailed documentation for the other overload.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         * @param customAttributes Pairs of values that were associated with this object.
         * @param sEntityId        Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated.
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <template <typename> class SmartPointer, typename T>
            requires std::derived_from<T, Serializable> &&
                     (std::same_as<SmartPointer<T>, std::shared_ptr<T>> ||
                      std::same_as<SmartPointer<T>, gc<T>>)
        static std::variant<SmartPointer<T>, Error> deserialize(
            std::filesystem::path pathToFile,
            std::unordered_map<std::string, std::string>& customAttributes,
            const std::string& sEntityId);

        /**
         * Deserializes an object and all reflected fields (including inherited) from a file.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @remark This is an overloaded function, see a more detailed documentation for the other overload.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         * @param sEntityId        Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated.
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <template <typename> class SmartPointer, typename T>
            requires std::derived_from<T, Serializable> &&
                     (std::same_as<SmartPointer<T>, std::shared_ptr<T>> ||
                      std::same_as<SmartPointer<T>, gc<T>>)
        static std::variant<SmartPointer<T>, Error> deserialize(
            const std::filesystem::path& pathToFile, const std::string& sEntityId);

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
        template <template <typename> class SmartPointer>
            requires std::same_as<SmartPointer<Serializable>, std::shared_ptr<Serializable>> ||
                     std::same_as<SmartPointer<Serializable>, gc<Serializable>>
        static std::variant<std::vector<DeserializedObjectInformation<SmartPointer>>, Error>
        deserializeMultiple(const std::filesystem::path& pathToFile, const std::set<std::string>& ids);

        /**
         * Deserializes an object and all reflected fields (including inherited) from a toml value.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @remark This is an overloaded function, see a more detailed documentation for the other overload.
         *
         * @param tomlData           Toml value to retrieve an object from.
         * @param customAttributes   Pairs of values that were associated with this object.
         * @param sEntityId          Unique ID of this object. When serializing multiple objects into
         * one toml value provide different IDs for each object so they could be differentiated.
         * @param optionalPathToFile Optional. Path to the file that this TOML data is deserialized from.
         * Used for fields marked as `SerializeAsExternal`
         *
         * @warning Don't use dots in the entity ID, dots are used
         * in recursion when this function is called from this function to process reflected field (sub
         * entity).
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <template <typename> class SmartPointer, typename T>
            requires std::derived_from<T, Serializable> &&
                     (std::same_as<SmartPointer<T>, std::shared_ptr<T>> ||
                      std::same_as<SmartPointer<T>, gc<T>>)
        static std::variant<SmartPointer<T>, Error> deserialize(
            const toml::value& tomlData,
            std::unordered_map<std::string, std::string>& customAttributes,
            std::string sEntityId = "",
            std::optional<std::filesystem::path> optionalPathToFile = {});

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
         * If this object was deserialized from a file that is located in the `res` directory
         * of this project, returns a pair of values:
         * - path to this file relative to the `res` directory,
         * - unique ID of this object in this file.
         *
         * This path will never point to a backup file and will always point to the original file
         * (even if the backup file was used in deserialization).
         *
         * Example: say this object is deserialized from the file located at `.../res/game/test.toml`,
         * this value will be equal to the following pair: {`game/test.toml`, `some.id`}.
         *
         * @return Empty if this object was not deserialized previously, otherwise path to the file
         * that was used in deserialization relative to the `res` directory.
         */
        std::optional<std::pair<std::string, std::string>> getPathDeserializedFromRelativeToRes() const;

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

    protected:
        // This field serializer will call `onAfterDeserialized` after deserialization.
        friend class SerializableObjectFieldSerializer;

        /**
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() {}

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

        /**
         * If this object was deserialized from a file that is located in the `res` directory
         * of this project, this field will contain a pair of values:
         * - path to this file relative to the `res` directory,
         * - unique ID of this object in this file.
         *
         * This path will never point to a backup file and will always point to the original file
         * (even if the backup file was used in deserialization).
         *
         * Example: say this object is deserialized from the file located at `.../res/game/test.toml`,
         * this value will be equal to `game/test.toml`.
         */
        std::optional<std::pair<std::string, std::string>> pathDeserializedFromRelativeToRes;

        /** Serializers used to serialize/deserialize fields. */
        static inline std::pair<std::mutex, std::vector<std::unique_ptr<IFieldSerializer>>>
            mtxFieldSerializers;

        /** Name of the key in which to store name of the field a section represents. */
        static inline const auto sSubEntityFieldNameKey = ".field_name";

        /**
         * Name of the key which we use when we serialize an object that was previously
         * deserialized from the `res` directory.
         */
        static inline const auto sPathRelativeToResKey = ".path_relative_to_res";

        /** Name of the key which we use when there is nothing to serialize. */
        static inline const auto sNothingToSerializeKey = ".none";

        ne_Serializable_GENERATED
    };

    template <template <typename> class SmartPointer, typename T>
        requires std::derived_from<T, Serializable> &&
                 (std::same_as<SmartPointer<T>, std::shared_ptr<T>> || std::same_as<SmartPointer<T>, gc<T>>)
    std::variant<SmartPointer<T>, Error> Serializable::deserialize(const std::filesystem::path& pathToFile) {
        std::unordered_map<std::string, std::string> foundCustomAttributes;
        return deserialize<SmartPointer, T>(pathToFile, foundCustomAttributes);
    }

    template <template <typename> class SmartPointer, typename T>
        requires std::derived_from<T, Serializable> &&
                 (std::same_as<SmartPointer<T>, std::shared_ptr<T>> || std::same_as<SmartPointer<T>, gc<T>>)
    std::variant<SmartPointer<T>, Error> Serializable::deserialize(
        const std::filesystem::path& pathToFile,
        std::unordered_map<std::string, std::string>& customAttributes) {
        return deserialize<SmartPointer, T>(pathToFile, customAttributes, "");
    }

    template <template <typename> class SmartPointer, typename T>
        requires std::derived_from<T, Serializable> &&
                 (std::same_as<SmartPointer<T>, std::shared_ptr<T>> || std::same_as<SmartPointer<T>, gc<T>>)
    std::variant<SmartPointer<T>, Error> Serializable::deserialize(
        const std::filesystem::path& pathToFile, const std::string& sEntityId) {
        std::unordered_map<std::string, std::string> foundCustomAttributes;
        return deserialize<SmartPointer, T>(pathToFile, foundCustomAttributes, sEntityId);
    }

    template <template <typename> class SmartPointer, typename T>
        requires std::derived_from<T, Serializable> &&
                 (std::same_as<SmartPointer<T>, std::shared_ptr<T>> || std::same_as<SmartPointer<T>, gc<T>>)
    std::variant<SmartPointer<T>, Error> Serializable::deserialize(
        const toml::value& tomlData,
        std::unordered_map<std::string, std::string>& customAttributes,
        std::string sEntityId,
        std::optional<std::filesystem::path> optionalPathToFile) {
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
        SmartPointer<T> pOriginalEntity = nullptr;
        for (const auto& [key, value] : sectionTable) {
            if (key == sNothingToSerializeKey) {
                continue;
            } else if (key == sPathRelativeToResKey) {
                // Make sure the value is string.
                if (!value.is_string()) {
                    Error error(fmt::format("found \"{}\" key's value is not string", sPathRelativeToResKey));
                    return error;
                }

                // Deserialize original entity.
                const auto deserializeResult = Serializable::deserialize<SmartPointer, T>(
                    ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / value.as_string().str);
                if (std::holds_alternative<Error>(deserializeResult)) {
                    auto err = std::get<Error>(deserializeResult);
                    err.addEntry();
                    return err;
                }
                pOriginalEntity = std::get<SmartPointer<T>>(deserializeResult);
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
            if (pOriginalEntity) {
                return Error(fmt::format(
                    "GUID \"{}\" was not found in the database, but "
                    "the original object at \"{}\" (ID \"{}\") was deserialized",
                    sTypeGuid,
                    pOriginalEntity->getPathDeserializedFromRelativeToRes().value().first,
                    pOriginalEntity->getPathDeserializedFromRelativeToRes().value().second));
            } else {
                return Error(fmt::format("no type found for GUID \"{}\"", sTypeGuid));
            }
        }
        if (!isDerivedFromSerializable(pType)) {
            if (pOriginalEntity) {
                return Error(fmt::format(
                    "deserialized type for \"{}\" does not derive from {}, but "
                    "the original object at \"{}\" (ID \"{}\") was deserialized",
                    sTypeGuid,
                    staticGetArchetype().getName(),
                    pOriginalEntity->getPathDeserializedFromRelativeToRes().value().first,
                    pOriginalEntity->getPathDeserializedFromRelativeToRes().value().second));
            } else {
                return Error(fmt::format(
                    "deserialized type with GUID \"{}\" does not derive from {}",
                    sTypeGuid,
                    staticGetArchetype().getName()));
            }
        }

        // Create instance.
        SmartPointer<T> pSmartPointerInstance = nullptr;
        if (pOriginalEntity != nullptr) {
            // Use the original entity instead of creating a new one.
            pSmartPointerInstance = std::move(pOriginalEntity);
        }
        if (pSmartPointerInstance == nullptr) {
            if constexpr (std::is_same_v<SmartPointer<T>, std::shared_ptr<T>>) {
                pSmartPointerInstance = pType->makeSharedInstance<T>();
                if (pSmartPointerInstance == nullptr) {
                    return Error(fmt::format(
                        "unable to make an object of type \"{}\" using type's default constructor "
                        "(does type \"{}\" has a default constructor?)",
                        pType->getName(),
                        pType->getName()));
                }
            } else if (std::is_same_v<SmartPointer<T>, gc<T>>) {
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
                pSmartPointerInstance = gc_dynamic_pointer_cast<T>(pParentGcInstance);
                if (!pSmartPointerInstance) {
                    return Error(fmt::format(
                        "dynamic cast failed to cast the type \"{}\" to the specified template argument "
                        "(are you trying to deserialize into a wrong type?)",
                        pParentGcInstance->getArchetype().getName()));
                }
            } else {
                return Error("unexpected smart pointer type received");
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
                        "actual object (if you removed/renamed this reflected field from your "
                        "class/struct - ignore this warning)",
                        sFieldName),
                    "");
                continue;
            }
            const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

            if (!isFieldSerializable(*pField))
                continue;

            // Check if we need to deserialize from external file.
            if (pField->getProperty<SerializeAsExternal>()) {
                // Make sure this field derives from `Serializable`.
                if (!Serializable::isDerivedFromSerializable(pField->getType().getArchetype())) [[unlikely]] {
                    // Show an error so that the developer will instantly see the mistake.
                    auto error = Error("only fields of type derived from `Serializable` can use "
                                       "`SerializeAsExternal` property");
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }

                // Make sure path to the main file is specified.
                if (!optionalPathToFile.has_value()) [[unlikely]] {
                    return Error("unable to deserialize field marked as `SerializeAsExternal` "
                                 "because path to the main file was not specified");
                }

                // Get entity ID chain.
                auto iLastDotPos = sTargetSection.rfind('.');
                if (iLastDotPos == std::string::npos) [[unlikely]] {
                    return Error(fmt::format("section name \"{}\" is corrupted", sTargetSection));
                }
                // Will be something like: "entityId.subEntityId",
                // "entityId.subEntityId.subSubEntityId" or etc.
                const auto sEntityIdChain = sTargetSection.substr(0, iLastDotPos);

                // Prepare path to the external file.
                const auto sExternalFileName = fmt::format(
                    "{}.{}.{}{}",
                    optionalPathToFile->stem().string(),
                    sEntityIdChain,
                    pField->getName(),
                    ConfigManager::getConfigFormatExtension());
                const auto pathToExternalFile = optionalPathToFile.value().parent_path() / sExternalFileName;

                // Deserialize external file.
                auto result = deserialize<std::shared_ptr, Serializable>(pathToExternalFile);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addEntry();
                    return error;
                }
                const auto pDeserializedExternalField =
                    std::get<std::shared_ptr<Serializable>>(std::move(result));

                // Clone deserialized data to field.
                Serializable* pFieldObject =
                    reinterpret_cast<Serializable*>(pField->getPtrUnsafe(&*pSmartPointerInstance));
                auto optionalError = SerializableObjectFieldSerializer::cloneSerializableObject(
                    pDeserializedExternalField.get(), pFieldObject, false);
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addEntry();
                    return error;
                }

                continue;
            }

            // Deserialize field value.
            bool bFoundSerializer = false;
            for (const auto& pSerializer : vFieldSerializers) {
                if (pSerializer->isFieldTypeSupported(pField)) {
                    bFoundSerializer = true;
                    auto optionalError = pSerializer->deserializeField(
                        &tomlData,
                        &value,
                        &*pSmartPointerInstance,
                        pField,
                        sTargetSection,
                        sEntityId,
                        customAttributes);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addEntry();
                        if (pOriginalEntity) {
                            Logger::get().error(
                                fmt::format(
                                    "an error occurred while deserializing "
                                    "changed field (this field was not deserialized), error: {}",
                                    error.getFullErrorMessage()),
                                "");
                        } else {
                            return error;
                        }
                    }
                }
            }

            if (!bFoundSerializer) {
                Logger::get().warn(
                    fmt::format("unable to find a deserializer that supports field \"{}\"", sFieldName), "");
            }
        }

        // Notify.
        Serializable* pTarget = dynamic_cast<Serializable*>(&*pSmartPointerInstance);
        pTarget->onAfterDeserialized();

        return pSmartPointerInstance;
    }

    template <template <typename> class SmartPointer, typename T>
        requires std::derived_from<T, Serializable> &&
                 (std::same_as<SmartPointer<T>, std::shared_ptr<T>> || std::same_as<SmartPointer<T>, gc<T>>)
    std::variant<SmartPointer<T>, Error> Serializable::deserialize(
        std::filesystem::path pathToFile,
        std::unordered_map<std::string, std::string>& customAttributes,
        const std::string& sEntityId) {
        // Add TOML extension to file.
        if (!pathToFile.string().ends_with(".toml")) {
            pathToFile += ".toml";
        }

        std::filesystem::path backupFile = pathToFile;
        backupFile += ConfigManager::getBackupFileExtension();

        if (!std::filesystem::exists(pathToFile)) {
            // Check if a backup file exists.
            if (std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(backupFile, pathToFile);
            } else {
                return Error("requested file or a backup file do not exist");
            }
        }

        // Load file.
        toml::value tomlData;
        try {
            tomlData = toml::parse(pathToFile);
        } catch (std::exception& exception) {
            return Error(fmt::format(
                "failed to parse TOML file at \"{}\", error: {}", pathToFile.string(), exception.what()));
        }

        // Deserialize.
        auto result = deserialize<SmartPointer, T>(tomlData, customAttributes, sEntityId, pathToFile);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        } else if (pathToFile.string().starts_with(
                       ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT).string())) {
            // File is located in the `res` directory, save a relative path to the `res` directory.
            auto sRelativePath =
                std::filesystem::relative(
                    pathToFile, ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT))
                    .string();

            // Replace all '\' characters with '/' just to be consistent.
            std::replace(sRelativePath.begin(), sRelativePath.end(), '\\', '/');

            // Remove the forward slash at the beginning (if exists).
            if (sRelativePath.starts_with('/')) {
                sRelativePath = sRelativePath.substr(1);
            }

            // Double check that everything is correct.
            const auto pathToOriginalFile =
                ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / sRelativePath;
            if (!std::filesystem::exists(pathToOriginalFile)) {
                return Error(fmt::format(
                    "failed to save the relative path to the `res` directory for the file at \"{}\", "
                    "reason: constructed path \"{}\" does not exist",
                    pathToFile.string(),
                    pathToOriginalFile.string()));
            }

            SmartPointer<Serializable> pDeserialized = std::get<SmartPointer<T>>(result);
            pDeserialized->pathDeserializedFromRelativeToRes = {sRelativePath, sEntityId};
        }

        return result;
    }

    template <template <typename> class SmartPointer>
        requires std::same_as<SmartPointer<Serializable>, std::shared_ptr<Serializable>> ||
                 std::same_as<SmartPointer<Serializable>, gc<Serializable>>
    std::variant<std::vector<DeserializedObjectInformation<SmartPointer>>, Error>
    Serializable::deserializeMultiple(
        const std::filesystem::path& pathToFile, const std::set<std::string>& ids) {
        // Check that specified IDs don't have dots in them.
        for (const auto& sId : ids) {
            if (sId.contains('.')) [[unlikely]] {
                return Error(
                    fmt::format("the specified object ID \"{}\" is not allowed to have dots in it", sId));
            }
        }

        // Deserialize.
        std::vector<DeserializedObjectInformation<SmartPointer>> deserializedObjects;
        for (const auto& sId : ids) {
            std::unordered_map<std::string, std::string> customAttributes;
            auto result = deserialize<SmartPointer, Serializable>(pathToFile, customAttributes, sId);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            const auto pObject = std::get<SmartPointer<Serializable>>(std::move(result));
            DeserializedObjectInformation objectInfo(pObject, sId, customAttributes);
            deserializedObjects.push_back(std::move(objectInfo));
        }

        return deserializedObjects;
    }
}; // namespace ne RNAMESPACE()

File_Serializable_GENERATED
