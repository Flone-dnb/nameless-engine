#pragma once

// Custom.
#include "io/IFieldSerializer.hpp"

namespace ne {
    /**
     * Serializer for the `std::string` type.
     */
    class StringFieldSerializer : public IFieldSerializer {
    public:
        StringFieldSerializer() = default;
        virtual ~StringFieldSerializer() override = default;

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
         * @param pTomlData     TOML value to serialize the field to.
         * @param pFieldOwner   Field's owner.
         * @param pField        Field to serialize.
         * @param sSectionName  Name of the section (for TOML value) to use for this field.
         * @param sEntityId     Current ID string of the entity (field owner) that we are serializing.
         * Only used when serializing a field of type that derives from Serializable.
         * @param iSubEntityId  Current ID of the sub entity (sub entity of the field owner).
         * Only used when serializing a field of type that derives from Serializable.
         *
         * @return Error if something went wrong, empty otherwise.
         */
        virtual std::optional<Error> serializeField(
            toml::value* pTomlData,
            Serializable* pFieldOwner,
            const rfk::Field* pField,
            const std::string& sSectionName,
            const std::string& sEntityId,
            size_t& iSubEntityId) override;

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
        virtual std::optional<Error> deserializeField(
            toml::value* pTomlDocument,
            toml::value* pTomlValue,
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
        virtual std::optional<Error> cloneField(
            Serializable* pFromInstance,
            const rfk::Field* pFromField,
            Serializable* pToInstance,
            const rfk::Field* pToField) override;

    private:
        /** Canonical type name for `std::string` fields. */
        static inline const std::string sStringCanonicalTypeName = "std::basic_string<char>";
    };
} // namespace ne
