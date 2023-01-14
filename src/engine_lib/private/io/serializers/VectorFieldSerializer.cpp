#include "io/serializers/VectorFieldSerializer.h"

// Custom.
#include "game/nodes/MeshNode.h"

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
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned long long>") {
            return true;
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            return true;
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            return true;
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            return true;
        } else if (sFieldCanonicalTypeName == "std::vector<ne::MeshVertex>") {
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
        size_t& iSubEntityId,
        Serializable* pOriginalObject) {
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
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned long long>") {
            const auto vOriginalArray = pField->getUnsafe<std::vector<unsigned long long>>(pFieldOwner);
            std::vector<std::string> vArray;
            for (const auto& iItem : vOriginalArray) {
                // Since toml11 (library that we use) uses `long long` for integers,
                // store this type as a string.
                const std::string sValue = std::to_string(iItem);
                vArray.push_back(sValue);
            }
            pTomlData->operator[](sSectionName).operator[](sFieldName) = vArray;
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
        } else if (sFieldCanonicalTypeName == "std::vector<ne::MeshVertex>") {
            MeshVertex::serializeVec(
                reinterpret_cast<std::vector<MeshVertex>*>(pField->getPtrUnsafe(pFieldOwner)),
                &pTomlData->operator[](sSectionName),
                sFieldName);
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

        if (sFieldCanonicalTypeName != "std::vector<ne::MeshVertex>" && !pTomlValue->is_array()) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                "but the TOML value is not an array.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            auto fieldValue = pTomlValue->as_array();
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
            auto fieldValue = pTomlValue->as_array();
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
            auto fieldValue = pTomlValue->as_array();
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
            auto fieldValue = pTomlValue->as_array();
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
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned long long>") {
            auto fieldValue = pTomlValue->as_array();
            std::vector<unsigned long long> vArray;
            for (const auto& item : fieldValue) {
                // Stored as string.
                if (!item.is_string()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not string.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                try {
                    unsigned long long iValue = std::stoull(item.as_string());
                    vArray.push_back(iValue);
                } catch (std::exception& ex) {
                    return Error(fmt::format(
                        "Failed to convert string to unsigned long long for field \"{}\": {}",
                        sFieldName,
                        ex.what()));
                }
            }
            pField->setUnsafe<std::vector<unsigned long long>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            auto fieldValue = pTomlValue->as_array();
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
            auto fieldValue = pTomlValue->as_array();
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
            auto fieldValue = pTomlValue->as_array();
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
        } else if (sFieldCanonicalTypeName == "std::vector<ne::MeshVertex>") {
            auto optionalError = MeshVertex::deserializeVec(
                reinterpret_cast<std::vector<MeshVertex>*>(pField->getPtrUnsafe(pFieldOwner)), pTomlValue);
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addEntry();
                return error;
            }
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
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned long long>") {
            auto value = pFromField->getUnsafe<std::vector<unsigned long long>>(pFromInstance);
            pToField->setUnsafe<std::vector<unsigned long long>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            auto value = pFromField->getUnsafe<std::vector<float>>(pFromInstance);
            pToField->setUnsafe<std::vector<float>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            auto value = pFromField->getUnsafe<std::vector<double>>(pFromInstance);
            pToField->setUnsafe<std::vector<double>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            auto value = pFromField->getUnsafe<std::vector<std::string>>(pFromInstance);
            pToField->setUnsafe<std::vector<std::string>>(pToInstance, std::move(value));
        } else if (sFieldCanonicalTypeName == "std::vector<ne::MeshVertex>") {
            auto value = pFromField->getUnsafe<std::vector<MeshVertex>>(pFromInstance);
            pToField->setUnsafe<std::vector<MeshVertex>>(pToInstance, std::move(value));
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        return {};
    }

    bool VectorFieldSerializer::isFieldValueEqual(
        Serializable* pFieldAOwner,
        const rfk::Field* pFieldA,
        Serializable* pFieldBOwner,
        const rfk::Field* pFieldB) {
        if (!isFieldTypeSupported(pFieldA))
            return false;
        if (!isFieldTypeSupported(pFieldB))
            return false;

        // Check that types are equal.
        const std::string sFieldACanonicalTypeName = pFieldA->getCanonicalTypeName();
        const std::string sFieldBCanonicalTypeName = pFieldB->getCanonicalTypeName();
        if (sFieldACanonicalTypeName != sFieldBCanonicalTypeName) {
            return false;
        }

#define COMPARE_VECTOR_FIELDS(INNERTYPE)                                                                     \
    if (sFieldACanonicalTypeName == fmt::format("std::vector<{}>", #INNERTYPE)) {                            \
        const auto vA = pFieldA->getUnsafe<std::vector<INNERTYPE>>(pFieldAOwner);                            \
        const auto vB = pFieldB->getUnsafe<std::vector<INNERTYPE>>(pFieldBOwner);                            \
        return vA == vB;                                                                                     \
    }

        COMPARE_VECTOR_FIELDS(bool)
        COMPARE_VECTOR_FIELDS(int)
        COMPARE_VECTOR_FIELDS(unsigned int)
        COMPARE_VECTOR_FIELDS(long long)
        COMPARE_VECTOR_FIELDS(unsigned long long)
        COMPARE_VECTOR_FIELDS(float)
        COMPARE_VECTOR_FIELDS(double)
        COMPARE_VECTOR_FIELDS(std::basic_string<char>)
        COMPARE_VECTOR_FIELDS(ne::MeshVertex)

        return false;
    }
} // namespace ne
