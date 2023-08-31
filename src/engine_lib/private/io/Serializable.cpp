#include "io/Serializable.h"

// Standard.
#include <typeinfo>

// Custom.
#include "io/serializers/IFieldSerializer.hpp"

// External.
#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include "toml11/toml.hpp"

#include "Serializable.generated_impl.h"

namespace ne {
    std::optional<Error> Serializable::serialize(
        std::filesystem::path pathToFile,
        bool bEnableBackup,
        const std::unordered_map<std::string, std::string>& customAttributes) {
        // Add TOML extension to file.
        if (!pathToFile.string().ends_with(".toml")) {
            pathToFile += ".toml";
        }

        // Make sure file directories exist.
        if (!std::filesystem::exists(pathToFile.parent_path())) {
            std::filesystem::create_directories(pathToFile.parent_path());
        }

#if defined(WIN32)
        // Check if the path length is too long.
        constexpr auto iMaxPathLimitBound = 15;
        constexpr auto iMaxPathLimit = MAX_PATH - iMaxPathLimitBound;
        const auto iFilePathLength = pathToFile.string().length();
        if (iFilePathLength > iMaxPathLimit - (iMaxPathLimitBound * 2) && iFilePathLength < iMaxPathLimit) {
            Logger::get().warn(std::format(
                "file path length {} is close to the platform limit of {} characters (path: {})",
                iFilePathLength,
                iMaxPathLimit,
                pathToFile.string()));
        } else if (iFilePathLength >= iMaxPathLimit) {
            return Error(std::format(
                "file path length {} exceeds the platform limit of {} characters (path: {})",
                iFilePathLength,
                iMaxPathLimit,
                pathToFile.string()));
        }
#endif

        // Serialize data to TOML value.
        toml::value tomlData;
        auto result = serialize(tomlData, "", customAttributes, pathToFile, bEnableBackup);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }

        // Handle backup file.
        std::filesystem::path backupFile = pathToFile;
        backupFile += ConfigManager::getBackupFileExtension();

        if (bEnableBackup) {
            // Check if we already have a file from previous serialization.
            if (std::filesystem::exists(pathToFile)) {
                // Make this old file a backup file.
                if (std::filesystem::exists(backupFile)) {
                    std::filesystem::remove(backupFile);
                }
                std::filesystem::rename(pathToFile, backupFile);
            }
        }

        // Save TOML data to file.
        std::ofstream file(pathToFile, std::ios::binary);
        if (!file.is_open()) {
            return Error(std::format(
                "failed to open the file \"{}\" (maybe because it's marked as read-only)",
                pathToFile.string()));
        }
        file << tomlData;
        file.close();

        if (bEnableBackup) {
            // Create backup file if it does not exist.
            if (!std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(pathToFile, backupFile);
            }
        }

