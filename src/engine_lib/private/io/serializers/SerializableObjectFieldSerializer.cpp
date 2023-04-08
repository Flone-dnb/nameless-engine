#include "io/serializers/SerializableObjectFieldSerializer.h"

// Custom.
#include "io/Serializable.h"
#include "io/properties/GuidProperty.h"
#include "io/Logger.h"
#include "misc/Globals.h"
#include "misc/GC.hpp"
#include "io/FieldSerializerManager.h"

// External.
#include "fmt/format.h"

namespace ne {
    bool SerializableObjectFieldSerializer::isFieldTypeSupported(const rfk::Field* pField) {
        const auto& fieldType = pField->getType();
        return fieldType.getArchetype() != nullptr && isDerivedFromSerializable(fieldType.getArchetype());
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
        const auto pFieldName = pField->getName();

        if (!isFieldTypeSupported(pField)) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        const auto pSubEntity = static_cast<Serializable*>(pField->getPtrUnsafe(pFieldOwner));

        const auto optionalError = serializeFieldObject(
            pSubEntity, pTomlData, pFieldName, sSectionName, sEntityId, iSubEntityId, pOriginalObject);
        if (optionalError.has_value()) {
            Error error = optionalError.value();
            error.addEntry();
            return error;
        }

        return {};
    }

