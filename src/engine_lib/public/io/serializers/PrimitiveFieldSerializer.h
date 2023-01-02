#pragma once

// Custom.
#include "io/serializers/IFieldSerializer.hpp"

namespace ne {
    /**
     * Serializer for some primitive types: `bool`, `int`, `unsigned int`, `long long`,
     * `unsigned long long`, `float`, `double`.
     */
    class PrimitiveFieldSerializer : public IFieldSerializer {
    public:
        PrimitiveFieldSerializer() = default;
        virtual ~PrimitiveFieldSerializer() override = default;

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
    };
} // namespace ne
