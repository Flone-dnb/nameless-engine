#include "io/serializers/GlmVecFieldSerializer.h"

// Custom.
#include "math/GLMath.hpp"

// External.
#include "fmt/core.h"

namespace ne {
    bool GlmVecFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == sVec2CanonicalTypeName ||
            sFieldCanonicalTypeName == sVec3CanonicalTypeName ||
            sFieldCanonicalTypeName == sVec4CanonicalTypeName) {
            return true;
        }

        return false;
    }

    std::optional<Error> GlmVecFieldSerializer::serializeField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId,
        Serializable* pOriginalObject) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        if (sFieldCanonicalTypeName == sVec2CanonicalTypeName) {
            const auto value = pField->getUnsafe<glm::vec2>(pFieldOwner);
            pTomlData->operator[](sSectionName).operator[](sFieldName) = std::vector<float>{value.x, value.y};
        } else if (sFieldCanonicalTypeName == sVec3CanonicalTypeName) {
            const auto value = pField->getUnsafe<glm::vec3>(pFieldOwner);
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                std::vector<float>{value.x, value.y, value.z};
        } else if (sFieldCanonicalTypeName == sVec4CanonicalTypeName) {
            const auto value = pField->getUnsafe<glm::vec4>(pFieldOwner);
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                std::vector<float>{value.x, value.y, value.z, value.w};
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        return {};
    }

    std::optional<Error> GlmVecFieldSerializer::deserializeField(
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

        if (sFieldCanonicalTypeName == sVec2CanonicalTypeName) {
            auto array = pTomlValue->as_array();
            std::vector<float> floatArray;
            if (array.size() != 2) {
                return Error(fmt::format(
                    "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                    "but the TOML value (array) has incorrect size.",
                    sFieldCanonicalTypeName,
                    sFieldName));
            }
            for (const auto& item : array) {
                if (!item.is_floating()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not float.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                floatArray.push_back(static_cast<float>(item.as_floating()));
            }
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec2*>(pField->getPtrUnsafe(pFieldOwner));
            float* pData = glm::value_ptr(*pVec);
            for (size_t i = 0; i < floatArray.size(); i++) {
                pData[i] = floatArray[i];
            }
        } else if (sFieldCanonicalTypeName == sVec3CanonicalTypeName) {
            auto array = pTomlValue->as_array();
            std::vector<float> floatArray;
            if (array.size() != 3) {
                return Error(fmt::format(
                    "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                    "but the TOML value (array) has incorrect size.",
                    sFieldCanonicalTypeName,
                    sFieldName));
            }
            for (const auto& item : array) {
                if (!item.is_floating()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not float.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                floatArray.push_back(static_cast<float>(item.as_floating()));
            }
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec3*>(pField->getPtrUnsafe(pFieldOwner));
            float* pData = glm::value_ptr(*pVec);
            for (size_t i = 0; i < floatArray.size(); i++) {
                pData[i] = floatArray[i];
            }
        } else if (sFieldCanonicalTypeName == sVec4CanonicalTypeName) {
            auto array = pTomlValue->as_array();
            std::vector<float> floatArray;
            if (array.size() != 4) {
                return Error(fmt::format(
                    "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                    "but the TOML value (array) has incorrect size.",
                    sFieldCanonicalTypeName,
                    sFieldName));
            }
            for (const auto& item : array) {
                if (!item.is_floating()) {
                    return Error(fmt::format(
                        "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not float.",
                        sFieldCanonicalTypeName,
                        sFieldName));
                }
                floatArray.push_back(static_cast<float>(item.as_floating()));
            }
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec4*>(pField->getPtrUnsafe(pFieldOwner));
            float* pData = glm::value_ptr(*pVec);
            for (size_t i = 0; i < floatArray.size(); i++) {
                pData[i] = floatArray[i];
            }
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        return {};
    }

    std::optional<Error> GlmVecFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == sVec2CanonicalTypeName) {
            auto value = pFromField->getUnsafe<glm::vec2>(pFromInstance);
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec2*>(pToField->getPtrUnsafe(pToInstance));
            pVec->x = value.x;
            pVec->y = value.y;
        } else if (sFieldCanonicalTypeName == sVec3CanonicalTypeName) {
            auto value = pFromField->getUnsafe<glm::vec3>(pFromInstance);
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec3*>(pToField->getPtrUnsafe(pToInstance));
            pVec->x = value.x;
            pVec->y = value.y;
            pVec->z = value.z;
        } else if (sFieldCanonicalTypeName == sVec4CanonicalTypeName) {
            auto value = pFromField->getUnsafe<glm::vec4>(pFromInstance);
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec4*>(pToField->getPtrUnsafe(pToInstance));
            pVec->x = value.x;
            pVec->y = value.y;
            pVec->z = value.z;
            pVec->w = value.w;
        } else {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFromField->getName()));
        }

        return {};
    }

    bool GlmVecFieldSerializer::isFieldValueEqual(
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

        if (sFieldACanonicalTypeName == sVec2CanonicalTypeName) {
            const auto valueA = pFieldA->getUnsafe<glm::vec2>(pFieldAOwner);
            const auto valueB = pFieldB->getUnsafe<glm::vec2>(pFieldBOwner);
            return glm::all(glm::epsilonEqual(valueA, valueB, floatEpsilon));
        } else if (sFieldACanonicalTypeName == sVec3CanonicalTypeName) {
            const auto valueA = pFieldA->getUnsafe<glm::vec3>(pFieldAOwner);
            const auto valueB = pFieldB->getUnsafe<glm::vec3>(pFieldBOwner);
            return glm::all(glm::epsilonEqual(valueA, valueB, floatEpsilon));
        } else if (sFieldACanonicalTypeName == sVec4CanonicalTypeName) {
            const auto valueA = pFieldA->getUnsafe<glm::vec4>(pFieldAOwner);
            const auto valueB = pFieldB->getUnsafe<glm::vec4>(pFieldBOwner);
            return glm::all(glm::epsilonEqual(valueA, valueB, floatEpsilon));
        }

        return false;
    }
} // namespace ne