        return {};
    }

    std::variant<std::string, Error> Serializable::serialize(
        toml::value& tomlData,
        const std::string& sEntityId,
        const std::unordered_map<std::string, std::string>& customAttributes,
        const std::optional<std::filesystem::path>& optionalPathToFile,
        bool bEnableBackup) {
        return serialize(tomlData, nullptr, sEntityId, customAttributes, optionalPathToFile, bEnableBackup);
    }

    const rfk::Struct*
    Serializable::getClassForGuid(const rfk::Struct* pArchetypeToAnalyze, const std::string& sGuid) {
        const auto vDirectSubclasses = pArchetypeToAnalyze->getDirectSubclasses();
        for (const auto& pDerivedEntity : vDirectSubclasses) {
            // Get GUID property.
            const auto pGuid = pDerivedEntity->getProperty<Guid>(false);
            if (pGuid == nullptr) {
                const Error err(std::format(
                    "Type {} does not have a GUID assigned to it.\n\n"
                    "Here is an example of how to assign a GUID to your type:\n"
                    "class RCLASS(Guid(\"00000000-0000-0000-0000-000000000000\")) MyCoolClass "
                    ": public ne::Serializable",
                    pDerivedEntity->getName()));
                err.showError();
                throw std::runtime_error(err.getFullErrorMessage());
            }

            if (pGuid->getGuid() == sGuid) {
                return pDerivedEntity;
            }

            const auto pResult = getClassForGuid(pDerivedEntity, sGuid);
            if (pResult != nullptr) {
                return pResult;
            }
        }

        return nullptr;
    }

    const rfk::Class* Serializable::getClassForGuid(const std::string& sGuid) {
        // Get GUID of this class.
        const auto& selfArchetype = Serializable::staticGetArchetype();
        const auto pSelfGuid = selfArchetype.getProperty<Guid>(false);
        if (pSelfGuid == nullptr) {
            const Error err(
                std::format("Type {} does not have a GUID assigned to it.", selfArchetype.getName()));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        if (pSelfGuid->getGuid() == sGuid) {
            return &selfArchetype;
        }

        return getClassForGuid(&selfArchetype, sGuid);
    }

    std::variant<std::set<std::string>, Error>
    Serializable::getIdsFromFile(std::filesystem::path pathToFile) {
        // Add TOML extension to file.
        if (!pathToFile.string().ends_with(".toml")) {
            pathToFile += ".toml";
        }

        // Handle backup file.
        std::filesystem::path backupFile = pathToFile;
        backupFile += ConfigManager::getBackupFileExtension();

        if (!std::filesystem::exists(pathToFile)) {
            // Check if backup file exists.
            if (std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(backupFile, pathToFile);
            } else {
                return Error("file or backup file do not exist");
            }
        }

        // Load file.
        toml::value tomlData;
        try {
            tomlData = toml::parse(pathToFile);
        } catch (std::exception& exception) {
            return Error(
                std::format("failed to load file \"{}\", error: {}", pathToFile.string(), exception.what()));
        }

        // Read all sections.
        std::vector<std::string> vSections;
        const auto fileTable = tomlData.as_table();
        for (const auto& [key, value] : fileTable) {
            if (value.is_table()) {
                vSections.push_back(key);
            }
        }

        // Check that we have at least one section.
        if (vSections.empty()) {
            return Error(std::format(
                "the specified file \"{}\" has 0 sections while expected at least 1 section",
                pathToFile.string()));
        }

        // Cycle over each section and get string before first dot.
        std::set<std::string> vIds;
        for (const auto& sSectionName : vSections) {
            const auto iFirstDotPos = sSectionName.find('.');
            if (iFirstDotPos == std::string::npos) {
                return Error(std::format(
                    "the specified file \"{}\" does not have dots in section names (corrupted file)",
                    pathToFile.string()));
            }

            vIds.insert(sSectionName.substr(0, iFirstDotPos));
        }

        return vIds;
    }

    std::optional<Error> Serializable::serializeMultiple(
        std::filesystem::path pathToFile,
        std::vector<SerializableObjectInformation> vObjects,
        bool bEnableBackup) {
        // Check that all objects are unique.
        for (size_t i = 0; i < vObjects.size(); i++) {
            for (size_t j = 0; j < vObjects.size(); j++) {
                if (i != j && vObjects[i].pObject == vObjects[j].pObject) [[unlikely]] {
                    return Error("the specified array of objects has doubles");
                }
            }
        }

        // Check that IDs are unique and don't have dots in them.
        for (const auto& objectData : vObjects) {
            if (objectData.sObjectUniqueId.empty()) [[unlikely]] {
                return Error("specified an empty object ID");
            }

            if (objectData.sObjectUniqueId.contains('.')) [[unlikely]] {
                return Error(std::format(
                    "the specified object ID \"{}\" is not allowed to have dots in it",
                    objectData.sObjectUniqueId));
            }
            for (const auto& compareObject : vObjects) {
                if (objectData.pObject != compareObject.pObject &&
                    objectData.sObjectUniqueId == compareObject.sObjectUniqueId) [[unlikely]] {
                    return Error("object IDs are not unique");
                }
            }
        }

        // Add TOML extension to file.
        if (!pathToFile.string().ends_with(".toml")) {
            pathToFile += ".toml";
        }

        // Make sure file directories exist.
        if (!std::filesystem::exists(pathToFile.parent_path())) {
            std::filesystem::create_directories(pathToFile.parent_path());
        }

        // Handle backup.
        std::filesystem::path backupFile = pathToFile;
        backupFile += ConfigManager::getBackupFileExtension();

        if (bEnableBackup) {
            // Check if we already have a file from previous serialization.
            if (std::filesystem::exists(pathToFile)) {
                // Make this old file a backup file.
                if (std::filesystem::exists(backupFile)) {
                    std::filesystem::remove(backupFile);
                }
                std::filesystem::rename(pathToFile, backupFile);
            }
        }

#if defined(WIN32)
        // Check if the path length is too long.
        constexpr auto iMaxPathLimitBound = 15;
        constexpr auto iMaxPathLimit = MAX_PATH - iMaxPathLimitBound;
        const auto iFilePathLength = pathToFile.string().length();
        if (iFilePathLength > iMaxPathLimit - (iMaxPathLimitBound * 2) && iFilePathLength < iMaxPathLimit) {
            Logger::get().warn(std::format(
                "file path length {} is close to the platform limit of {} characters (path: {})",
                iFilePathLength,
                iMaxPathLimit,
                pathToFile.string()));
        } else if (iFilePathLength >= iMaxPathLimit) {
            return Error(std::format(
                "file path length {} exceeds the platform limit of {} characters (path: {})",
                iFilePathLength,
                iMaxPathLimit,
                pathToFile.string()));
        }
#endif

        // Serialize.
        toml::value tomlData;
        for (const auto& objectData : vObjects) {
            auto result = objectData.pObject->serialize(
                tomlData,
                objectData.pOriginalObject,
                objectData.sObjectUniqueId,
                objectData.customAttributes,
                pathToFile,
                bEnableBackup);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                return err;
            }
        }

        // Save TOML data to file.
        std::ofstream file(pathToFile, std::ios::binary);
        if (!file.is_open()) {
            return Error(std::format(
                "failed to open the file \"{}\" (maybe because it's marked as read-only)",
                pathToFile.string()));
        }
        file << tomlData;
        file.close();

        if (bEnableBackup) {
            // Create backup file if it does not exist.
            if (!std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(pathToFile, backupFile);
            }
        }

        return {};
    }

    std::optional<std::pair<std::string, std::string>>
    Serializable::getPathDeserializedFromRelativeToRes() const {
        return pathDeserializedFromRelativeToRes;
    }

    std::variant<std::string, Error> Serializable::serialize( // NOLINT: too complex
        toml::value& tomlData,
        Serializable* pOriginalObject,
        std::string sEntityId,
        const std::unordered_map<std::string, std::string>& customAttributes,
        const std::optional<std::filesystem::path>& optionalPathToFile,
        bool bEnableBackup) {
        rfk::Class const& selfArchetype = getArchetype();
        if (sEntityId.empty()) {
            // Put something as entity ID so it would not look weird.
            sEntityId = "0";
        }

        // Check that custom attribute key names are not empty.
        if (!customAttributes.empty() && customAttributes.find("") != customAttributes.end()) {
            Error err("empty attributes are not allowed");
            return err;
        }

        // Check that this type has a GUID.
        const auto pGuid = selfArchetype.getProperty<Guid>(false);
        if (pGuid == nullptr) {
            Error err(
                std::format("type \"{}\" does not have a GUID assigned to it", selfArchetype.getName()));
            return err;
        }

        // Don't require the original object to have a path it was deserialized from (if original object is
        // specified). This is because `serialize` can be called for a `Serializable` field which could
        // have an original object (if field's owner has original object) but fields don't have path
        // to the file they were deserialized from.

        // Create section.
        const auto sSectionName = std::format("{}.{}", sEntityId, pGuid->getGuid());

        // Prepare data.
        struct Data {
            Serializable* self = nullptr;
            rfk::Class const* selfArchetype = nullptr;
            toml::value* pTomlData = nullptr;
            std::string sSectionName;
            std::vector<IFieldSerializer*> vFieldSerializers;
            std::optional<Error> error;
            std::string sEntityId;
            Serializable* pOriginalEntity = nullptr;
            std::optional<std::filesystem::path> optionalPathToFile = {};
            bool bEnableBackup = false;
            gc<Serializable> pGcOriginalEntity = nullptr;
            size_t iSubEntityId = 0;
            size_t iTotalFieldsSerialized = 0;
        };

        Data loopData{
            this,
            &selfArchetype,
            &tomlData,
            sSectionName,
            FieldSerializerManager::getFieldSerializers(),
            {},
            sEntityId,
            pOriginalObject,
            optionalPathToFile,
            bEnableBackup};

        selfArchetype.foreachField(
            [](rfk::Field const& field, void* userData) -> bool { // NOLINT: too complex
                if (!SerializableObjectFieldSerializer::isFieldSerializable(field)) {
                    return true;
                }

                Data* pData = static_cast<Data*>(userData);
                const auto pFieldName = field.getName();

                // We require all reflected field names to be unique (not only here,
                // but this is also required for some serializers).
                try {
                    // Throws if not found.
                    toml::find(*pData->pTomlData, pData->sSectionName, pFieldName);
                } catch (...) {
                    // No field exists with this name in this section - OK.
                    // Save field data in TOML.

                    // Check if there was an original (previously deserialized) object.
                    Serializable* pOriginalFieldObject = nullptr;
                    if (pData->pOriginalEntity != nullptr) {
                        // Check if this field's value was changed compared to the value
                        // in the original file.
                        const auto pOriginalField = pData->pOriginalEntity->getArchetype().getFieldByName(
                            pFieldName, rfk::EFieldFlags::Default, true);
                        if (pOriginalField == nullptr) {
                            pData->error = Error(std::format(
                                "the field \"{}\" (maybe inherited) of type \"{}\" was not found "
                                "in the original entity",
                                field.getName(),
                                pData->selfArchetype->getName()));
                            return false;
                        }

                        // Save a pointer to the field if it's serializable.
                        if (SerializableObjectFieldSerializer::isDerivedFromSerializable(
                                pOriginalField->getType().getArchetype())) {
                            pOriginalFieldObject = static_cast<Serializable*>(
                                pOriginalField->getPtrUnsafe(&*pData->pOriginalEntity));
                        }

                        // Check if value was not changed.
                        bool bIsFoundSerializer = false;
                        for (const auto& pSerializer : pData->vFieldSerializers) {
                            if (pSerializer->isFieldTypeSupported(pOriginalField) &&
                                pSerializer->isFieldTypeSupported(&field)) {
                                bIsFoundSerializer = true;
                                if (pSerializer->isFieldValueEqual(
                                        pData->self, &field, pData->pOriginalEntity, pOriginalField)) {
                                    // Field value was not changed, skip it.
                                    return true;
                                }

                                // Field value was changed, serialize it.
                                break;
                            }
                        }

                        if (!bIsFoundSerializer) {
                            pData->error = Error(std::format(
                                "failed to compare a value of the field \"{}\" of type \"{}\" "
                                "with the field from the original file at \"{}\" (ID \"{}\"), reason: no "
                                "serializer supports both field types (maybe we took the wrong field from "
                                "the original file)",
                                field.getName(),
                                pData->selfArchetype->getName(),
                                pData->pOriginalEntity->getPathDeserializedFromRelativeToRes().value().first,
                                pData->pOriginalEntity->getPathDeserializedFromRelativeToRes()
                                    .value()
                                    .second));
                            return false;
                        }
                    }

                    // Check if need to serialize as external file.
                    const auto pSerializeProperty = field.getProperty<Serialize>();
                    if (pSerializeProperty->getSerializationType() ==
                        FieldSerializationType::FST_AS_EXTERNAL_FILE) {
                        // Make sure this field derives from `Serializable`.
                        if (!SerializableObjectFieldSerializer::isDerivedFromSerializable(
                                field.getType().getArchetype())) [[unlikely]] {
                            // Show an error so that the developer will instantly see the mistake.
                            auto error = Error("only fields of type derived from `Serializable` can use "
                                               "`Serialize(AsExternal)` property");
                            error.showError();
                            throw std::runtime_error(error.getFullErrorMessage());
                        }

                        // Make sure path to the main file is specified.
                        if (!pData->optionalPathToFile.has_value()) [[unlikely]] {
                            pData->error =
                                Error("unable to serialize field marked as `Serialize(AsExternal)` "
                                      "because path to the main file was not specified");
                            return false;
                        }

                        // Get entity ID chain.
                        auto iLastDotPos = pData->sSectionName.rfind('.');
                        if (iLastDotPos == std::string::npos) [[unlikely]] {
                            pData->error =
                                Error(std::format("section name \"{}\" is corrupted", pData->sSectionName));
                            return false;
                        }
                        // Will be something like: "entityId.subEntityId",
                        // "entityId.subEntityId.subSubEntityId" or etc.
                        const auto sEntityIdChain = pData->sSectionName.substr(0, iLastDotPos);

                        // Prepare path to the external file.
                        const auto sExternalFileName = std::format(
                            "{}.{}.{}{}",
                            pData->optionalPathToFile->stem().string(),
                            sEntityIdChain,
                            field.getName(),
                            ConfigManager::getConfigFormatExtension());
                        const auto pathToExternalFile =
                            pData->optionalPathToFile.value().parent_path() / sExternalFileName;

                        // Serialize as external file.
                        Serializable* pFieldObject =
                            reinterpret_cast<Serializable*>(field.getPtrUnsafe(pData->self));
                        auto optionalError =
                            pFieldObject->serialize(pathToExternalFile, pData->bEnableBackup);
                        if (optionalError.has_value()) {
                            pData->error = optionalError.value();
                            pData->error->addCurrentLocationToErrorStack();
                            return false;
                        }

                        // Reference external file in the main file.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](field.getName()) =
                            sExternalFileName;

                        pData->iTotalFieldsSerialized += 1;
                        return true;
                    }

                    // Serialize field.
                    if (pData->vFieldSerializers.empty()) {
                        pData->error =
                            Error("unable to serialize an entity because there are no field serializers "
                                  "registered yet (most likely because no game object was created yet)");
                        return false;
                    }
                    for (const auto& pSerializer : pData->vFieldSerializers) {
                        if (pSerializer->isFieldTypeSupported(&field)) {
                            auto optionalError = pSerializer->serializeField(
                                pData->pTomlData,
                                pData->self,
                                &field,
                                pData->sSectionName,
                                pData->sEntityId,
                                pData->iSubEntityId,
                                pOriginalFieldObject);
                            if (optionalError.has_value()) {
                                pData->error = optionalError.value();
                                pData->error->addCurrentLocationToErrorStack();
                                return false;
                            }

                            pData->iTotalFieldsSerialized += 1;
                            return true;
                        }
                    }

                    pData->error = Error(std::format(
                        "the field \"{}\" with type \"{}\" (maybe inherited) of type \"{}\" has unsupported "
                        "for serialization type",
                        field.getName(),
                        field.getCanonicalTypeName(),
                        pData->selfArchetype->getName()));
                    return false;
                }

                // A field with this name in this section was found.
                // If we continue it will get overwritten.
                pData->error = Error(std::format(
                    "found two fields with the same name \"{}\" in type \"{}\" (maybe inherited)",
                    pFieldName,
                    pData->selfArchetype->getName()));

                return false;
            },
            &loopData,
            true);

        if (loopData.error.has_value()) {
            auto err = std::move(loopData.error.value());
            err.addCurrentLocationToErrorStack();
            return err;
        }

        // Add a note to specify that there's nothing to serialize.
        if (customAttributes.empty() && loopData.iTotalFieldsSerialized == 0) {
            tomlData[sSectionName][sNothingToSerializeKey] = "nothing to serialize here";
        }

        if (loopData.pOriginalEntity != nullptr &&
            loopData.pOriginalEntity->getPathDeserializedFromRelativeToRes().has_value()) {
            // Write path to the original entity.
            tomlData[sSectionName][sPathRelativeToResKey] =
                loopData.pOriginalEntity->getPathDeserializedFromRelativeToRes().value().first;
        }

        // Write custom attributes, they will be written with two dots in the beginning.
        for (const auto& [key, value] : customAttributes) {
            tomlData[sSectionName][std::format("..{}", key)] = value;
        }

        return sSectionName;
    }

} // namespace ne
