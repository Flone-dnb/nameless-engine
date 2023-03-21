#pragma once

// Standard.
#include <variant>

// Custom.
#include "io/serializers/IFieldSerializer.hpp"
#include "misc/GC.hpp"

namespace rfk {
    class Struct;
    class Field;
    class Archetype;
} // namespace rfk

namespace ne {
    /**
     * Serializer for field types that derive from Serializable class.
     */
    class SerializableObjectFieldSerializer : public IFieldSerializer {
        // Checks GUIDs uniqueness in debug builds.
        friend class Game;

    public:
        SerializableObjectFieldSerializer() = default;
        virtual ~SerializableObjectFieldSerializer() override = default;

        /**
         * Serializes field's object (serializable object wrapped into a container, such as `gc`)
         * into a TOML value.
         *
         * @remark This function can be used by other serializers.
         *
         * @param pObject          Object to serialize.
         * @param pTomlData        TOML value to serialize the field to.
         * @param sFieldName       Name of the field to serialize.
         * @param sSectionName     Name of the section (for TOML value) to use for this field.
         * @param sEntityId        Current ID string of the entity (field owner) that we are serializing.
         * Only used when serializing a field of type that derives from Serializable.
         * @param iSubEntityId     Current ID of the sub entity (sub entity of the field owner).
         * Only used when serializing a field of type that derives from Serializable.
         * @param pOriginalObject  Optional. Original object of the same type as the object being
         * serialized, this object is a deserialized version of the object being serialized,
         * used to compare serializable fields' values and only serialize changed values.
         * Only used when serializing a field of type that derives from Serializable.
         *
         * @return Error if something went wrong, empty otherwise.
         */
        [[nodiscard]] static std::optional<Error> serializeFieldObject(
            Serializable* pObject,
            toml::value* pTomlData,
            const std::string& sFieldName,
            const std::string& sSectionName,
            const std::string& sEntityId,
            size_t& iSubEntityId,
            Serializable* pOriginalObject = nullptr);

        /**
         * Clones reflected serializable fields of one object to another.
         *
         * @remark This function can be used by other serializers.
         *
         * @param pFrom                    Object to clone fields from.
         * @param pTo                      Object to clone fields to.
         * @param bNotifyAboutDeserialized Whether or not to notify the target object we cloned fields to
         * about it being finally deserialized or not. This should be `true` if you are calling this function
         * as the last step of your `deserialize` function or in some special cases for new objects
         * that won't reach `deserialize` function (for ex. when copying data to new object), otherwise
         * `false`.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error>
        cloneSerializableObject(Serializable* pFrom, Serializable* pTo, bool bNotifyAboutDeserialized);

        /**
         * Deserializes field's object from a TOML value.
         *
         * @remark This function can be used by other serializers.
         *
         * @param pTomlDocument     TOML document that contains a value to deserialize.
         * @param pTomlValue        TOML value to deserialize the field from.
         * @param sFieldName        Name of the field to deserialize to.
         * @param pTarget           Field's object to deserialize the value to.
         * @param sOwnerSectionName Name of the TOML section where is field was found.
         * @param sEntityId         Current ID string of the entity (field owner) that we
         * are deserializing.
         * @param customAttributes  Pairs of values that were found with this object in TOML data.
         * Only found when deserializing a field of type that derives from Serializable.
         *
         * @return Error if something went wrong, otherwise deserialized object.
         */
        static std::variant<std::shared_ptr<Serializable>, Error> deserializeSerializableObject(
            const toml::value* pTomlDocument,
            const toml::value* pTomlValue,
            const std::string& sFieldName,
            Serializable* pTarget,
            const std::string& sOwnerSectionName,
            const std::string& sEntityId,
            std::unordered_map<std::string, std::string>& customAttributes);

        /**
         * Checks if the specified fields' values are equal or not.
         *
         * @remark Uses all registered field serializers to compare each field.
         *
         * @remark This function can be used by other serializers.
         *
         * @param pObjectA Object to compare with B.
         * @param pObjectB Object to compare with A.
         *
         * @return `false` if some field is unsupported by this serializer or if fields' values
         * are not equal, `true` otherwise.
         */
        static bool isSerializableObjectValueEqual(Serializable* pObjectA, Serializable* pObjectB);

