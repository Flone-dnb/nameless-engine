#include "io/serializers/PrimitiveFieldSerializer.h"

// Standard.
#include <format>

namespace ne {
    bool PrimitiveFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto& fieldType = pField->getType();

        // The `match` function can only be used with the primitive types.
        if (fieldType.match(rfk::getType<bool>())) {
            return true;
        }
        if (fieldType.match(rfk::getType<int>())) {
            return true;
        }
        if (fieldType.match(rfk::getType<unsigned int>())) {
            return true;
        }
        if (fieldType.match(rfk::getType<long long>())) {
            return true;
        }
        if (fieldType.match(rfk::getType<unsigned long long>())) {
            return true;
        }
        if (fieldType.match(rfk::getType<float>())) {
            return true;
        }
        if (fieldType.match(rfk::getType<double>())) {
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
        size_t& iSubEntityId,
        Serializable* pOriginalObject) {
        const auto& fieldType = pField->getType();
        const auto pFieldName = pField->getName();

        // The `match` function can only be used with the primitive types.
        if (fieldType.match(rfk::getType<bool>())) {
            pTomlData->operator[](sSectionName).operator[](pFieldName) = pField->getUnsafe<bool>(pFieldOwner);
        } else if (fieldType.match(rfk::getType<int>())) {
            pTomlData->operator[](sSectionName).operator[](pFieldName) = pField->getUnsafe<int>(pFieldOwner);
        } else if (fieldType.match(rfk::getType<unsigned int>())) {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<unsigned int>(pFieldOwner);
        } else if (fieldType.match(rfk::getType<long long>())) {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<long long>(pFieldOwner);
        } else if (fieldType.match(rfk::getType<unsigned long long>())) {
            const auto iValue = pField->getUnsafe<unsigned long long>(pFieldOwner);
            // Since toml11 (library that we use) uses `long long` for integers,
            // store this type as a string.
            const std::string sValue = std::to_string(iValue);
            pTomlData->operator[](sSectionName).operator[](pFieldName) = sValue;
        } else if (fieldType.match(rfk::getType<float>())) {
            // Store float as string for better precision.
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                toml::format(toml::value(pField->getUnsafe<float>(pFieldOwner)));
        } else if (fieldType.match(rfk::getType<double>())) {
            // Store double as string for better precision.
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                toml::format(toml::value(pField->getUnsafe<double>(pFieldOwner)));
        } else {
            return Error(std::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                pField->getCanonicalTypeName(),
                pFieldName));
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
        const auto pFieldName = pField->getName();

        // The `match` function can only be used with the primitive types.
        if (fieldType.match(rfk::getType<bool>()) && pTomlValue->is_boolean()) {
            auto fieldValue = pTomlValue->as_boolean();
            pField->setUnsafe<bool>(pFieldOwner, std::move(fieldValue)); // NOLINT
        } else if (fieldType.match(rfk::getType<int>()) && pTomlValue->is_integer()) {
            auto fieldValue = static_cast<int>(pTomlValue->as_integer());
            pField->setUnsafe<int>(pFieldOwner, std::move(fieldValue)); // NOLINT
        } else if (fieldType.match(rfk::getType<unsigned int>()) && pTomlValue->is_integer()) {
            const auto iOriginalValue = pTomlValue->as_integer();
            auto fieldValue = static_cast<unsigned int>(iOriginalValue);
            if (iOriginalValue < 0) {
                // Since integers are stored as `long long` in toml11 library that we use,
                // we add this check.
                fieldValue = 0;
            }
            pField->setUnsafe<unsigned int>(pFieldOwner, std::move(fieldValue)); // NOLINT
        } else if (fieldType.match(rfk::getType<long long>()) && pTomlValue->is_integer()) {
            long long fieldValue = pTomlValue->as_integer();
            pField->setUnsafe<long long>(pFieldOwner, std::move(fieldValue)); // NOLINT
        } else if (fieldType.match(rfk::getType<unsigned long long>()) && pTomlValue->is_string()) {
            // Stored as a string.
            const auto& sValue = pTomlValue->as_string();
            try {
                unsigned long long iValue = std::stoull(sValue);
                pField->setUnsafe<unsigned long long>(pFieldOwner, std::move(iValue)); // NOLINT
            } catch (std::exception& ex) {
                return Error(std::format(
                    "Failed to convert string to unsigned long long for field \"{}\": {}",
                    pFieldName,
                    ex.what()));
            }
        } else if (fieldType.match(rfk::getType<float>()) && pTomlValue->is_string()) {
            // We store float as a string for better precision.
            try {
                float fieldValue = std::stof(pTomlValue->as_string().str);
                pField->setUnsafe<float>(pFieldOwner, std::move(fieldValue)); // NOLINT
            } catch (std::exception& ex) {
                return Error(std::format(
                    "Failed to convert string to double for field \"{}\": {}", pFieldName, ex.what()));
            }
        } else if (fieldType.match(rfk::getType<double>()) && pTomlValue->is_string()) {
            // We store double as a string for better precision.
            try {
                double fieldValue = std::stod(pTomlValue->as_string().str);
                pField->setUnsafe<double>(pFieldOwner, std::move(fieldValue)); // NOLINT
            } catch (std::exception& ex) {
                return Error(std::format(
                    "Failed to convert string to double for field \"{}\": {}", pFieldName, ex.what()));
            }
        } else {
            return Error(std::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                pField->getCanonicalTypeName(),
                pFieldName));
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
            pToField->setUnsafe<bool>(pToInstance, std::move(value)); // NOLINT
        } else if (pFromField->getType().match(rfk::getType<int>())) {
            auto value = pFromField->getUnsafe<int>(pFromInstance);
            pToField->setUnsafe<int>(pToInstance, std::move(value)); // NOLINT
        } else if (pFromField->getType().match(rfk::getType<unsigned int>())) {
            auto value = pFromField->getUnsafe<unsigned int>(pFromInstance);
            pToField->setUnsafe<unsigned int>(pToInstance, std::move(value)); // NOLINT
        } else if (pFromField->getType().match(rfk::getType<long long>())) {
            auto value = pFromField->getUnsafe<long long>(pFromInstance);
            pToField->setUnsafe<long long>(pToInstance, std::move(value)); // NOLINT
        } else if (pFromField->getType().match(rfk::getType<unsigned long long>())) {
            auto value = pFromField->getUnsafe<unsigned long long>(pFromInstance);
            pToField->setUnsafe<unsigned long long>(pToInstance, std::move(value)); // NOLINT
        } else if (pFromField->getType().match(rfk::getType<float>())) {
            auto value = pFromField->getUnsafe<float>(pFromInstance);
            pToField->setUnsafe<float>(pToInstance, std::move(value)); // NOLINT
        } else if (pFromField->getType().match(rfk::getType<double>())) {
            auto value = pFromField->getUnsafe<double>(pFromInstance);
            pToField->setUnsafe<double>(pToInstance, std::move(value)); // NOLINT
        } else {
            return Error(std::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                pFromField->getCanonicalTypeName(),
                pFromField->getName()));
        }

        return {};
    }

    bool PrimitiveFieldSerializer::isFieldValueEqual(
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

        // Check that types are equal.
        const std::string sFieldACanonicalTypeName = pFieldA->getCanonicalTypeName();
        const std::string sFieldBCanonicalTypeName = pFieldB->getCanonicalTypeName();
        if (sFieldACanonicalTypeName != sFieldBCanonicalTypeName) {
            return false;
        }

        const auto& fieldAType = pFieldA->getType();

        // The `match` function can only be used with the primitive types.
        if (fieldAType.match(rfk::getType<bool>())) {
            const auto fieldAValue = pFieldA->getUnsafe<bool>(pFieldAOwner);
            const auto fieldBValue = pFieldB->getUnsafe<bool>(pFieldBOwner);
            return fieldAValue == fieldBValue;
        }
        if (fieldAType.match(rfk::getType<int>())) {
            const auto fieldAValue = pFieldA->getUnsafe<int>(pFieldAOwner);
            const auto fieldBValue = pFieldB->getUnsafe<int>(pFieldBOwner);
            return fieldAValue == fieldBValue;
        }
        if (fieldAType.match(rfk::getType<unsigned int>())) {
            const auto fieldAValue = pFieldA->getUnsafe<unsigned int>(pFieldAOwner);
            const auto fieldBValue = pFieldB->getUnsafe<unsigned int>(pFieldBOwner);
            return fieldAValue == fieldBValue;
        }
        if (fieldAType.match(rfk::getType<long long>())) {
            const auto fieldAValue = pFieldA->getUnsafe<long long>(pFieldAOwner);
            const auto fieldBValue = pFieldB->getUnsafe<long long>(pFieldBOwner);
            return fieldAValue == fieldBValue;
        }
        if (fieldAType.match(rfk::getType<unsigned long long>())) {
            const auto fieldAValue = pFieldA->getUnsafe<unsigned long long>(pFieldAOwner);
            const auto fieldBValue = pFieldB->getUnsafe<unsigned long long>(pFieldBOwner);
            return fieldAValue == fieldBValue;
        }
        if (fieldAType.match(rfk::getType<float>())) {
            constexpr auto floatDelta = 0.00001F; // NOLINT
            const auto fieldAValue = pFieldA->getUnsafe<float>(pFieldAOwner);
            const auto fieldBValue = pFieldB->getUnsafe<float>(pFieldBOwner);
            return fabs(fieldAValue - fieldBValue) < floatDelta;
        }
        if (fieldAType.match(rfk::getType<double>())) {
            constexpr auto doubleDelta = 0.0000000000001; // NOLINT
            const auto fieldAValue = pFieldA->getUnsafe<double>(pFieldAOwner);
            const auto fieldBValue = pFieldB->getUnsafe<double>(pFieldBOwner);
            return fabs(fieldAValue - fieldBValue) < doubleDelta;
        }

        return false;
    }
} // namespace ne
