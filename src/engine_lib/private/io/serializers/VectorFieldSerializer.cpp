#include "io/serializers/VectorFieldSerializer.h"

// External.
#include "fmt/format.h"

namespace ne {
    bool VectorFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            return true;
        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            return true;
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned int>") {
            return true;
        } else if (sFieldCanonicalTypeName == "std::vector<long long>") {
            return true;
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            return true;
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            return true;
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            return true;
        }

        return false;
    }

    std::optional<Error> VectorFieldSerializer::serializeField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<bool>>(pFieldOwner);
        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<int>>(pFieldOwner);
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned int>") {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<unsigned int>>(pFieldOwner);
        } else if (sFieldCanonicalTypeName == "std::vector<long long>") {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<long long>>(pFieldOwner);
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<float>>(pFieldOwner);
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            const std::vector<double> vArray = pField->getUnsafe<std::vector<double>>(pFieldOwner);
            // Store double as string for better precision.
            std::vector<std::string> vStrArray;
            for (const auto& item : vArray) {
                vStrArray.push_back(toml::format(toml::value(item)));
            }
            pTomlData->operator[](sSectionName).operator[](sFieldName) = vStrArray;
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<std::string>>(pFieldOwner);
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        return {};
    }

    std::optional<Error> VectorFieldSerializer::deserializeField(
        const toml::value* pTomlDocument,
        const toml::value* pTomlValue,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sOwnerSectionName,
        const std::string& sEntityId,
        std::unordered_map<std::string, std::string>& customAttributes) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        if (!pTomlValue->is_array()) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                "but the TOML value is not an array.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        auto fieldValue = pTomlValue->as_array();

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            std::vector<bool> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_boolean()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not boolean.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                vArray.push_back(item.as_boolean());
            }
            pField->setUnsafe<std::vector<bool>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            std::vector<int> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_integer()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not integer.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                vArray.push_back(static_cast<int>(item.as_integer()));
            }
            pField->setUnsafe<std::vector<int>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned int>") {
            std::vector<unsigned int> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_integer()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not integer.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                const auto iOriginalValue = item.as_integer();
                auto fieldValue = static_cast<unsigned int>(iOriginalValue);
                if (iOriginalValue < 0) {
                    // Since integers are stored as `long long` in toml11 library that we use,
                    // we add this check.
                    fieldValue = 0;
                }
                vArray.push_back(fieldValue);
            }
            pField->setUnsafe<std::vector<unsigned int>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<long long>") {
            std::vector<long long> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_integer()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not integer.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                vArray.push_back(item.as_integer());
            }
            pField->setUnsafe<std::vector<long long>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            std::vector<float> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_floating()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not floating.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                vArray.push_back(static_cast<float>(item.as_floating()));
            }
            pField->setUnsafe<std::vector<float>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            // We are storing double as a string for better precision.
            std::vector<double> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_string()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not string.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                try {
                    vArray.push_back(std::stod(item.as_string().str));
                } catch (std::exception& ex) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but an exception occurred while trying to convert a string to a double: {}",
                        sFieldCanonicalTypeName,
                        sFieldName,
                        ex.what()));
                }
            }
            pField->setUnsafe<std::vector<double>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            std::vector<std::string> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_string()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not string.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                vArray.push_back(item.as_string());
            }
            pField->setUnsafe<std::vector<std::string>>(pFieldOwner, std::move(vArray));
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        return {};
    }

    std::optional<Error> VectorFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());
        const auto sFieldName = pFromField->getName();

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            auto value = pFromField->getUnsafe<std::vector<bool>>(pFromInstance);
            pToField->setUnsafe<std::vector<bool>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            auto value = pFromField->getUnsafe<std::vector<int>>(pFromInstance);
            pToField->setUnsafe<std::vector<int>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned int>") {
            auto value = pFromField->getUnsafe<std::vector<unsigned int>>(pFromInstance);
            pToField->setUnsafe<std::vector<unsigned int>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == "std::vector<long long>") {
            auto value = pFromField->getUnsafe<std::vector<long long>>(pFromInstance);
            pToField->setUnsafe<std::vector<long long>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            auto value = pFromField->getUnsafe<std::vector<float>>(pFromInstance);
            pToField->setUnsafe<std::vector<float>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            auto value = pFromField->getUnsafe<std::vector<double>>(pFromInstance);
            pToField->setUnsafe<std::vector<double>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            auto value = pFromField->getUnsafe<std::vector<std::string>>(pFromInstance);
            pToField->setUnsafe<std::vector<std::string>>(pToInstance, std::move(value));
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        return {};
    }
} // namespace ne
