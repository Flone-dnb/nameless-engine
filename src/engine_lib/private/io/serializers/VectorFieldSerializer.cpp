#include "io/serializers/VectorFieldSerializer.h"

// Standard.
#include <format>

// Custom.
#include "game/nodes/MeshNode.h"
#include "io/serializers/SerializableObjectFieldSerializer.h"

namespace ne {

    bool VectorFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            return true;
        }
        if (sFieldCanonicalTypeName == "std::vector<int>") {
            return true;
        }
        if (sFieldCanonicalTypeName == "std::vector<unsigned int>") {
            return true;
        }
        if (sFieldCanonicalTypeName == "std::vector<long long>") {
            return true;
        }
        if (sFieldCanonicalTypeName == "std::vector<unsigned long long>") {
            return true;
        }
        if (sFieldCanonicalTypeName == "std::vector<float>") {
            return true;
        }
        if (sFieldCanonicalTypeName == "std::vector<double>") {
            return true;
        }
        if (sFieldCanonicalTypeName == std::format("std::vector<{}>", sStringCanonicalTypeName)) {
            return true;
        }
        if (sFieldCanonicalTypeName == "std::vector<std::vector<unsigned int>>") {
            return true;
        }
        if (sFieldCanonicalTypeName.starts_with("std::vector<") &&
            isMostInnerTypeSerializable(sFieldCanonicalTypeName)) {
            return sFieldCanonicalTypeName.contains("std::shared_ptr<");
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
        const auto pFieldName = pField->getName();

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<std::vector<bool>>(pFieldOwner);

        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<std::vector<int>>(pFieldOwner);

        } else if (sFieldCanonicalTypeName == "std::vector<unsigned int>") {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<std::vector<unsigned int>>(pFieldOwner);

        } else if (sFieldCanonicalTypeName == "std::vector<long long>") {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<std::vector<long long>>(pFieldOwner);

        } else if (sFieldCanonicalTypeName == "std::vector<unsigned long long>") {
            const auto vOriginalArray = pField->getUnsafe<std::vector<unsigned long long>>(pFieldOwner);
            std::vector<std::string> vArray(vOriginalArray.size());
            for (size_t i = 0; i < vOriginalArray.size(); i++) {
                // Since toml11 (library that we use) uses `long long` for integers,
                // store this type as a string.
                vArray[i] = std::to_string(vOriginalArray[i]);
            }
            pTomlData->operator[](sSectionName).operator[](pFieldName) = vArray;

        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            const std::vector<float> vArray = pField->getUnsafe<std::vector<float>>(pFieldOwner);
            // Store float as string for better precision.
            std::vector<std::string> vStrArray(vArray.size());
            for (size_t i = 0; i < vArray.size(); i++) {
                vStrArray[i] = toml::format(toml::value(vArray[i]));
            }
            pTomlData->operator[](sSectionName).operator[](pFieldName) = vStrArray;

        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            const std::vector<double> vArray = pField->getUnsafe<std::vector<double>>(pFieldOwner);
            // Store double as string for better precision.
            std::vector<std::string> vStrArray(vArray.size());
            for (size_t i = 0; i < vArray.size(); i++) {
                vStrArray[i] = toml::format(toml::value(vArray[i]));
            }
            pTomlData->operator[](sSectionName).operator[](pFieldName) = vStrArray;

        } else if (sFieldCanonicalTypeName == std::format("std::vector<{}>", sStringCanonicalTypeName)) {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<std::vector<std::string>>(pFieldOwner);

        } else if (sFieldCanonicalTypeName == "std::vector<std::vector<unsigned int>>") {
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                pField->getUnsafe<std::vector<std::vector<unsigned int>>>(pFieldOwner);

        } else if (
            sFieldCanonicalTypeName.starts_with("std::vector<") &&
            isMostInnerTypeSerializable(sFieldCanonicalTypeName)) {
// Define a helper macro.
#define SERIALIZE_VECTOR_INNER_POINTER_SERIALIZABLE                                                          \
    toml::table table;                                                                                       \
    for (size_t i = 0; i < pArray->size(); i++) {                                                            \
        const auto& pSerializable = pArray->at(i);                                                           \
        const auto pGuid = pSerializable->getArchetype().getProperty<Guid>(false);                           \
        if (pGuid == nullptr) [[unlikely]] {                                                                 \
            return Error(std::format(                                                                        \
                "type \"{}\" should have a GUID assigned to it", pSerializable->getArchetype().getName()));  \
        }                                                                                                    \
        toml::value data;                                                                                    \
        auto result = pSerializable->serialize(data);                                                        \
        if (std::holds_alternative<Error>(result)) [[unlikely]] {                                            \
            auto error = std::get<Error>(std::move(result));                                                 \
            error.addCurrentLocationToErrorStack();                                                          \
            return error;                                                                                    \
        }                                                                                                    \
        table[std::format("{}[{}]", pFieldName, i)] = std::move(data);                                       \
    }                                                                                                        \
    pTomlData->operator[](sSectionName).operator[](pFieldName) = std::move(table);

            if (sFieldCanonicalTypeName.contains("std::shared_ptr<")) {
                const auto pArray = reinterpret_cast<std::vector<std::shared_ptr<Serializable>>*>(
                    pField->getPtrUnsafe(pFieldOwner));
                SERIALIZE_VECTOR_INNER_POINTER_SERIALIZABLE
            } else [[unlikely]] {
                return Error(std::format(
                    "the type \"{}\" of the specified array field \"{}\" has unsupported smart pointer type",
                    sFieldCanonicalTypeName,
                    pFieldName));
            }
        } else [[unlikely]] {
            return Error(std::format(
                "the type \"{}\" of the specified field \"{}\" is not supported by this serializer",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        return {};
    } // namespace ne

    std::optional<Error> VectorFieldSerializer::deserializeField( // NOLINT: too complex
        const toml::value* pTomlDocument,
        const toml::value* pTomlValue,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sOwnerSectionName,
        const std::string& sEntityId,
        std::unordered_map<std::string, std::string>& customAttributes) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto pFieldName = pField->getName();

#define GET_TOML_VALUE_AS_ARRAY_WITH_CHECK                                                                   \
    if (!pTomlValue->is_array()) [[unlikely]] {                                                              \
        return Error(std::format(                                                                            \
            "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "                \
            "but the TOML value is not an array.",                                                           \
            sFieldCanonicalTypeName,                                                                         \
            pFieldName));                                                                                    \
    }                                                                                                        \
    auto fieldValue = pTomlValue->as_array();

#define GET_TOML_VALUE_AS_TABLE_WITH_CHECK                                                                   \
    if (!pTomlValue->is_table()) [[unlikely]] {                                                              \
        return Error(std::format(                                                                            \
            "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "                \
            "but the TOML value is not a table.",                                                            \
            sFieldCanonicalTypeName,                                                                         \
            pFieldName));                                                                                    \
    }                                                                                                        \
    auto& fieldValue = pTomlValue->as_table();

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            std::vector<bool> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_boolean()) [[unlikely]] {
                    return Error(std::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not boolean.",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }
                vArray.push_back(item.as_boolean());
            }
            pField->setUnsafe<std::vector<bool>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            std::vector<int> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_integer()) [[unlikely]] {
                    return Error(std::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not integer.",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }
                vArray.push_back(static_cast<int>(item.as_integer()));
            }
            pField->setUnsafe<std::vector<int>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned int>") {
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            std::vector<unsigned int> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_integer()) [[unlikely]] {
                    return Error(std::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not integer.",
                        sFieldCanonicalTypeName,
                        pFieldName));
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
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            std::vector<long long> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_integer()) [[unlikely]] {
                    return Error(std::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not integer.",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }
                vArray.push_back(item.as_integer());
            }
            pField->setUnsafe<std::vector<long long>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<unsigned long long>") {
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            std::vector<unsigned long long> vArray;
            for (const auto& item : fieldValue) {
                // Stored as string.
                if (!item.is_string()) [[unlikely]] {
                    return Error(std::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not string.",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }
                try {
                    unsigned long long iValue = std::stoull(item.as_string());
                    vArray.push_back(iValue);
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "Failed to convert string to unsigned long long for field \"{}\": {}",
                        pFieldName,
                        ex.what()));
                }
            }
            pField->setUnsafe<std::vector<unsigned long long>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            // We are storing float as a string for better precision.
            std::vector<float> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_string()) [[unlikely]] {
                    return Error(std::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not string.",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }
                try {
                    vArray.push_back(std::stof(item.as_string().str));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but an exception occurred while trying to convert a string to a float: {}",
                        sFieldCanonicalTypeName,
                        pFieldName,
                        ex.what()));
                }
            }
            pField->setUnsafe<std::vector<float>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            // We are storing double as a string for better precision.
            std::vector<double> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_string()) [[unlikely]] {
                    return Error(std::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not string.",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }
                try {
                    vArray.push_back(std::stod(item.as_string().str));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but an exception occurred while trying to convert a string to a double: {}",
                        sFieldCanonicalTypeName,
                        pFieldName,
                        ex.what()));
                }
            }
            pField->setUnsafe<std::vector<double>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == std::format("std::vector<{}>", sStringCanonicalTypeName)) {
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            std::vector<std::string> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_string()) [[unlikely]] {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not string",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }
                vArray.push_back(item.as_string());
            }
            pField->setUnsafe<std::vector<std::string>>(pFieldOwner, std::move(vArray));
        } else if (sFieldCanonicalTypeName == "std::vector<std::vector<unsigned int>>") {
            GET_TOML_VALUE_AS_ARRAY_WITH_CHECK
            std::vector<std::vector<unsigned int>> vDoubleArray;
            for (const auto& item : fieldValue) {
                if (!item.is_array()) [[unlikely]] {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not array",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }
                auto innerArray = item.as_array();
                std::vector<unsigned int> vInnerArray;
                for (const auto& innerItem : innerArray) {
                    if (!innerItem.is_integer()) [[unlikely]] {
                        return Error(std::format(
                            "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                            "but the TOML value is not integer",
                            sFieldCanonicalTypeName,
                            pFieldName));
                    }
                    vInnerArray.push_back(static_cast<unsigned int>(innerItem.as_integer()));
                }
                vDoubleArray.push_back(std::move(vInnerArray));
            }
            pField->setUnsafe<std::vector<std::vector<unsigned int>>>(pFieldOwner, std::move(vDoubleArray));
        } else if (
            sFieldCanonicalTypeName.starts_with("std::vector<") &&
            isMostInnerTypeSerializable(sFieldCanonicalTypeName)) {
            GET_TOML_VALUE_AS_TABLE_WITH_CHECK

            if (sFieldCanonicalTypeName.contains("std::shared_ptr<")) {
                const auto pArray = reinterpret_cast<std::vector<std::shared_ptr<Serializable>>*>(
                    pField->getPtrUnsafe(pFieldOwner));

                // Make sure target array is empty.
                if (!pArray->empty()) {
                    pArray->clear();
                }

                for (const auto& [key, value] : fieldValue) {
                    auto result =
                        Serializable::deserialize<std::shared_ptr, Serializable>(value, customAttributes);
                    if (std::holds_alternative<Error>(result)) {
                        auto error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        return error;
                    }

                    pArray->push_back(std::get<std::shared_ptr<Serializable>>(std::move(result)));
                }
            } else [[unlikely]] {
                return Error(std::format(
                    "the type \"{}\" of the specified array field \"{}\" has unsupported smart pointer type",
                    sFieldCanonicalTypeName,
                    pFieldName));
            }
        } else [[unlikely]] {
            return Error(std::format(
                "the type \"{}\" of the specified field \"{}\" is not supported by this serializer",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        return {};
    }

    std::optional<Error> VectorFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());
        const auto pFieldName = pFromField->getName();

#define CLONE_VECTOR_FIELDS(INNERTYPE)                                                                       \
    if (sFieldCanonicalTypeName == #INNERTYPE) {                                                             \
        auto value = pFromField->getUnsafe<INNERTYPE>(pFromInstance);                                        \
        pToField->setUnsafe<INNERTYPE>(pToInstance, std::move(value));                                       \
        return {};                                                                                           \
    }

        CLONE_VECTOR_FIELDS(std::vector<bool>)
        CLONE_VECTOR_FIELDS(std::vector<int>)
        CLONE_VECTOR_FIELDS(std::vector<unsigned int>)
        CLONE_VECTOR_FIELDS(std::vector<long long>)
        CLONE_VECTOR_FIELDS(std::vector<unsigned long long>)
        CLONE_VECTOR_FIELDS(std::vector<float>)
        CLONE_VECTOR_FIELDS(std::vector<double>)
        CLONE_VECTOR_FIELDS(std::vector<std::vector<unsigned int>>)

        // String.
        if (sFieldCanonicalTypeName == std::format("std::vector<{}>", sStringCanonicalTypeName)) {
            auto value = pFromField->getUnsafe<std::vector<std::string>>(pFromInstance);
            pToField->setUnsafe<std::vector<std::string>>(pToInstance, std::move(value));
            return {};
        }

        // Serializable pointer.
        if (sFieldCanonicalTypeName.starts_with("std::vector<") &&
            isMostInnerTypeSerializable(sFieldCanonicalTypeName)) {
#define CLONE_VECTOR_SMART_POINTER_SERIALIZABLE_FIELDS(VectorType, MakeInstanceFunctionName)                 \
    const auto pFromArray = reinterpret_cast<VectorType>(pFromField->getPtrUnsafe(pFromInstance));           \
    const auto pToArray = reinterpret_cast<VectorType>(pToField->getPtrUnsafe(pToInstance));                 \
    if (!pToArray->empty()) [[unlikely]] {                                                                   \
        return Error(std::format(                                                                            \
            "the specified field array \"{}\" needs to be empty", sFieldCanonicalTypeName, pFieldName));     \
    }                                                                                                        \
    pToArray->resize(pFromArray->size());                                                                    \
    for (size_t i = 0; i < pFromArray->size(); i++) {                                                        \
        pToArray->at(i) = pFromArray->at(i)->getArchetype().MakeInstanceFunctionName<Serializable>();        \
        const auto optionalError = SerializableObjectFieldSerializer::cloneSerializableObject(               \
            pFromArray->at(i).get(), pToArray->at(i).get(), true);                                           \
        if (optionalError.has_value()) {                                                                     \
            auto error = optionalError.value();                                                              \
            error.addCurrentLocationToErrorStack();                                                          \
            return error;                                                                                    \
        }                                                                                                    \
    }

            if (sFieldCanonicalTypeName.contains("std::shared_ptr<")) {
                CLONE_VECTOR_SMART_POINTER_SERIALIZABLE_FIELDS(
                    std::vector<std::shared_ptr<Serializable>>*, makeSharedInstance)
            } else [[unlikely]] {
                return Error(std::format(
                    "the type \"{}\" of the specified array field \"{}\" has unsupported smart pointer type",
                    sFieldCanonicalTypeName,
                    pFieldName));
            }
            return {};
        }

        return Error(std::format(
            "the type \"{}\" of the specified field \"{}\" is not supported by this serializer",
            sFieldCanonicalTypeName,
            pFieldName));
    }

    bool VectorFieldSerializer::isFieldValueEqual(
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

#define COMPARE_VECTOR_FIELDS(INNERTYPE)                                                                     \
    if (sFieldACanonicalTypeName == std::format("std::vector<{}>", #INNERTYPE)) {                            \
        const auto vFieldA = pFieldA->getUnsafe<std::vector<INNERTYPE>>(pFieldAOwner);                       \
        const auto vFieldB = pFieldB->getUnsafe<std::vector<INNERTYPE>>(pFieldBOwner);                       \
        return vFieldA == vFieldB;                                                                           \
    }

        COMPARE_VECTOR_FIELDS(bool)
        COMPARE_VECTOR_FIELDS(int)
        COMPARE_VECTOR_FIELDS(unsigned int)
        COMPARE_VECTOR_FIELDS(long long)
        COMPARE_VECTOR_FIELDS(unsigned long long)
        COMPARE_VECTOR_FIELDS(float)
        COMPARE_VECTOR_FIELDS(double)
        COMPARE_VECTOR_FIELDS(std::basic_string<char>)
        COMPARE_VECTOR_FIELDS(std::vector<unsigned int>)

        // Serializable pointers.
        if (sFieldACanonicalTypeName.starts_with("std::vector<") &&
            isMostInnerTypeSerializable(sFieldACanonicalTypeName)) {

#define COMPARE_VECTOR_SMART_POINTER_SERIALIZABLE_FIELDS(ArrayType)                                          \
    const auto pArrayA = reinterpret_cast<ArrayType>(pFieldA->getPtrUnsafe(pFieldAOwner));                   \
    const auto pArrayB = reinterpret_cast<ArrayType>(pFieldB->getPtrUnsafe(pFieldBOwner));                   \
    if (pArrayA->size() != pArrayB->size()) {                                                                \
        return false;                                                                                        \
    }                                                                                                        \
    for (size_t i = 0; i < pArrayA->size(); i++) {                                                           \
        if (!SerializableObjectFieldSerializer::isSerializableObjectValueEqual(                              \
                pArrayA->at(i).get(), pArrayB->at(i).get())) {                                               \
            return false;                                                                                    \
        }                                                                                                    \
    }                                                                                                        \
    return true;

            if (sFieldACanonicalTypeName.contains("std::shared_ptr<")) {
                COMPARE_VECTOR_SMART_POINTER_SERIALIZABLE_FIELDS(std::vector<std::shared_ptr<Serializable>>*)
            }
        }

        return false;
    }

    bool VectorFieldSerializer::isMostInnerTypeSerializable(const std::string& sFieldCanonicalTypeName) {
        // Get position of the first '<' occurrence.
        auto iFoundInnerTypeStartPosition = sFieldCanonicalTypeName.find('<');
        if (iFoundInnerTypeStartPosition == std::string::npos) {
            return false;
        }

        // Get position of the second '<' occurrence.
        iFoundInnerTypeStartPosition = sFieldCanonicalTypeName.find('<', iFoundInnerTypeStartPosition + 1);
        if (iFoundInnerTypeStartPosition == std::string::npos) {
            return false;
        }
        iFoundInnerTypeStartPosition += 1;

        // Find '>' character now.
        const auto iFoundInnerTypeStopPosition =
            sFieldCanonicalTypeName.find('>', iFoundInnerTypeStartPosition + 1);
        if (iFoundInnerTypeStopPosition == std::string::npos) {
            return false;
        }

        // Make sure found positions are valid.
        if (iFoundInnerTypeStartPosition >= iFoundInnerTypeStopPosition) {
            return false;
        }

        // Cut inner type.
        const auto sInnerTypeCanonicalName = sFieldCanonicalTypeName.substr(
            iFoundInnerTypeStartPosition, iFoundInnerTypeStopPosition - iFoundInnerTypeStartPosition);

        return SerializableObjectFieldSerializer::isTypeDerivesFromSerializable(sInnerTypeCanonicalName);
    }

} // namespace ne