    std::optional<Error> SerializableObjectFieldSerializer::cloneSerializableObject(
        Serializable* pFrom, Serializable* pTo, bool bNotifyAboutDeserialized) {
        rfk::Class const& fromArchetype = pFrom->getArchetype();
        rfk::Class const& toArchetype = pTo->getArchetype();

        // Check if types are equal.
        const auto pFromGuid = fromArchetype.getProperty<Guid>(false);
        const auto pToGuid = toArchetype.getProperty<Guid>(false);
        if (pFromGuid == nullptr) {
            const Error err(
                fmt::format("The type {} does not have a GUID assigned to it", fromArchetype.getName()));
            return err;
        }
        if (pToGuid == nullptr) {
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

        Data loopData{pFrom, pTo, FieldSerializerManager::getFieldSerializers(), std::optional<Error>{}};

        fromArchetype.foreachField(
            [](rfk::Field const& field, void* pUserData) -> bool {
                if (!isFieldSerializable(field)) {
                    return true;
                }

                Data* pData = static_cast<Data*>(pUserData);
                const auto pFieldName = field.getName();

                const auto* pFieldTo =
                    pData->pTo->getArchetype().getFieldByName(pFieldName, rfk::EFieldFlags::Default, true);
                if (pFieldTo == nullptr) {
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

        if (bNotifyAboutDeserialized) {
            pTo->onAfterDeserialized();
        }

        return {};
    }

    std::variant<std::shared_ptr<Serializable>, Error>
    SerializableObjectFieldSerializer::deserializeSerializableObject(
        const toml::value* pTomlDocument,
        const toml::value* pTomlValue,
        const std::string& sFieldName,
        Serializable* pTarget,
        const std::string& sOwnerSectionName,
        const std::string& sEntityId,
        std::unordered_map<std::string, std::string>& customAttributes) {
        if (!pTomlDocument->is_table()) {
            return Error(fmt::format(
                "The type of the specified field \"{}\" is supported by this serializer, "
                "but the TOML document is not a table.",
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
            if (sSectionName == sOwnerSectionName) {
                continue; // skip self
            }

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
        auto result = Serializable::deserialize<std::shared_ptr, Serializable>(
            *pTomlDocument, subAttributes, sSubEntityId);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        return std::get<std::shared_ptr<Serializable>>(std::move(result));
    }

    bool SerializableObjectFieldSerializer::isSerializableObjectValueEqual(
        Serializable* pObjectA, Serializable* pObjectB) {
        rfk::Class const& entityAArchetype = pObjectA->getArchetype();

        // Prepare data.
        struct Data {
            Serializable* pSelf = nullptr;
            Serializable* pOtherEntity = nullptr;
            std::vector<IFieldSerializer*> vFieldSerializers;
            bool bIsEqual = true;
            std::optional<Error> error = {};
        };

        Data loopData{pObjectA, pObjectB, FieldSerializerManager::getFieldSerializers()};

        entityAArchetype.foreachField(
            [](rfk::Field const& field, void* pUserData) -> bool {
                if (!isFieldSerializable(field)) {
                    return true;
                }

                Data* pData = static_cast<Data*>(pUserData);
                const auto pFieldName = field.getName();

                // Check if this field's value is equal.
                // Reflected field names are unique (this is checked in Serializable).
                const auto pOtherField = pData->pOtherEntity->getArchetype().getFieldByName(
                    pFieldName, rfk::EFieldFlags::Default, true);
                if (pOtherField == nullptr) {
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
                        }

                        // Field values are different, stop.
                        pData->bIsEqual = false;
                        return false;
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

    bool
    SerializableObjectFieldSerializer::isTypeDerivesFromSerializable(const std::string& sCanonicalTypeName) {
        // Make sure the type has no templates (not supported).
        if (sCanonicalTypeName.contains('<')) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "unable to check if type \"{}\" derives from Serializable because templates are not "
                    "supported",
                    sCanonicalTypeName),
                "");
            return false;
        }

        // Make sure the type is not a raw pointer (not supported).
        if (sCanonicalTypeName.contains('*')) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "unable to check if type \"{}\" derives from Serializable because pointer types are not "
                    "supported",
                    sCanonicalTypeName),
                "");
            return false;
        }

        // See if the type has a namespace.
        auto iFoundNamespaceEndPosition = sCanonicalTypeName.rfind(':');
        if (iFoundNamespaceEndPosition == std::string::npos) {
            // No namespace.
            return isTypeDerivesFromSerializable(sCanonicalTypeName, nullptr);
        }

        // Make sure found position is valid.
        if (iFoundNamespaceEndPosition <= 1) {
            return false;
        }

        iFoundNamespaceEndPosition -= 1;

        // Cut full namespace name (including inner namespace).
        const auto sNamespaceName = sCanonicalTypeName.substr(0, iFoundNamespaceEndPosition);

        // Find this namespace in the reflection database.
        try {
            const auto pNamespace = rfk::getDatabase().getNamespaceByName(sNamespaceName.c_str());
            if (pNamespace == nullptr) {
                return false;
            }

            // Get inner type name.
            const auto sInnerType = sCanonicalTypeName.substr(iFoundNamespaceEndPosition + 1);

            // Get GUID.
            return isTypeDerivesFromSerializable(sInnerType, pNamespace);
        } catch (std::exception& exception) {
            Logger::get().error(
                fmt::format(
                    "failed to get type GUID because namespace name \"{}\" is incorrectly formatted, error: "
                    "{}",
                    sNamespaceName,
                    exception.what()),
                "");
            return false;
        }
    }

    bool SerializableObjectFieldSerializer::isFieldSerializable(const rfk::Field& field) {
        const auto& fieldType = field.getType();

        // Ignore this field if not marked as Serialize.
        if (field.getProperty<Serialize>() == nullptr) {
            return false;
        }

        // Don't serialize specific types.
        if (fieldType.isConst() || fieldType.isPointer() || fieldType.isLValueReference() ||
            fieldType.isRValueReference() || fieldType.isCArray()) {
            return false;
        }

        return true;
    }

    bool SerializableObjectFieldSerializer::isDerivedFromSerializable(const rfk::Archetype* pArchetype) {
        if (rfk::Class const* pClass = rfk::classCast(pArchetype)) {
            // Make sure the type derives from `Serializable`t.
            if (pClass->isSubclassOf(Serializable::staticGetArchetype())) {
                return true;
            }

            // Make sure the type has GUID.
            const auto pGuid = pClass->getProperty<Guid>(false);
            if (pGuid == nullptr) {
                return false;
            }

            // (don't know if this is needed or not)
            if (pGuid->getGuid() == Serializable::staticGetArchetype().getProperty<Guid>(false)->getGuid()) {
                return true;
            }

            return false;
        }

        if (rfk::Struct const* pStruct = rfk::structCast(pArchetype)) {
            // Check parents.
            return pStruct->isSubclassOf(Serializable::staticGetArchetype());
        }

        return false;
    }

    bool SerializableObjectFieldSerializer::isTypeDerivesFromSerializable(
        const std::string& sCanonicalTypeName, const rfk::Namespace* pNamespace) {
        // Attempt to find the target type.
        rfk::Struct const* pTargetType = nullptr;
        if (pNamespace == nullptr) {
            pTargetType = rfk::getDatabase().getFileLevelClassByName(sCanonicalTypeName.c_str());
            if (pTargetType == nullptr) {
                pTargetType = rfk::getDatabase().getFileLevelStructByName(sCanonicalTypeName.c_str());
                if (pTargetType == nullptr) {
                    return false;
                }
            }
        } else {
            pTargetType = pNamespace->getClassByName(sCanonicalTypeName.c_str());
            if (pTargetType == nullptr) {
                pTargetType = pNamespace->getStructByName(sCanonicalTypeName.c_str());
                if (pTargetType == nullptr) {
                    return false;
                }
            }
        }

        // Make sure this type derives from `Serializable`.
        if (!pTargetType->isSubclassOf(Serializable::staticGetArchetype())) {
            return false;
        }

        // Make sure this type has a GUID.
        const auto pGuidProperty = pTargetType->getProperty<Guid>(false);

        return pGuidProperty != nullptr;
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
        const auto pFieldName = pField->getName();

        if (!isFieldTypeSupported(pField)) {
            return Error(fmt::format(
                "The type \"{}\" of the specified field \"{}\" is not supported by this serializer.",
                sFieldCanonicalTypeName,
                pFieldName));
        }

        Serializable* pTarget = static_cast<Serializable*>(pField->getPtrUnsafe(pFieldOwner));

        // Deserialize object.
        auto result = deserializeSerializableObject(
            pTomlDocument, pTomlValue, pFieldName, pTarget, sOwnerSectionName, sEntityId, customAttributes);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        auto pDeserializedObject = std::get<std::shared_ptr<Serializable>>(std::move(result));

        // Safely clone to target.
        auto optionalError = cloneSerializableObject(&*pDeserializedObject, pTarget, true);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            return error;
        }

        return {};
    }

    std::optional<Error> SerializableObjectFieldSerializer::cloneField(
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

        auto optionalError = cloneSerializableObject(
            static_cast<Serializable*>(pFromField->getPtrUnsafe(pFromInstance)),
            static_cast<Serializable*>(pToField->getPtrUnsafe(pToInstance)),
            false);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
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

        const auto pEntityA = static_cast<Serializable*>(pFieldA->getPtrUnsafe(pFieldAOwner));
        const auto pEntityB = static_cast<Serializable*>(pFieldB->getPtrUnsafe(pFieldBOwner));

        return isSerializableObjectValueEqual(pEntityA, pEntityB);
    }

#if defined(DEBUG)
    void SerializableObjectFieldSerializer::checkGuidUniqueness() {
        // Record start time.
        const auto startTime = std::chrono::steady_clock::now();

        // Map of GUIDs (key) and type names (value).
        std::unordered_map<std::string, std::string> vGuids;

        // Get GUID of this class.
        const auto& selfArchetype = Serializable::staticGetArchetype();
        const auto pSelfGuid = selfArchetype.getProperty<Guid>(false);
        if (pSelfGuid == nullptr) {
            const Error err(
                fmt::format("type {} does not have a GUID assigned to it", selfArchetype.getName()));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }
        vGuids[pSelfGuid->getGuid()] = selfArchetype.getName();

        collectGuids(&selfArchetype, vGuids);

        // Calculate time it took for us to do all this.
        const auto endTime = std::chrono::steady_clock::now();
        const auto durationInMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        const float timeTookInSec = static_cast<float>(durationInMs) / 1000.0F; // NOLINT

        // Limit precision to 1 digit.
        std::stringstream durationStream;
        durationStream << std::fixed << std::setprecision(1) << timeTookInSec;

        Logger::get().info(
            fmt::format(
                "[{}] finished checking all GUID uniqueness, took: {} sec.",
                sDebugOnlyLoggingSubCategory,
                durationStream.str()),
            "");
    }

    void SerializableObjectFieldSerializer::collectGuids(
        const rfk::Struct* pArchetypeToAnalyze, std::unordered_map<std::string, std::string>& vAllGuids) {
        const auto vDirectSubclasses = pArchetypeToAnalyze->getDirectSubclasses();
        for (const auto& pDerivedEntity : vDirectSubclasses) {
            const auto pGuid = pDerivedEntity->getProperty<Guid>(false);
            if (pGuid == nullptr) {
                const Error err(fmt::format(
                    "type {} does not have a GUID assigned to it.\n\n"
                    "Here is an example of how to assign a GUID to your type:\n"
                    "class RCLASS(Guid(\"00000000-0000-0000-0000-000000000000\")) MyCoolClass "
                    ": public ne::Serializable",
                    pDerivedEntity->getName()));
                err.showError();
                throw std::runtime_error(err.getFullErrorMessage());
            }

            // Look if this GUID is already used.
            const auto it = vAllGuids.find(pGuid->getGuid());
            if (it != vAllGuids.end()) [[unlikely]] {
                const Error err(fmt::format(
                    "GUID of type {} is already used by type {}, please generate another GUID",
                    pDerivedEntity->getName(),
                    it->second));
                err.showError();
                throw std::runtime_error(err.getFullErrorMessage());
            }

            // Add this GUID.
            vAllGuids[pGuid->getGuid()] = pDerivedEntity->getName();

            // Go though all children.
            collectGuids(pDerivedEntity, vAllGuids);
        }
    }
#endif

    std::optional<Error> SerializableObjectFieldSerializer::serializeFieldObject(
        Serializable* pObject,
        toml::value* pTomlData,
        const std::string& sFieldName,
        const std::string& sSectionName,
        const std::string& sEntityId,
        size_t& iSubEntityId,
        Serializable* pOriginalObject) {
        // Add a key to specify that this value has a reflected type.
        pTomlData->operator[](sSectionName).operator[](sFieldName) = "reflected type, see other sub-section";

        // Serialize this field "under our ID".
        const std::string sSubEntityIdSectionName = fmt::format("{}.{}", sEntityId, iSubEntityId);

        auto result = pObject->serialize(*pTomlData, pOriginalObject, sSubEntityIdSectionName);
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
} // namespace ne
