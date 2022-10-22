#include "io/serializers/PrimitiveFieldSerializer.h"

// External.
#include "fmt/format.h"

namespace ne {
    bool PrimitiveFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto& fieldType = pField->getType();

        // The `match` function can only be used with the primitive types.
        if (fieldType.match(rfk::getType<bool>())) {
            return true;
        } else if (fieldType.match(rfk::getType<int>())) {
            return true;
        } else if (fieldType.match(rfk::getType<long long>())) {
            return true;
        } else if (fieldType.match(rfk::getType<float>())) {
            return true;
        } else if (fieldType.match(rfk::getType<double>())) {
            return true;
        }

        return false;
    }

    std::optional<Error> PrimitiveFieldSerializer::serializeField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId) {
        const auto& fieldType = pField->getType();
        const auto sFieldName = pField->getName();

        // The `match` function can only be used with the primitive types.
        if (fieldType.match(rfk::getType<bool>())) {
            pTomlData->operator[](sSectionName).operator[](sFieldName) = pField->getUnsafe<bool>(pFieldOwner);
        } else if (fieldType.match(rfk::getType<int>())) {
            pTomlData->operator[](sSectionName).operator[](sFieldName) = pField->getUnsafe<int>(pFieldOwner);
        } else if (fieldType.match(rfk::getType<long long>())) {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<long long>(pFieldOwner);
        } else if (fieldType.match(rfk::getType<float>())) {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<float>(pFieldOwner);
        } else if (fieldType.match(rfk::getType<double>())) {
            // Store double as string for better precision.
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                toml::format(toml::value(pField->getUnsafe<double>(pFieldOwner)));
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                pField->getCanonicalTypeName(),
                sFieldName));
        }

        return {};
    }

    std::optional<Error> PrimitiveFieldSerializer::deserializeField(
        const toml::value* pTomlDocument,
        const toml::value* pTomlValue,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sOwnerSectionName,
        const std::string& sEntityId,
        std::unordered_map<std::string, std::string>& customAttributes) {
        const auto& fieldType = pField->getType();
        const auto sFieldName = pField->getName();

        // The `match` function can only be used with the primitive types.
        if (fieldType.match(rfk::getType<bool>()) && pTomlValue->is_boolean()) {
            auto fieldValue = pTomlValue->as_boolean();
            pField->setUnsafe<bool>(pFieldOwner, std::move(fieldValue));
        } else if (fieldType.match(rfk::getType<int>()) && pTomlValue->is_integer()) {
            auto fieldValue = static_cast<int>(pTomlValue->as_integer());
            pField->setUnsafe<int>(pFieldOwner, std::move(fieldValue));
        } else if (fieldType.match(rfk::getType<long long>()) && pTomlValue->is_integer()) {
            long long fieldValue = pTomlValue->as_integer();
            pField->setUnsafe<long long>(pFieldOwner, std::move(fieldValue));
        } else if (fieldType.match(rfk::getType<float>()) && pTomlValue->is_floating()) {
            auto fieldValue = static_cast<float>(pTomlValue->as_floating());
            pField->setUnsafe<float>(pFieldOwner, std::move(fieldValue));
        } else if (fieldType.match(rfk::getType<double>()) && pTomlValue->is_string()) {
            // We store double as a string for better precision.
            try {
                double fieldValue = std::stod(pTomlValue->as_string().str);
                pField->setUnsafe<double>(pFieldOwner, std::move(fieldValue));
            } catch (std::exception& ex) {
                return Error(fmt::format(
                    "Failed to convert string to double for field \"{}\": {}", sFieldName, ex.what()));
            }
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                pField->getCanonicalTypeName(),
                sFieldName));
        }

        return {};
    }

    std::optional<Error> PrimitiveFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        // The `match` function can only be used with the primitive types.
        if (pFromField->getType().match(rfk::getType<bool>())) {
            auto value = pFromField->getUnsafe<bool>(pFromInstance);
            pToField->setUnsafe<bool>(pToInstance, std::move(value));
        } else if (pFromField->getType().match(rfk::getType<int>())) {
            auto value = pFromField->getUnsafe<int>(pFromInstance);
            pToField->setUnsafe<int>(pToInstance, std::move(value));
        } else if (pFromField->getType().match(rfk::getType<long long>())) {
            auto value = pFromField->getUnsafe<long long>(pFromInstance);
            pToField->setUnsafe<long long>(pToInstance, std::move(value));
        } else if (pFromField->getType().match(rfk::getType<float>())) {
            auto value = pFromField->getUnsafe<float>(pFromInstance);
            pToField->setUnsafe<float>(pToInstance, std::move(value));
        } else if (pFromField->getType().match(rfk::getType<double>())) {
            auto value = pFromField->getUnsafe<double>(pFromInstance);
            pToField->setUnsafe<double>(pToInstance, std::move(value));
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                pFromField->getCanonicalTypeName(),
                pFromField->getName()));
        }

        return {};
    }
} // namespace ne
