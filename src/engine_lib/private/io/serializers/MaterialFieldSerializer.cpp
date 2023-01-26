#include "io/serializers/MaterialFieldSerializer.h"

// Custom.
#include "io/Serializable.h"
#include "materials/Material.h"
#include "io/serializers/SerializableObjectFieldSerializer.h"

// External.
#include "fmt/core.h"

namespace ne {
    bool MaterialFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        return sFieldCanonicalTypeName == sSharedPtrMaterialCanonicalTypeName;
    }

    std::optional<Error> MaterialFieldSerializer::serializeField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId,
        Serializable* pOriginalObject) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto pFieldName = pField->getName();

        if (!isFieldTypeSupported(pField)) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        const auto pSubEntity = static_cast<std::shared_ptr<Material>*>(pField->getPtrUnsafe(pFieldOwner));
        if (*pSubEntity == nullptr) {
            return {};
        }

        Material* pRawMaterial = (*pSubEntity).get();

        const auto optionalError = SerializableObjectFieldSerializer::serializeFieldObject(
            pRawMaterial, pTomlData, pFieldName, sSectionName, sEntityId, iSubEntityId, pOriginalObject);
        if (optionalError.has_value()) {
            Error error = optionalError.value();
            error.addEntry();
            return error;
        }

        return {};
    }

    std::optional<Error> MaterialFieldSerializer::deserializeField(
        const toml::value* pTomlDocument,
        const toml::value* pTomlValue,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sOwnerSectionName,
        const std::string& sEntityId,
        std::unordered_map<std::string, std::string>& customAttributes) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto pFieldName = pField->getName();

        if (!isFieldTypeSupported(pField)) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        const auto pSubEntity = static_cast<std::shared_ptr<Material>*>(pField->getPtrUnsafe(pFieldOwner));
        Serializable* pTarget = (*pSubEntity).get();

        // Deserialize object.
        auto result = SerializableObjectFieldSerializer::deserializeSerializableObject(
            pTomlDocument, pTomlValue, pFieldName, pTarget, sOwnerSectionName, sEntityId, customAttributes);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        auto pDeserializedObject = std::get<std::shared_ptr<Serializable>>(std::move(result));

        // Safely clone to target.
        auto optionalError = SerializableObjectFieldSerializer::cloneSerializableObject(
            pDeserializedObject.get(), pTarget, true);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            return error;
        }

        return {};
    }

    std::optional<Error> MaterialFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());
        const auto pFieldName = pFromField->getName();

        if (!isFieldTypeSupported(pFromField)) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        const auto pFrom = static_cast<std::shared_ptr<Material>*>(pFromField->getPtrUnsafe(pFromInstance));
        const auto pTo = static_cast<std::shared_ptr<Material>*>(pToField->getPtrUnsafe(pToInstance));

        if ((*pFrom) == nullptr || (*pTo) == nullptr) {
            return Error("One of the fields is `nullptr`.");
        }

        auto optionalError =
            SerializableObjectFieldSerializer::cloneSerializableObject((*pFrom).get(), (*pTo).get(), false);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            return error;
        }

        return {};
    }

    bool MaterialFieldSerializer::isFieldValueEqual(
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

        const auto pEntityA = static_cast<std::shared_ptr<Material>*>(pFieldA->getPtrUnsafe(pFieldAOwner));
        const auto pEntityB = static_cast<std::shared_ptr<Material>*>(pFieldB->getPtrUnsafe(pFieldBOwner));

        if ((*pEntityA) == nullptr && (*pEntityB) == nullptr) {
            return true;
        }

        if ((*pEntityA) == nullptr || (*pEntityB) == nullptr) {
            return false;
        }

        return SerializableObjectFieldSerializer::isSerializableObjectValueEqual(
            (*pEntityA).get(), (*pEntityB).get());
    }
} // namespace ne
