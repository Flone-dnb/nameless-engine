#include "io/serializers/SerializableObjectFieldSerializer.h"

// Custom.
#include "io/Serializable.h"
#include "io/GuidProperty.h"
#include "io/Logger.h"

// External.
#include "fmt/format.h"

namespace ne {
    bool SerializableObjectFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto& fieldType = pField->getType();

        if (fieldType.getArchetype() && Serializable::isDerivedFromSerializable(fieldType.getArchetype())) {
            return true;
        }

        return false;
    }

    std::optional<Error> SerializableObjectFieldSerializer::serializeField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId,
        Serializable* pOriginalObject) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        if (!isFieldTypeSupported(pField)) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        const auto& fieldType = pField->getType();

        // Check that this type has GUID.
        if (!fieldType.getArchetype()->getProperty<Guid>(false)) {
            return Error(fmt::format(
                "The type \"{}\" is serializable but it does not have a GUID assigned to it.",
                fieldType.getArchetype()->getName()));
        }

        // Add a key to specify that this value has a reflected type.
        pTomlData->operator[](sSectionName).operator[](sFieldName) = "reflected type, see other sub-section";

        // Serialize this field "under our ID".
        const std::string sSubEntityIdSectionName = fmt::format("{}.{}", sEntityId, iSubEntityId);
        const auto pSubEntity = static_cast<Serializable*>(pField->getPtrUnsafe(pFieldOwner));
        auto result = pSubEntity->serialize(*pTomlData, pOriginalObject, sSubEntityIdSectionName);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        const auto sSubEntityFinalSectionName = std::get<std::string>(result);
        iSubEntityId += 1;

        // Add a new key ".field_name" to this sub entity so that we will know to which
        // field this entity should be assigned.
        pTomlData->operator[](sSubEntityFinalSectionName).operator[](sSubEntityFieldNameKey) = sFieldName;

        return {};
    }

    std::optional<Error>
    SerializableObjectFieldSerializer::cloneSerializableObject(Serializable* pFrom, Serializable* pTo) {
        rfk::Class const& fromArchetype = pFrom->getArchetype();
        rfk::Class const& toArchetype = pTo->getArchetype();

        // Check if types are equal.
        const auto pFromGuid = fromArchetype.getProperty<Guid>(false);
        const auto pToGuid = toArchetype.getProperty<Guid>(false);
        if (!pFromGuid) {
            const Error err(
                fmt::format("The type {} does not have a GUID assigned to it", fromArchetype.getName()));
            return err;
        }
        if (!pToGuid) {
            const Error err(
                fmt::format("The type {} does not have a GUID assigned to it", toArchetype.getName()));
            return err;
        }

        if (pFromGuid->getGuid() != pToGuid->getGuid()) {
            return Error(fmt::format(
                "Types \"{}\" and \"{}\" are not the same", fromArchetype.getName(), toArchetype.getName()));
        }

        struct Data {
            Serializable* pFrom;
            Serializable* pTo;
            std::vector<IFieldSerializer*> vFieldSerializers;
            std::optional<Error> error;
        };

        Data loopData{pFrom, pTo, Serializable::getFieldSerializers(), std::optional<Error>{}};

        fromArchetype.foreachField(
            [](rfk::Field const& field, void* userData) -> bool {
                if (!Serializable::isFieldSerializable(field))
                    return true;

                Data* pData = static_cast<Data*>(userData);
                const auto sFieldName = field.getName();

                const auto* pFieldTo =
                    pData->pTo->getArchetype().getFieldByName(sFieldName, rfk::EFieldFlags::Default, true);
                if (!pFieldTo) {
                    pData->error = Error(fmt::format(
                        "Unable to find the field \"{}\" in type \"{}\".",
                        field.getName(),
                        pData->pTo->getArchetype().getName()));
                    return false;
                }

                for (const auto& pSerializer : pData->vFieldSerializers) {
                    if (pSerializer->isFieldTypeSupported(&field)) {
                        auto optionalError =
                            pSerializer->cloneField(pData->pFrom, &field, pData->pTo, pFieldTo);
                        if (optionalError.has_value()) {
                            auto err = std::move(optionalError.value());
                            err.addEntry();
                            pData->error = err;
                            return false;
                        }

                        return true;
                    }
                }

                pData->error = Error(
                    fmt::format("The field \"{}\" has unsupported for serialization type", field.getName()));
                return false;
            },
            &loopData,
            true);

        if (loopData.error.has_value()) {
            auto err = std::move(loopData.error.value());
            err.addEntry();
            return err;
        }

        return {};
    }

    std::optional<Error> SerializableObjectFieldSerializer::deserializeField(
        const toml::value* pTomlDocument,
        const toml::value* pTomlValue,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sOwnerSectionName,
        const std::string& sEntityId,
        std::unordered_map<std::string, std::string>& customAttributes) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        if (!isFieldTypeSupported(pField)) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        if (!pTomlDocument->is_table()) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is supported by this serializer, "
                "but the TOML document is not a table.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        // Read all sections.
        std::vector<std::string> vSections;
        const auto fileTable = pTomlDocument->as_table();
        for (const auto& [key, value] : fileTable) {
            if (value.is_table()) {
                vSections.push_back(key);
            }
        }

        // Find a section that has the key ".field_name = *our field name*".
        std::string sSectionNameForField;
        // This will have minimum value of 1 where the dot separates IDs from GUID.
        const auto iNumberOfDotsInTargetSectionName =
            std::ranges::count(sOwnerSectionName.begin(), sOwnerSectionName.end(), '.');
        for (const auto& sSectionName : vSections) {
            if (sSectionName == sOwnerSectionName)
                continue; // skip self

            const auto iNumberOfDotsInSectionName =
                std::ranges::count(sSectionName.begin(), sSectionName.end(), '.');

            // Look for a section that has 1 more dot than our section. Example:
            // Our section: ["0.3056171360419407975"]
            // Child section that we are looking for: ["0.1.4321359943817265529"]
            if (iNumberOfDotsInSectionName != iNumberOfDotsInTargetSectionName + 1) {
                continue;
            }

            // Here we might get into the following situation:
            // Our section: "0.3056171360419407975"
            // Current section: "1.0.3056171360419407975" - first field of some OTHER entity.

            // Get entity ID chain.
            auto iLastDotPos = sSectionName.rfind('.');
            if (iLastDotPos == std::string::npos) {
                return Error(fmt::format("section name \"{}\" is corrupted", sSectionName));
            }
            // Will be something like: "entityId.subEntityId", "entityId.subEntityId.subSubEntityId"
            // or etc.
            auto sEntityIdChain = sSectionName.substr(0, iLastDotPos);

            // Remove last entity id from chain because we don't know it.
            iLastDotPos = sEntityIdChain.rfind('.');
            if (iLastDotPos == std::string::npos) {
                return Error(fmt::format("section name \"{}\" is corrupted", sSectionName));
            }
            sEntityIdChain = sEntityIdChain.substr(0, iLastDotPos);

            // Check that this is indeed our sub entity.
            if (sEntityIdChain != sEntityId) {
                continue;
            }

            // Get this section.
            toml::value entitySection;
            try {
                entitySection = toml::find(*pTomlDocument, sSectionName);
            } catch (std::exception& ex) {
                return Error(fmt::format("no section \"{}\" was found ({})", sOwnerSectionName, ex.what()));
            }

            if (!entitySection.is_table()) {
                return Error(fmt::format("found \"{}\" section is not a section", sOwnerSectionName));
            }

            // Look for a key that holds the field name.
            toml::value fieldKey;
            try {
                fieldKey = toml::find(entitySection, sSubEntityFieldNameKey);
            } catch (...) {
                // Not found, go to next section.
                continue;
            }

            if (!fieldKey.is_string()) {
                return Error(
                    fmt::format("found field name key \"{}\" is not a string", sSubEntityFieldNameKey));
            }

            if (fieldKey.as_string() == sFieldName) {
                sSectionNameForField = sSectionName;
                break;
            }
        }

        if (sSectionNameForField.empty()) {
            return Error(fmt::format("could not find a section that represents field \"{}\"", sFieldName));
        }

        // Cut field's GUID from the section name.
        // The section name could look something like this: [entityId.subEntityId.subEntityGUID].
        const auto iSubEntityGuidDotPos = sSectionNameForField.rfind('.');
        if (iSubEntityGuidDotPos == std::string::npos) [[unlikely]] {
            return Error(
                fmt::format("sub entity does not have a GUID (section: \"{}\")", sSectionNameForField));
        }
        if (iSubEntityGuidDotPos + 1 == sSectionNameForField.size()) [[unlikely]] {
            return Error(fmt::format("section name \"{}\" does not have a GUID", sSectionNameForField));
        }
        if (iSubEntityGuidDotPos == 0) [[unlikely]] {
            return Error(fmt::format("section \"{}\" is not full", sSectionNameForField));
        }
        const auto sSubEntityId = sSectionNameForField.substr(0, iSubEntityGuidDotPos);

        // Deserialize section into an object.
        std::unordered_map<std::string, std::string> subAttributes;
        auto result = Serializable::deserialize<Serializable>(*pTomlDocument, subAttributes, sSubEntityId);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }
        auto pSubEntity = std::get<gc<Serializable>>(std::move(result));

        // Move object to field.
        cloneSerializableObject(&*pSubEntity, static_cast<Serializable*>(pField->getPtrUnsafe(pFieldOwner)));

        return {};
    }

    std::optional<Error> SerializableObjectFieldSerializer::cloneField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());
        const auto sFieldName = pFromField->getName();

        if (!isFieldTypeSupported(pFromField)) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                sFieldName));
        }

        auto optionalError = cloneSerializableObject(
            static_cast<Serializable*>(pFromField->getPtrUnsafe(pFromInstance)),
            static_cast<Serializable*>(pToField->getPtrUnsafe(pToInstance)));
        if (optionalError.has_value()) {
            auto error = std::move(optionalError.value());
            error.addEntry();
            return error;
        }

        return {};
    }

    bool SerializableObjectFieldSerializer::isFieldValueEqual(
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

        const auto pEntityA = static_cast<Serializable*>(pFieldA->getPtrUnsafe(pFieldAOwner));
        const auto pEntityB = static_cast<Serializable*>(pFieldB->getPtrUnsafe(pFieldBOwner));

        rfk::Class const& entityAArchetype = pEntityA->getArchetype();

        // Prepare data.
        struct Data {
            Serializable* pSelf = nullptr;
            Serializable* pOtherEntity = nullptr;
            std::vector<IFieldSerializer*> vFieldSerializers;
            bool bIsEqual = true;
            std::optional<Error> error = {};
        };

        Data loopData{pEntityA, pEntityB, Serializable::getFieldSerializers()};

        entityAArchetype.foreachField(
            [](rfk::Field const& field, void* userData) -> bool {
                if (!Serializable::isFieldSerializable(field))
                    return true;

                Data* pData = static_cast<Data*>(userData);
                const auto sFieldName = field.getName();

                // Check if this field's value is equal.
                // Reflected field names are unique (this is checked in Serializable).
                const auto pOtherField = pData->pOtherEntity->getArchetype().getFieldByName(
                    sFieldName, rfk::EFieldFlags::Default, true);
                if (!pOtherField) {
                    // Probably will never happen but still add a check.
                    Logger::get().error(
                        fmt::format(
                            "the field \"{}\" (maybe inherited) of type \"{}\" was not found "
                            "in the other entity of type \"{}\" (this is strange because "
                            "entities have equal canonical type name)",
                            field.getName(),
                            pData->pSelf->getArchetype().getName(),
                            pData->pOtherEntity->getArchetype().getName()),
                        "");
                    pData->bIsEqual = false;
                    return false;
                }

                // Check if the field values are equal.
                for (const auto& pSerializer : pData->vFieldSerializers) {
                    if (pSerializer->isFieldTypeSupported(pOtherField) &&
                        pSerializer->isFieldTypeSupported(&field)) {
                        if (pSerializer->isFieldValueEqual(
                                pData->pSelf, &field, pData->pOtherEntity, pOtherField)) {
                            // Field values are equal, continue.
                            return true;
                        } else {
                            // Field values are different, stop.
                            pData->bIsEqual = false;
                            return false;
                        }
                    }
                }

                Logger::get().error(
                    fmt::format(
                        "failed to compare value of the field \"{}\" of type \"{}\" "
                        "with the field from other entity, reason: no serializer "
                        "supports both field types (maybe we took the wrong field from the "
                        "original file",
                        field.getName(),
                        pData->pSelf->getArchetype().getName()),
                    "");
                pData->bIsEqual = false;
                return false;
            },
            &loopData,
            true);

        return loopData.bIsEqual;
    }
} // namespace ne
