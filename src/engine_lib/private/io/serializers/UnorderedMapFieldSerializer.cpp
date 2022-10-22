#include "io/serializers/UnorderedMapFieldSerializer.h"

// External.
#include "fmt/format.h"

namespace ne {
    bool UnorderedMapFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        // Define a helper macro...
#define SUPPORTED_UNORDERED_MAP_TYPES(TYPEA, TYPEB)                                                          \
    if (sFieldCanonicalTypeName == fmt::format("std::unordered_map<{}, {}>", #TYPEA, #TYPEB)) {              \
        return true;                                                                                         \
    }

        // and another helper macro.
#define SUPPORTED_UNORDERED_MAP_TYPE(TYPE)                                                                   \
    SUPPORTED_UNORDERED_MAP_TYPES(TYPE, bool)                                                                \
    SUPPORTED_UNORDERED_MAP_TYPES(TYPE, int)                                                                 \
    SUPPORTED_UNORDERED_MAP_TYPES(TYPE, long long)                                                           \
    SUPPORTED_UNORDERED_MAP_TYPES(TYPE, float)                                                               \
    SUPPORTED_UNORDERED_MAP_TYPES(TYPE, double)                                                              \
    SUPPORTED_UNORDERED_MAP_TYPES(TYPE, std::basic_string<char>)

        SUPPORTED_UNORDERED_MAP_TYPE(bool)
        SUPPORTED_UNORDERED_MAP_TYPE(int)
        SUPPORTED_UNORDERED_MAP_TYPE(long long)
        SUPPORTED_UNORDERED_MAP_TYPE(float)
        SUPPORTED_UNORDERED_MAP_TYPE(double)
        SUPPORTED_UNORDERED_MAP_TYPE(std::basic_string<char>)

        return false;
    }

