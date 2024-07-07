#include "io/serializers/StringFieldSerializer.h"

// Standard.
#include <format>

namespace ne {
    bool StringFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        return sFieldCanonicalTypeName == sStringCanonicalTypeName;
    }

    std::optional<Error> StringFieldSerializer::serializeField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId,
        Serializable* pOriginalObject) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto pFieldName = pField->getName();

        if (sFieldCanonicalTypeName == sStringCanonicalTypeName) {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<std::string>(pFieldOwner);
        } else [[unlikely]] {
            return Error(std::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        return {};
    }

    std::optional<Error> StringFieldSerializer::deserializeField(
        const toml::value* pTomlDocument,
        const toml::value* pTomlValue,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sOwnerSectionName,
        const std::string& sEntityId,
        std::unordered_map<std::string, std::string>& customAttributes) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto pFieldName = pField->getName();

        if (sFieldCanonicalTypeName == sStringCanonicalTypeName && pTomlValue->is_string()) {
            auto fieldValue = pTomlValue->as_string();
            pField->setUnsafe<std::string>(pFieldOwner, std::move(fieldValue));
        } else [[unlikely]] {
            if (!pTomlValue->is_string()) {
                return Error(std::format(
                    "type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                    "but the TOML value is not string",
                    sFieldCanonicalTypeName,
                    pFieldName));
            }

            return Error(std::format(
                "type \"{}\" of the specified field \"{}\" is not supported by this serializer",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        return {};
    }

    std::optional<Error> StringFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == sStringCanonicalTypeName) {
            auto value = pFromField->getUnsafe<std::string>(pFromInstance);
            pToField->setUnsafe<std::string>(pToInstance, std::move(value));
        } else [[unlikely]] {
            return Error(std::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFromField->getName()));
        }

        return {};
    }

    bool StringFieldSerializer::isFieldValueEqual(
        Serializable* pFieldAOwner,
        const rfk::Field* pFieldA,
        Serializable* pFieldBOwner,
        const rfk::Field* pFieldB) {
        if (!isFieldTypeSupported(pFieldA)) {
            return false;
        }
        if (!isFieldTypeSupported(pFieldB)) {
            return false;
        }

        const auto sValueA = pFieldA->getUnsafe<std::string>(pFieldAOwner);
        const auto sValueB = pFieldB->getUnsafe<std::string>(pFieldBOwner);

        return sValueA == sValueB;
    }
} // namespace ne
