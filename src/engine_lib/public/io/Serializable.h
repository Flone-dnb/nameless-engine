#pragma once

// Standard.
#include <filesystem>
#include <memory>
#include <variant>
#include <optional>
#include <set>
#include <format>

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"
#include "misc/ProjectPaths.h"
#include "io/ConfigManager.h"
#include "io/properties/GuidProperty.h"
#include "io/properties/SerializeProperty.h"
#include "io/serializers/SerializableObjectFieldSerializer.h"
#include "io/FieldSerializerManager.h"

// External.
#include "Refureku/Object.h"
#include "GcPtr.h"

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
    template <typename SmartPointer, typename InnerType = typename SmartPointer::element_type>
        requires std::same_as<SmartPointer, sgc::GcPtr<Serializable>> ||
                 std::same_as<SmartPointer, std::unique_ptr<Serializable>>
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
            SmartPointer pObject,
            std::string sObjectUniqueId,
            std::unordered_map<std::string, std::string> customAttributes) {
            this->pObject = std::move(pObject);
            this->sObjectUniqueId = sObjectUniqueId;
            this->customAttributes = customAttributes;
        }

        /** Object to serialize. */
        SmartPointer pObject;

        /** Unique object ID. */
        std::string sObjectUniqueId;

        /** Map of object attributes (custom information) that were deserialized. */
        std::unordered_map<std::string, std::string> customAttributes;
    };

    /**
     * Adds support for serialization/deserialization for your reflected type.
     *
     * Inherit your class/struct from this class to add functions which will
     * serialize the type and reflected fields (even inherited) that are marked with special properties.
     */
    class RCLASS(Guid("f5a59b47-ead8-4da4-892e-cf05abb2f3cc")) Serializable : public rfk::Object {
        // This field serializer will call `onAfterDeserialized` after deserialization.
        friend class SerializableObjectFieldSerializer;

    public:
        Serializable() = default;
        virtual ~Serializable() override = default;

        /**
         * Analyzes the file for serialized objects, gathers and returns unique IDs of those objects.
         *
         * @param pathToFile File to read serialized data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, otherwise array of unique IDs of objects that exist
         * in the specified file and parsed TOML data that you can reuse.
         */
        static std::variant<std::pair<std::set<std::string>, toml::value>, Error>
        getIdsFromFile(std::filesystem::path pathToFile);

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
         * - `T` (where `T` is any type that derives from Serializable)
         * - and more, see `io/serializers` directory for available field serializers
         * (you don't need to use them directly, they will be automatically picked inside
         * of this function).
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
         * Used for fields marked as `Serialize(AsExternal)`.
         * @param bEnableBackup      Optional. If this TOML data will be serialized to file whether
         * the backup file is needed or not. Used for fields marked as `Serialize(AsExternal)`.
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
         * Used for fields marked as `Serialize(AsExternal)`.
         * @param bEnableBackup      Optional. If this TOML data will be serialized to file whether
         * the backup file is needed or not. Used for fields marked as `Serialize(AsExternal)`.
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
         * Deserializes an object and all reflected fields (including inherited) from a file.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @remark You can use either `sgc::GcPtr` or `std::unique_ptr` as a smart pointer for deserialized
         * object.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <typename SmartPointer, typename InnerType = typename SmartPointer::element_type>
            requires std::derived_from<InnerType, Serializable> &&
                     (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                      std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
        static std::variant<SmartPointer, Error> deserialize(const std::filesystem::path& pathToFile);

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
        template <typename SmartPointer, typename InnerType = typename SmartPointer::element_type>
            requires std::derived_from<InnerType, Serializable> &&
                     (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                      std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
        static std::variant<SmartPointer, Error> deserialize(
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
        template <typename SmartPointer, typename InnerType = typename SmartPointer::element_type>
            requires std::derived_from<InnerType, Serializable> &&
                     (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                      std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
        static std::variant<SmartPointer, Error> deserialize(
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
        template <typename SmartPointer, typename InnerType = typename SmartPointer::element_type>
            requires std::derived_from<InnerType, Serializable> &&
                     (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                      std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
        static std::variant<SmartPointer, Error> deserialize(
            const std::filesystem::path& pathToFile, const std::string& sEntityId);

        /**
         * Deserializes multiple objects and their reflected fields (including inherited) from a file.
         *
         * @param pathToFile File to read reflected data from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, otherwise an array of pointers to deserialized objects.
         */
        template <typename SmartPointer, typename InnerType = typename SmartPointer::element_type>
            requires std::same_as<SmartPointer, sgc::GcPtr<Serializable>> ||
                     std::same_as<SmartPointer, std::unique_ptr<Serializable>>
        static std::variant<std::vector<DeserializedObjectInformation<SmartPointer>>, Error>
        deserializeMultiple(std::filesystem::path pathToFile);

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
         * Used for fields marked as `Serialize(AsExternal)`
         *
         * @warning Don't use dots in the entity ID, dots are used
         * in recursion when this function is called from this function to process reflected field (sub
         * entity).
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <typename SmartPointer, typename InnerType = typename SmartPointer::element_type>
            requires std::derived_from<InnerType, Serializable> &&
                     (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                      std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
        static std::variant<SmartPointer, Error> deserialize(
            const toml::value& tomlData,
            std::unordered_map<std::string, std::string>& customAttributes,
            std::string sEntityId = "",
            const std::optional<std::filesystem::path>& optionalPathToFile = {});

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

    protected:
        /**
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() {}

    private:
        /**
         * Adds ".toml" extension to the path (if needed) and copies a backup file to the specified path
         * if the specified path does not exist but there is a backup file.
         *
         * @param pathToFile Path to toml file (may point to non-existing path or don't have ".toml"
         * extension).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> resolvePathToToml(std::filesystem::path& pathToFile);

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
         * Deserializes an object and all reflected fields (including inherited) from a file.
         * Specify the type of an object (that is located in the file) as the T template parameter, which
         * can be entity's actual type or entity's parent (up to Serializable).
         *
         * @remark This is an overloaded function, see a more detailed documentation for the other overload.
         *
         * @param tomlData           Toml value to retrieve an object from.
         * @param customAttributes   Pairs of values that were associated with this object.
         * @param sSectionName       Name of the TOML section to deserialize.
         * @param sTypeGuid          GUID of the type to deserialize (taken from section name).
         * @param sEntityId          Entity ID chain string.
         * @param optionalPathToFile Optional. Path to the file that this TOML data is deserialized from.
         * Used for fields marked as `Serialize(AsExternal)`
         *
         * @return Error if something went wrong, otherwise a pointer to deserialized object.
         */
        template <typename SmartPointer, typename InnerType = typename SmartPointer::element_type>
            requires std::derived_from<InnerType, Serializable> &&
                     (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                      std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
        static std::variant<SmartPointer, Error> deserializeFromSection(
            const toml::value& tomlData,
            std::unordered_map<std::string, std::string>& customAttributes,
            const std::string& sSectionName,
            const std::string& sTypeGuid,
            const std::string& sEntityId,
            const std::optional<std::filesystem::path>& optionalPathToFile);

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

        /** Name of the key in which to store name of the field a section represents. */
        static inline const auto sSubEntityFieldNameKey = ".field_name";

        /**
         * Name of the key which we use when we serialize an object that was previously
         * deserialized from the `res` directory.
         */
        static inline const auto sPathRelativeToResKey = ".path_relative_to_res";

        /** Name of the key which we use when there is nothing to serialize. */
        static inline const auto sNothingToSerializeKey = ".none";

        /** Text that we add to custom (user-specified) attributes in TOML files. */
        static constexpr std::string_view sCustomAttributePrefix = "..";

        ne_Serializable_GENERATED
    };

    template <typename SmartPointer, typename InnerType>
        requires std::derived_from<InnerType, Serializable> &&
                 (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                  std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
    std::variant<SmartPointer, Error> Serializable::deserialize(const std::filesystem::path& pathToFile) {
        std::unordered_map<std::string, std::string> foundCustomAttributes;
        return deserialize<SmartPointer>(pathToFile, foundCustomAttributes);
    }

    template <typename SmartPointer, typename InnerType>
        requires std::derived_from<InnerType, Serializable> &&
                 (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                  std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
    std::variant<SmartPointer, Error> Serializable::deserialize(
        const std::filesystem::path& pathToFile,
        std::unordered_map<std::string, std::string>& customAttributes) {
        return deserialize<SmartPointer>(pathToFile, customAttributes, "");
    }

    template <typename SmartPointer, typename InnerType>
        requires std::derived_from<InnerType, Serializable> &&
                 (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                  std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
    std::variant<SmartPointer, Error> Serializable::deserialize(
        const std::filesystem::path& pathToFile, const std::string& sEntityId) {
        std::unordered_map<std::string, std::string> foundCustomAttributes;
        return deserialize<SmartPointer>(pathToFile, foundCustomAttributes, sEntityId);
    }

    template <typename SmartPointer, typename InnerType>
        requires std::same_as<SmartPointer, sgc::GcPtr<Serializable>> ||
                 std::same_as<SmartPointer, std::unique_ptr<Serializable>>
    std::variant<std::vector<DeserializedObjectInformation<SmartPointer>>, Error>
    Serializable::deserializeMultiple(std::filesystem::path pathToFile) {
        // Resolve path.
        auto optionalError = resolvePathToToml(pathToFile);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Parse file.
        toml::value tomlData;
        try {
            tomlData = toml::parse(pathToFile);
        } catch (std::exception& exception) {
            return Error(std::format(
                "failed to parse TOML file at \"{}\", error: {}", pathToFile.string(), exception.what()));
        }

        // Get TOML as table.
        const auto fileTable = tomlData.as_table();
        if (fileTable.empty()) [[unlikely]] {
            return Error("provided toml value has 0 sections while expected at least 1 section");
        }

        // Deserialize.
        std::vector<DeserializedObjectInformation<SmartPointer>> deserializedObjects;
        for (const auto& [sSectionName, tomlValue] : fileTable) {
            // Get type GUID.
            const auto iIdEndDotPos = sSectionName.rfind('.');
            if (iIdEndDotPos == std::string::npos) [[unlikely]] {
                return Error("provided toml value does not contain entity ID");
            }
            const auto sTypeGuid = sSectionName.substr(iIdEndDotPos + 1);

            // Get entity ID chain.
            const auto sEntityId = sSectionName.substr(0, iIdEndDotPos);

            // Check if this is a sub-entity.
            if (sEntityId.contains('.')) {
                // Only deserialize top-level entities because sub-entities (fields)
                // will be deserialized while we deserialize top-level entities.
                continue;
            }

            // Deserialize object from this section.
            std::unordered_map<std::string, std::string> customAttributes;
            auto result = deserializeFromSection<SmartPointer>(
                tomlData, customAttributes, sSectionName, sTypeGuid, sEntityId, pathToFile);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save object info.
            DeserializedObjectInformation objectInfo(
                std::get<SmartPointer>(std::move(result)), sEntityId, customAttributes);
            deserializedObjects.push_back(std::move(objectInfo));
        }

        return deserializedObjects;
    }

    template <typename SmartPointer, typename InnerType>
        requires std::derived_from<InnerType, Serializable> &&
                 (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                  std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
    std::variant<SmartPointer, Error> Serializable::deserialize(
        std::filesystem::path pathToFile,
        std::unordered_map<std::string, std::string>& customAttributes,
        const std::string& sEntityId) {
        // Resolve path.
        auto optionalError = resolvePathToToml(pathToFile);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Parse file.
        toml::value tomlData;
        try {
            tomlData = toml::parse(pathToFile);
        } catch (std::exception& exception) {
            return Error(std::format(
                "failed to parse TOML file at \"{}\", error: {}", pathToFile.string(), exception.what()));
        }

        // Deserialize.
        return deserialize<SmartPointer>(tomlData, customAttributes, sEntityId, pathToFile);
    }

    template <typename SmartPointer, typename InnerType>
        requires std::derived_from<InnerType, Serializable> &&
                 (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                  std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
    std::variant<SmartPointer, Error> Serializable::deserialize(
        const toml::value& tomlData,
        std::unordered_map<std::string, std::string>& customAttributes,
        std::string sEntityId,
        const std::optional<std::filesystem::path>& optionalPathToFile) {
        if (sEntityId.empty()) {
            // Put something as entity ID so it would not look weird.
            sEntityId = "0";
        }

        // Get TOML as table.
        const auto fileTable = tomlData.as_table();
        if (fileTable.empty()) [[unlikely]] {
            return Error("provided toml value has 0 sections while expected at least 1 section");
        }

        // Find a section that starts with the specified entity ID.
        // Each entity section has the following format: [entityId.GUID]
        // For sub entities (field with reflected type) format: [parentEntityId.childEntityId.childGUID]
        std::string sTargetSection;
        std::string sTypeGuid;
        for (const auto& [sSectionName, value] : fileTable) {
            // We can't just use `sSectionName.starts_with(sEntityId)` because we might make a mistake in the
            // following situation: [100...] with entity ID equal to "10" and even if we add a dot
            // to `sEntityId` we still might make a mistake in the following situation:
            // [10.30.GUID] while we look for just [10.GUID].

            // Get ID end position (GUID start position).
            const auto iIdEndDotPos = sSectionName.rfind('.');
            if (iIdEndDotPos == std::string::npos) [[unlikely]] {
                return Error(std::format("section name \"{}\" does not contain entity ID", sSectionName));
            }
            if (iIdEndDotPos + 1 == sSectionName.size()) [[unlikely]] {
                return Error(std::format("section name \"{}\" does not have a GUID", sSectionName));
            }
            if (iIdEndDotPos == 0) [[unlikely]] {
                return Error(std::format("section \"{}\" is not full", sSectionName));
            }

            // Get ID chain (either entity ID or something like "parentEntityId.childEntityId").
            if (std::string_view(sSectionName.data(), iIdEndDotPos) != sEntityId) { // compare without copy
                continue;
            }

            // Save target section name.
            sTargetSection = sSectionName;

            // Save this entity's GUID.
            sTypeGuid = sSectionName.substr(iIdEndDotPos + 1);

            break;
        }

        // Make sure something was found.
        if (sTargetSection.empty()) [[unlikely]] {
            return Error(std::format("could not find entity with ID \"{}\"", sEntityId));
        }

        return deserializeFromSection<SmartPointer>(
            tomlData, customAttributes, sTargetSection, sTypeGuid, sEntityId, optionalPathToFile);
    }

    template <typename SmartPointer, typename InnerType>
        requires std::derived_from<InnerType, Serializable> &&
                 (std::same_as<SmartPointer, sgc::GcPtr<InnerType>> ||
                  std::same_as<SmartPointer, std::unique_ptr<InnerType>>)
    std::variant<SmartPointer, Error> Serializable::deserializeFromSection(
        const toml::value& tomlData,
        std::unordered_map<std::string, std::string>& customAttributes,
        const std::string& sSectionName,
        const std::string& sTypeGuid,
        const std::string& sEntityId,
        const std::optional<std::filesystem::path>& optionalPathToFile) {
        // Get all keys (field names) from this section.
        const auto& targetSection = tomlData.at(sSectionName);
        if (!targetSection.is_table()) [[unlikely]] {
            return Error(std::format("found \"{}\" section is not a section", sSectionName));
        }

        // Collect keys from target section.
        const auto& sectionTable = targetSection.as_table();
        std::unordered_map<std::string_view, const toml::value*> fieldsToDeserialize;
        SmartPointer pOriginalEntity = nullptr;
        for (const auto& [sKey, value] : sectionTable) {
            if (sKey == sNothingToSerializeKey) {
                continue;
            } else if (sKey == sPathRelativeToResKey) {
                // Make sure the value is string.
                if (!value.is_string()) [[unlikely]] {
                    return Error(
                        std::format("found \"{}\" key's value is not string", sPathRelativeToResKey));
                }

                // Deserialize original entity.
                auto deserializeResult = Serializable::deserialize<SmartPointer>(
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / value.as_string());
                if (std::holds_alternative<Error>(deserializeResult)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(deserializeResult));
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
                pOriginalEntity = std::get<SmartPointer>(std::move(deserializeResult));
            } else if (sKey.starts_with(sCustomAttributePrefix)) {
                // Custom attribute.
                // Make sure it's a string.
                if (!value.is_string()) [[unlikely]] {
                    return Error(std::format("found custom attribute \"{}\" is not a string", sKey));
                }

                // Add attribute.
                customAttributes[sKey.substr(sCustomAttributePrefix.size())] = value.as_string();
            } else {
                fieldsToDeserialize[sKey] = &value;
            }
        }

        // Get archetype for found GUID.
        const auto pType = getClassForGuid(sTypeGuid);
        if (pType == nullptr) [[unlikely]] {
            if (pOriginalEntity) {
                return Error(std::format(
                    "GUID \"{}\" was not found in the database, but "
                    "the original object at \"{}\" (ID \"{}\") was deserialized",
                    sTypeGuid,
                    pOriginalEntity->getPathDeserializedFromRelativeToRes().value().first,
                    pOriginalEntity->getPathDeserializedFromRelativeToRes().value().second));
            } else {
                return Error(std::format("no type found for GUID \"{}\"", sTypeGuid));
            }
        }

        // Make sure the type indeed derives from serializable.
        if (!SerializableObjectFieldSerializer::isDerivedFromSerializable(pType)) [[unlikely]] {
            if (pOriginalEntity) {
                return Error(std::format(
                    "deserialized type for \"{}\" does not derive from {}, but "
                    "the original object at \"{}\" (ID \"{}\") was deserialized",
                    sTypeGuid,
                    staticGetArchetype().getName(),
                    pOriginalEntity->getPathDeserializedFromRelativeToRes().value().first,
                    pOriginalEntity->getPathDeserializedFromRelativeToRes().value().second));
            } else {
                return Error(std::format(
                    "deserialized type with GUID \"{}\" does not derive from {}",
                    sTypeGuid,
                    staticGetArchetype().getName()));
            }
        }

        // Create instance.
        SmartPointer pSmartPointerInstance = nullptr;
        if (pOriginalEntity != nullptr) {
            // Use the original entity instead of creating a new one.
            pSmartPointerInstance = std::move(pOriginalEntity);
        }
        if (pSmartPointerInstance == nullptr) {
            if constexpr (std::is_same_v<SmartPointer, std::unique_ptr<InnerType>>) {
                // Create a unique instance.
                pSmartPointerInstance = pType->makeUniqueInstance<InnerType>();
                if (pSmartPointerInstance == nullptr) [[unlikely]] {
                    return Error(std::format(
                        "unable to make an object of type \"{}\" using type's default constructor "
                        "(does type \"{}\" has a default constructor?)",
                        pType->getName(),
                        pType->getName()));
                }
            }
            // NOTE: don't allow deserializing into a `shared_ptr` since it may create a false assumption
            // that this `shared_ptr` will also save/load its referenced entity or keep its reference
            // (you can easily convert a unique_ptr to a shared_ptr so it should not be an issue).
            else if (std::is_same_v<SmartPointer, sgc::GcPtr<InnerType>>) {
                // Create GC instance.
                // (this part is a temporary solution until we will add a `makeGcFromThisType` method to
                // `rfk::Struct`)
                std::unique_ptr<InnerType> pInstance = pType->makeUniqueInstance<InnerType>();
                if (pInstance == nullptr) [[unlikely]] {
                    return Error(std::format(
                        "unable to make an object of type \"{}\" using type's default constructor "
                        "(does type \"{}\" has a default constructor?)",
                        pType->getName(),
                        pType->getName()));
                }
                sgc::GcPtr<rfk::Object> pParentGcInstance = pInstance->makeGcFromThisType();
                pSmartPointerInstance = dynamic_cast<InnerType*>(pParentGcInstance.get());
                if (pSmartPointerInstance == nullptr) [[unlikely]] {
                    return Error(std::format(
                        "dynamic cast failed to cast the type \"{}\" to the specified template argument "
                        "(are you trying to deserialize into a wrong type?)",
                        pParentGcInstance->getArchetype().getName()));
                }
            } else [[unlikely]] {
                return Error("unexpected smart pointer type received");
            }
        }

        // Get field serializers.
        const auto vFieldSerializers = FieldSerializerManager::getFieldSerializers();
        const auto vBinaryFieldSerializers = FieldSerializerManager::getBinaryFieldSerializers();

        // Deserialize fields.
        for (auto& [sFieldName, pFieldTomlValue] : fieldsToDeserialize) {
            if (sFieldName == sSubEntityFieldNameKey) {
                // This field is used as section metadata and tells us what field of parent entity
                // this section describes.
                continue;
            }

            // Get field by name.
            rfk::Field const* pField =
                pType->getFieldByName(sFieldName.data(), rfk::EFieldFlags::Default, true);
            if (pField == nullptr) [[unlikely]] { // rarely happens
                Logger::get().warn(std::format(
                    "field name \"{}\" exists in the specified toml value but does not exist in the "
                    "actual object (if you removed/renamed this reflected field from your "
                    "class/struct - ignore this warning)",
                    sFieldName));
                continue;
            }
            const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

            // Check if it's serializable.
            if (!SerializableObjectFieldSerializer::isFieldSerializable(*pField)) {
                continue;
            }

            // Check if we need to deserialize from external file.
            const auto pSerializeProperty = pField->getProperty<Serialize>();
            if (pSerializeProperty->getSerializationType() == FieldSerializationType::FST_AS_EXTERNAL_FILE ||
                pSerializeProperty->getSerializationType() ==
                    FieldSerializationType::FST_AS_EXTERNAL_BINARY_FILE) {
                // Make sure this field derives from `Serializable`.
                if (!SerializableObjectFieldSerializer::isDerivedFromSerializable(
                        pField->getType().getArchetype())) [[unlikely]] {
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

                // Prepare path to the external file.
                if (!pFieldTomlValue->is_string()) [[unlikely]] {
                    return Error(std::format(
                        "expected field \"{}\" to store external filename in file \"{}\"",
                        pField->getName(),
                        optionalPathToFile.value().string()));
                }
                const auto sExternalFileName = pFieldTomlValue->as_string();
                const auto pathToExternalFile = optionalPathToFile.value().parent_path() / sExternalFileName;

                // Get field object.
                const auto pFieldObject =
                    reinterpret_cast<Serializable*>(pField->getPtrUnsafe(pSmartPointerInstance.get()));

                if (pSerializeProperty->getSerializationType() == FST_AS_EXTERNAL_FILE) {
                    // Deserialize external file.
                    auto result = deserialize<std::unique_ptr<Serializable>>(pathToExternalFile);
                    if (std::holds_alternative<Error>(result)) [[unlikely]] {
                        auto error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        return error;
                    }
                    const auto pDeserializedExternalField =
                        std::get<std::unique_ptr<Serializable>>(std::move(result));

                    // Clone deserialized data to field.
                    auto optionalError = SerializableObjectFieldSerializer::cloneSerializableObject(
                        pDeserializedExternalField.get(), pFieldObject, false);
                    if (optionalError.has_value()) [[unlikely]] {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        return error;
                    }
                } else if (pSerializeProperty->getSerializationType() == FST_AS_EXTERNAL_BINARY_FILE) {
                    // Deserialize binary file.
                    bool bDeserialized = false;
                    for (const auto& pBinarySerializer : vBinaryFieldSerializers) {
                        if (!pBinarySerializer->isFieldTypeSupported(pField)) {
                            continue;
                        }

                        // Deserialize as binary.
                        auto optionalError = pBinarySerializer->deserializeField(
                            pathToExternalFile, pSmartPointerInstance.get(), pField);
                        if (optionalError.has_value()) [[unlikely]] {
                            auto error = optionalError.value();
                            error.addCurrentLocationToErrorStack();
                            return error;
                        }

                        // Finished.
                        bDeserialized = true;
                        break;
                    }

                    // Make sure we deserialized this field.
                    if (!bDeserialized) [[unlikely]] {
                        return Error(std::format(
                            "the field \"{}\" with type \"{}\" (maybe inherited) has "
                            "unsupported for deserialization type",
                            pField->getName(),
                            pField->getCanonicalTypeName()));
                    }

                    // Notify about deserialization.
                    pFieldObject->onAfterDeserialized();
                } else [[unlikely]] {
                    return Error(std::format("unhandled case on field \"{}\"", pField->getName()));
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
                        pFieldTomlValue,
                        pSmartPointerInstance.get(),
                        pField,
                        sSectionName,
                        sEntityId,
                        customAttributes);
                    if (optionalError.has_value()) [[unlikely]] {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        if (pOriginalEntity) {
                            Logger::get().error(std::format(
                                "an error occurred while deserializing "
                                "changed field (this field was not deserialized), error: {}",
                                error.getFullErrorMessage()));
                        } else {
                            return error;
                        }
                    }
                }
            }

            if (!bFoundSerializer) [[unlikely]] {
                Logger::get().warn(
                    std::format("unable to find a deserializer that supports field \"{}\"", sFieldName));
            }
        }

        if (optionalPathToFile.has_value() &&
            optionalPathToFile->string().starts_with(
                ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT).string())) {
            // File is located in the `res` directory, save a relative path to the `res` directory.
            auto sRelativePath = std::filesystem::relative(
                                     optionalPathToFile->string(),
                                     ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT))
                                     .string();

            // Replace all '\' characters with '/' just to be consistent.
            std::replace(sRelativePath.begin(), sRelativePath.end(), '\\', '/');

            // Remove the forward slash at the beginning (if exists).
            if (sRelativePath.starts_with('/')) {
                sRelativePath = sRelativePath.substr(1);
            }

            // Double check that everything is correct.
            const auto pathToOriginalFile =
                ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / sRelativePath;
            if (!std::filesystem::exists(pathToOriginalFile)) [[unlikely]] {
                return Error(std::format(
                    "failed to save the relative path to the `res` directory for the file at \"{}\", "
                    "reason: constructed path \"{}\" does not exist",
                    optionalPathToFile->string(),
                    pathToOriginalFile.string()));
            }

            // Save deserialization path.
            pSmartPointerInstance->pathDeserializedFromRelativeToRes = {sRelativePath, sEntityId};
        }

        // Notify about deserialization finished.
        Serializable* pTarget = dynamic_cast<Serializable*>(pSmartPointerInstance.get());
        pTarget->onAfterDeserialized();

        return pSmartPointerInstance;
    }

}; // namespace ne RNAMESPACE()

File_Serializable_GENERATED