    std::optional<Error> UnorderedMapFieldSerializer::serializeField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        // Define a helper macro...
#define SERIALIZE_UNORDERED_MAP_TYPE(TYPEA, TYPEB)                                                           \
    if (sFieldCanonicalTypeName == fmt::format("std::unordered_map<{}, {}>", #TYPEA, #TYPEB)) {              \
        const auto originalMap = pField->getUnsafe<std::unordered_map<TYPEA, TYPEB>>(pFieldOwner);           \
        if (std::string(#TYPEB) != "double") {                                                               \
            std::unordered_map<std::string, TYPEB> map;                                                      \
            for (const auto& [key, value] : originalMap) {                                                   \
                map[fmt::format("{}", key)] = value;                                                         \
            }                                                                                                \
            pTomlData->operator[](sSectionName).operator[](sFieldName) = map;                                \
        } else {                                                                                             \
            std::unordered_map<std::string, std::string> map;                                                \
            for (const auto& [key, value] : originalMap) {                                                   \
                map[fmt::format("{}", key)] = fmt::format("{}", value);                                      \
            }                                                                                                \
            pTomlData->operator[](sSectionName).operator[](sFieldName) = map;                                \
        }                                                                                                    \
        return {};                                                                                           \
    }
        // and another helper macro.
#define SERIALIZE_UNORDERED_MAP_TYPES(TYPE)                                                                  \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, bool)                                                                 \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, int)                                                                  \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, long long)                                                            \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, float)                                                                \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, double)                                                               \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, std::basic_string<char>)

        SERIALIZE_UNORDERED_MAP_TYPES(bool)
        SERIALIZE_UNORDERED_MAP_TYPES(int)
        SERIALIZE_UNORDERED_MAP_TYPES(long long)
        SERIALIZE_UNORDERED_MAP_TYPES(float)
        SERIALIZE_UNORDERED_MAP_TYPES(double)
        SERIALIZE_UNORDERED_MAP_TYPES(std::basic_string<char>)

        return Error(fmt::format(
            "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
            sFieldCanonicalTypeName,
            sFieldName));
    } // namespace ne

    // ------------------------------------------------------------------------------------------------

    template <typename T> std::optional<T> convertStringToType(const std::string& sText) { return {}; }

    template <> std::optional<bool> convertStringToType<bool>(const std::string& sText) {
        if (sText == "true")
            return true;
        else
            return false;
    }

    template <> std::optional<int> convertStringToType<int>(const std::string& sText) {
        try {
            return std::stoi(sText);
        } catch (...) {
            return {};
        }
    }

    template <> std::optional<long long> convertStringToType<long long>(const std::string& sText) {
        try {
            return std::stoll(sText);
        } catch (...) {
            return {};
        }
    }

    template <> std::optional<float> convertStringToType<float>(const std::string& sText) {
        try {
            return std::stof(sText);
        } catch (...) {
            return {};
        }
    }

    template <> std::optional<double> convertStringToType<double>(const std::string& sText) {
        try {
            return std::stod(sText);
        } catch (...) {
            return {};
        }
    }

    template <> std::optional<std::string> convertStringToType<std::string>(const std::string& sText) {
        return sText;
    }

    // ------------------------------------------------------------------------------------------------

    template <typename T> std::optional<T> convertTomlValueToType(const toml::value& value) { return {}; }

    template <> std::optional<bool> convertTomlValueToType<bool>(const toml::value& value) {
        if (!value.is_boolean()) {
            return {};
        }
        return value.as_boolean();
    }

    template <> std::optional<int> convertTomlValueToType<int>(const toml::value& value) {
        if (!value.is_integer()) {
            return {};
        }
        return static_cast<int>(value.as_integer());
    }

    template <> std::optional<long long> convertTomlValueToType<long long>(const toml::value& value) {
        if (!value.is_integer()) {
            return {};
        }
        return value.as_integer();
    }

    template <> std::optional<float> convertTomlValueToType<float>(const toml::value& value) {
        if (!value.is_floating()) {
            return {};
        }
        return static_cast<float>(value.as_floating());
    }

    template <> std::optional<double> convertTomlValueToType<double>(const toml::value& value) {
        if (value.is_floating())
            return value.as_floating();
        else if (value.is_string())
            try {
                return std::stod(value.as_string());
            } catch (...) {
                return {};
            }

        return {};
    }

    template <> std::optional<std::string> convertTomlValueToType<std::string>(const toml::value& value) {
        if (!value.is_string()) {
            return {};
        }
        return value.as_string().str;
    }

    // ------------------------------------------------------------------------------------------------

    std::optional<Error> UnorderedMapFieldSerializer::deserializeField(
        const toml::value* pTomlDocument,
        const toml::value* pTomlValue,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sOwnerSectionName,
        const std::string& sEntityId,
        std::unordered_map<std::string, std::string>& customAttributes) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        if (!pTomlValue->is_table()) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                "but the TOML value is not a table.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

// Define another helper macro...
#define DESERIALIZE_UNORDERED_MAP_TYPE(TYPEA, TYPEB)                                                         \
    if (sFieldCanonicalTypeName == fmt::format("std::unordered_map<{}, {}>", #TYPEA, #TYPEB)) {              \
        auto fieldValue = pTomlValue->as_table();                                                            \
        std::unordered_map<TYPEA, TYPEB> map;                                                                \
        for (const auto& [sKey, value] : fieldValue) {                                                       \
            TYPEA castedKey;                                                                                 \
            const auto optionalKey = convertStringToType<TYPEA>(sKey);                                       \
            if (!optionalKey.has_value()) {                                                                  \
                return Error(fmt::format(                                                                    \
                    "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "        \
                    "but deserializer failed to convert string to type {}",                                  \
                    sFieldCanonicalTypeName,                                                                 \
                    sFieldName,                                                                              \
                    #TYPEA));                                                                                \
            }                                                                                                \
            castedKey = optionalKey.value();                                                                 \
            TYPEB castedValue;                                                                               \
            const auto optionalValue = convertTomlValueToType<TYPEB>(value);                                 \
            if (!optionalValue.has_value()) {                                                                \
                return Error(fmt::format(                                                                    \
                    "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "        \
                    "but deserializer failed to convert TOML value to type {}",                              \
                    sFieldCanonicalTypeName,                                                                 \
                    sFieldName,                                                                              \
                    #TYPEB));                                                                                \
            }                                                                                                \
            castedValue = optionalValue.value();                                                             \
            map[castedKey] = castedValue;                                                                    \
        }                                                                                                    \
        pField->setUnsafe<std::unordered_map<TYPEA, TYPEB>>(pFieldOwner, std::move(map));                    \
        return {};                                                                                           \
    }

// and another helper macro.
#define DESERIALIZE_UNORDERED_MAP_TYPES(TYPE)                                                                \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, bool)                                                               \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, int)                                                                \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, long long)                                                          \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, float)                                                              \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, double)                                                             \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, std::basic_string<char>)

        DESERIALIZE_UNORDERED_MAP_TYPES(bool)
        DESERIALIZE_UNORDERED_MAP_TYPES(int)
        DESERIALIZE_UNORDERED_MAP_TYPES(long long)
        DESERIALIZE_UNORDERED_MAP_TYPES(float)
        DESERIALIZE_UNORDERED_MAP_TYPES(double)
        DESERIALIZE_UNORDERED_MAP_TYPES(std::basic_string<char>)

        return Error(fmt::format(
            "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
            sFieldCanonicalTypeName,
            sFieldName));
    }

    std::optional<Error> UnorderedMapFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());
        const auto sFieldName = pFromField->getName();

// Define another helper macro...
#define CLONE_UNORDERED_MAP_TYPE(TYPEA, TYPEB)                                                               \
    if (sFieldCanonicalTypeName == fmt::format("std::unordered_map<{}, {}>", #TYPEA, #TYPEB)) {              \
        auto value = pFromField->getUnsafe<std::unordered_map<TYPEA, TYPEB>>(pFromInstance);                 \
        pToField->setUnsafe<std::unordered_map<TYPEA, TYPEB>>(pToInstance, std::move(value));                \
        return {};                                                                                           \
    }

// and another helper macro.
#define CLONE_UNORDERED_MAP_TYPES(TYPE)                                                                      \
    CLONE_UNORDERED_MAP_TYPE(TYPE, bool)                                                                     \
    CLONE_UNORDERED_MAP_TYPE(TYPE, int)                                                                      \
    CLONE_UNORDERED_MAP_TYPE(TYPE, long long)                                                                \
    CLONE_UNORDERED_MAP_TYPE(TYPE, float)                                                                    \
    CLONE_UNORDERED_MAP_TYPE(TYPE, double)                                                                   \
    CLONE_UNORDERED_MAP_TYPE(TYPE, std::basic_string<char>)

        CLONE_UNORDERED_MAP_TYPES(bool)
        CLONE_UNORDERED_MAP_TYPES(int)
        CLONE_UNORDERED_MAP_TYPES(long long)
        CLONE_UNORDERED_MAP_TYPES(float)
        CLONE_UNORDERED_MAP_TYPES(double)
        CLONE_UNORDERED_MAP_TYPES(std::basic_string<char>)

        return Error(fmt::format(
            "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
            sFieldCanonicalTypeName,
            sFieldName));
    }
} // namespace ne
