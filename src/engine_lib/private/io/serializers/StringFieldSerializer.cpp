#include "io/serializers/StringFieldSerializer.h"

// External.
#include "fmt/format.h"

namespace ne {
    bool StringFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == sStringCanonicalTypeName) {
            return true;
        }

        return false;
    }

    std::optional<Error> StringFieldSerializer::serializeField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        if (sFieldCanonicalTypeName == sStringCanonicalTypeName) {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::string>(pFieldOwner);
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
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
        const auto sFieldName = pField->getName();

        if (sFieldCanonicalTypeName == sStringCanonicalTypeName && pTomlValue->is_string()) {
            auto fieldValue = pTomlValue->as_string().str;
            pField->setUnsafe<std::string>(pFieldOwner, std::move(fieldValue));
        } else if (!pTomlValue->is_string()) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                "but the TOML value is not string.",
                sFieldCanonicalTypeName,
                sFieldName));
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
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
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFromField->getName()));
        }

        return {};
    }
} // namespace ne