        /**
         * Looks if the specified canonical type name derives from `Serializable` or not.
         *
         * @param sCanonicalTypeName Canonical type name (not just type name, see
         * `rfk::Field::getCanonicalTypeName`).
         *
         * @return `false` if the specified type was not found to be derived from `Serializable`,
         * otherwise `true`.
         */
        static bool isTypeDerivesFromSerializable(const std::string& sCanonicalTypeName);

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
         * Tests if this serializer supports serialization/deserialization of this field.
         *
         * @param pField Field to test for serialization/deserialization support.
         *
         * @return `true` if this serializer can be used to serialize this field, `false` otherwise.
         */
        virtual bool isFieldTypeSupported(const rfk::Field* pField) override;

        /**
         * Serializes field into a TOML value.
         *
         * @param pTomlData        TOML value to serialize the field to.
         * @param pFieldOwner      Field's owner.
         * @param pField           Field to serialize.
         * @param sSectionName     Name of the section (for TOML value) to use for this field.
         * @param sEntityId        Current ID string of the entity (field owner) that we are serializing.
         * Only used when serializing a field of type that derives from Serializable.
         * @param iSubEntityId     Current ID of the sub entity (sub entity of the field owner).
         * Only used when serializing a field of type that derives from Serializable.
         * @param pOriginalObject  Optional. Original object of the same type as the object being
         * serialized, this object is a deserialized version of the object being serialized,
         * used to compare serializable fields' values and only serialize changed values.
         * Only used when serializing a field of type that derives from Serializable.
         *
         * @return Error if something went wrong, empty otherwise.
         */
        [[nodiscard]] virtual std::optional<Error> serializeField(
            toml::value* pTomlData,
            Serializable* pFieldOwner,
            const rfk::Field* pField,
            const std::string& sSectionName,
            const std::string& sEntityId,
            size_t& iSubEntityId,
            Serializable* pOriginalObject = nullptr) override;

        /**
         * Deserializes field from a TOML value.
         *
         * @param pTomlDocument     TOML document that contains a value to deserialize.
         * @param pTomlValue        TOML value to deserialize the field from.
         * @param pFieldOwner       Field's owner.
         * @param pField            Field to deserialize TOML value to.
         * @param sOwnerSectionName Name of the TOML section where is field was found.
         * @param sEntityId         Current ID string of the entity (field owner) that we
         * are deserializing.
         * @param customAttributes  Pairs of values that were found with this object in TOML data.
         * Only found when deserializing a field of type that derives from Serializable.
         *
         * @return Error if something went wrong, empty otherwise.
         */
        [[nodiscard]] virtual std::optional<Error> deserializeField(
            const toml::value* pTomlDocument,
            const toml::value* pTomlValue,
            Serializable* pFieldOwner,
            const rfk::Field* pField,
            const std::string& sOwnerSectionName,
            const std::string& sEntityId,
            std::unordered_map<std::string, std::string>& customAttributes) override;

        /**
         * Clones field's data from one object to another.
         *
         * @param pFromInstance Instance to copy the field from.
         * @param pFromField    Field to copy.
         * @param pToInstance   Instance to copy to.
         * @param pToField      Field to copy to.
         *
         * @return Error if something went wrong, empty otherwise.
         */
        [[nodiscard]] virtual std::optional<Error> cloneField(
            Serializable* pFromInstance,
            const rfk::Field* pFromField,
            Serializable* pToInstance,
            const rfk::Field* pToField) override;

        /**
         * Checks if the specified fields' values are equal or not.
         *
         * @param pFieldAOwner Owner of the field A.
         * @param pFieldA      Field A to compare.
         * @param pFieldBOwner Owner of the field B.
         * @param pFieldB      Field B to compare.
         *
         * @return `false` if some field is unsupported by this serializer or if fields' values
         * are not equal, `true` otherwise.
         */
        virtual bool isFieldValueEqual(
            Serializable* pFieldAOwner,
            const rfk::Field* pFieldA,
            Serializable* pFieldBOwner,
            const rfk::Field* pFieldB) override;

    private:
#if defined(DEBUG)
        /**
         * Checks that all classes/structs that inherit from Serializable have correct and unique GUIDs.
         *
         * Automatically called by the Game object (object that owns GameInstance) and has no point in being
         * called from your game's code.
         */
        static void checkGuidUniqueness();

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
         * Looks if the specified canonical type name derives from `Serializable`.
         *
         * @param sCanonicalTypeName Canonical type name without namespace in the name.
         * @param pNamespace         Optional. Namespace that the specified type resides in.
         *
         * @return `false` if the specified type was not found to be derived from `Serializable`,
         * otherwise `true`.
         */
        static bool isTypeDerivesFromSerializable(
            const std::string& sCanonicalTypeName, const rfk::Namespace* pNamespace);

        /** Name of the key in which to store name of the field a section represents. */
        static inline const auto sSubEntityFieldNameKey = ".field_name";
    };
} // namespace ne
