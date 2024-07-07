#pragma once

// Standard.
#include <optional>
#include <format>

// Custom.
#include "misc/Error.h"

// External.
#include "Refureku/Refureku.h"
#include "toml11/single_include/toml.hpp"

namespace ne {
    class Serializable;

    /**
     * Used by various serializers to store float and double values as string for better precision.
     *
     * @param value Value to convert.
     *
     * @return Converted values with fixed precision.
     */
    inline std::string floatingToString(double value) { return std::format("{:.15f}", value); }

    /**
     * Interface for implementing support for serialization of new field types.
     *
     * By implementing this interface and registering it in the FieldSerializerManager class you can
     * add support for serialization/deserialization of new field types and extend
     * serialization/deserialization functionality for Serializable derived classes.
     */
    class IFieldSerializer {
    public:
        IFieldSerializer() = default;
        virtual ~IFieldSerializer() = default;

        /**
         * Tests if this serializer supports serialization/deserialization of this field.
         *
         * @param pField Field to test for serialization/deserialization support.
         *
         * @return `true` if this serializer can be used to serialize this field, `false` otherwise.
         */
        virtual bool isFieldTypeSupported(const rfk::Field* pField) = 0;

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
            Serializable* pOriginalObject = nullptr) = 0;

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
            std::unordered_map<std::string, std::string>& customAttributes) = 0;

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
            const rfk::Field* pToField) = 0;

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
            const rfk::Field* pFieldB) = 0;
    };
} // namespace ne
