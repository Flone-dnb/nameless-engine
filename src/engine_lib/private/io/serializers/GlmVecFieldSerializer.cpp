#include "io/serializers/GlmVecFieldSerializer.h"

// Standard.
#include <format>

// Custom.
#include "math/GLMath.hpp"

namespace ne {
    bool GlmVecFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        return sFieldCanonicalTypeName == sVec3CanonicalTypeName1 || // test most likely types first
               sFieldCanonicalTypeName == sVec3CanonicalTypeName2 ||
               sFieldCanonicalTypeName == sVec2CanonicalTypeName1 ||
               sFieldCanonicalTypeName == sVec2CanonicalTypeName2 ||
               sFieldCanonicalTypeName == sVec4CanonicalTypeName1 ||
               sFieldCanonicalTypeName == sVec4CanonicalTypeName2;
    }

    std::vector<std::string> vecFloatToString(const std::vector<float>& vInitial) {
        std::vector<std::string> vOutput;
        for (size_t i = 0; i < vInitial.size(); i++) {
            // Store as string for better precision.
            vOutput.push_back(floatingToString(vInitial[i]));
        }
        return vOutput;
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
        const auto pFieldName = pField->getName();

        if (sFieldCanonicalTypeName == sVec2CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec2CanonicalTypeName2) {
            const auto value = pField->getUnsafe<glm::vec2>(pFieldOwner);
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                vecFloatToString(std::vector<float>{value.x, value.y});
        } else if (
            sFieldCanonicalTypeName == sVec3CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec3CanonicalTypeName2) {
            const auto value = pField->getUnsafe<glm::vec3>(pFieldOwner);
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                vecFloatToString(std::vector<float>{value.x, value.y, value.z});
        } else if (
            sFieldCanonicalTypeName == sVec4CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec4CanonicalTypeName2) {
            const auto value = pField->getUnsafe<glm::vec4>(pFieldOwner);
            pTomlData->operator[](sSectionName).operator[](pFieldName) =
                vecFloatToString(std::vector<float>{value.x, value.y, value.z, value.w});
        } else [[unlikely]] {
            return Error(std::format(
                "type \"{}\" of the specified field \"{}\" is not supported by this serializer",
                sFieldCanonicalTypeName,
                pFieldName));
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
        const auto pFieldName = pField->getName();

        // Make sure it's an array.
        if (!pTomlValue->is_array()) [[unlikely]] {
            return Error(std::format(
                "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                "but the TOML value is not an array.",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        if (sFieldCanonicalTypeName == sVec3CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec3CanonicalTypeName2) {
            // Make sure array size is correct.
            auto array = pTomlValue->as_array();
            std::vector<float> floatArray;
            if (array.size() != 3) [[unlikely]] {
                return Error(std::format(
                    "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                    "but the TOML value (array) has incorrect size",
                    sFieldCanonicalTypeName,
                    pFieldName));
            }

            for (const auto& item : array) {
                if (!item.is_string()) [[unlikely]] {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not a string",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }

                try {
                    floatArray.push_back(std::stof(item.as_string()));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but an exception occurred while trying to convert a string to a float: {}",
                        sFieldCanonicalTypeName,
                        pFieldName,
                        ex.what()));
                }
            }
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec3*>(pField->getPtrUnsafe(pFieldOwner));
            float* pData = glm::value_ptr(*pVec);
            for (size_t i = 0; i < floatArray.size(); i++) {
                pData[i] = floatArray[i];
            }
        } else if (
            sFieldCanonicalTypeName == sVec2CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec2CanonicalTypeName2) {
            // Make sure array size is correct.
            auto array = pTomlValue->as_array();
            std::vector<float> floatArray;
            if (array.size() != 2) [[unlikely]] {
                return Error(std::format(
                    "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                    "but the TOML value (array) has incorrect size",
                    sFieldCanonicalTypeName,
                    pFieldName));
            }

            for (const auto& item : array) {
                if (!item.is_string()) [[unlikely]] {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not a string",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }

                try {
                    floatArray.push_back(std::stof(item.as_string()));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but an exception occurred while trying to convert a string to a float: {}",
                        sFieldCanonicalTypeName,
                        pFieldName,
                        ex.what()));
                }
            }
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec2*>(pField->getPtrUnsafe(pFieldOwner));
            float* pData = glm::value_ptr(*pVec);
            for (size_t i = 0; i < floatArray.size(); i++) {
                pData[i] = floatArray[i];
            }
        } else if (
            sFieldCanonicalTypeName == sVec4CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec4CanonicalTypeName2) {
            // Make sure array size is correct.
            auto array = pTomlValue->as_array();
            std::vector<float> floatArray;
            if (array.size() != 4) [[unlikely]] {
                return Error(std::format(
                    "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                    "but the TOML value (array) has incorrect size",
                    sFieldCanonicalTypeName,
                    pFieldName));
            }

            for (const auto& item : array) {
                if (!item.is_string()) [[unlikely]] {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but the TOML value is not float",
                        sFieldCanonicalTypeName,
                        pFieldName));
                }

                try {
                    floatArray.push_back(std::stof(item.as_string()));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "the type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                        "but an exception occurred while trying to convert a string to a float: {}",
                        sFieldCanonicalTypeName,
                        pFieldName,
                        ex.what()));
                }
            }
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec4*>(pField->getPtrUnsafe(pFieldOwner));
            float* pData = glm::value_ptr(*pVec);
            for (size_t i = 0; i < floatArray.size(); i++) {
                pData[i] = floatArray[i];
            }
        } else [[unlikely]] {
            return Error(std::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        return {};
    }

    std::optional<Error> GlmVecFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == sVec3CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec3CanonicalTypeName2) {
            auto value = pFromField->getUnsafe<glm::vec3>(pFromInstance);
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec3*>(pToField->getPtrUnsafe(pToInstance));
            pVec->x = value.x;
            pVec->y = value.y;
            pVec->z = value.z;
        } else if (
            sFieldCanonicalTypeName == sVec2CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec2CanonicalTypeName2) {
            auto value = pFromField->getUnsafe<glm::vec2>(pFromInstance);
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec2*>(pToField->getPtrUnsafe(pToInstance));
            pVec->x = value.x;
            pVec->y = value.y;
        } else if (
            sFieldCanonicalTypeName == sVec4CanonicalTypeName1 ||
            sFieldCanonicalTypeName == sVec4CanonicalTypeName2) {
            auto value = pFromField->getUnsafe<glm::vec4>(pFromInstance);
            // `setUnsafe` throws exception for `glm::vec` for some reason, thus use this approach
            auto pVec = reinterpret_cast<glm::vec4*>(pToField->getPtrUnsafe(pToInstance));
            pVec->x = value.x;
            pVec->y = value.y;
            pVec->z = value.z;
            pVec->w = value.w;
        } else [[unlikely]] {
            return Error(std::format(
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

        if (sFieldACanonicalTypeName == sVec3CanonicalTypeName1 ||
            sFieldACanonicalTypeName == sVec3CanonicalTypeName2) {
            const auto valueA = pFieldA->getUnsafe<glm::vec3>(pFieldAOwner);
            const auto valueB = pFieldB->getUnsafe<glm::vec3>(pFieldBOwner);
            return glm::all(glm::epsilonEqual(valueA, valueB, floatEpsilon));
        }

        if (sFieldACanonicalTypeName == sVec2CanonicalTypeName1 ||
            sFieldACanonicalTypeName == sVec2CanonicalTypeName2) {
            const auto valueA = pFieldA->getUnsafe<glm::vec2>(pFieldAOwner);
            const auto valueB = pFieldB->getUnsafe<glm::vec2>(pFieldBOwner);
            return glm::all(glm::epsilonEqual(valueA, valueB, floatEpsilon));
        }

        if (sFieldACanonicalTypeName == sVec4CanonicalTypeName1 ||
            sFieldACanonicalTypeName == sVec4CanonicalTypeName2) {
            const auto valueA = pFieldA->getUnsafe<glm::vec4>(pFieldAOwner);
            const auto valueB = pFieldB->getUnsafe<glm::vec4>(pFieldBOwner);
            return glm::all(glm::epsilonEqual(valueA, valueB, floatEpsilon));
        }

        return false;
    }
} // namespace ne
