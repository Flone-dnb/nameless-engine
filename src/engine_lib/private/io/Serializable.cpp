#include "io/Serializable.h"

// External.
#include "fmt/format.h"

#include "Reflection_impl.hpp"

namespace ne {
    std::optional<Error> Serializable::serialize(
        const std::filesystem::path& pathToFile,
        bool bEnableBackup,
        const std::unordered_map<std::string, std::string>& customAttributes) {
        toml::value tomlData;
        auto result = serialize(tomlData, "", customAttributes);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        // Add TOML extension to file.
        auto fixedPath = pathToFile;
        if (!fixedPath.string().ends_with(".toml")) {
            fixedPath += ".toml";
        }

        std::filesystem::path backupFile = fixedPath;
        backupFile += ConfigManager::getBackupFileExtension();

        if (bEnableBackup) {
            // Check if we already have a file from previous serialization.
            if (std::filesystem::exists(fixedPath)) {
                // Make this old file a backup file.
                if (std::filesystem::exists(backupFile)) {
                    std::filesystem::remove(backupFile);
                }
                std::filesystem::rename(fixedPath, backupFile);
            }
        }

        // Save TOML data to file.
        std::ofstream file(fixedPath, std::ios::binary);
        if (!file.is_open()) {
            return Error(fmt::format("failed to open the file \"{}\"", fixedPath.string()));
        }
        file << tomlData;
        file.close();

        if (bEnableBackup) {
            // Create backup file if it does not exist.
            if (!std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(fixedPath, backupFile);
            }
        }

        return {};
    }

    std::variant<std::string, Error> Serializable::serialize(
        toml::value& tomlData,
        std::string sEntityId,
        const std::unordered_map<std::string, std::string>& customAttributes) {
        rfk::Class const& selfArchetype = getArchetype();
        if (sEntityId.empty()) {
            // Put something as entity ID so it would not look weird.
            sEntityId = "0";
        }

        // Check that custom attribute key names are not empty.
        if (!customAttributes.empty() && customAttributes.find("") != customAttributes.end()) {
            const Error err("empty attributes are not allowed");
            return err;
        }

        // Check that this type has a GUID.
        const auto pGuid = selfArchetype.getProperty<Guid>(false);
        if (!pGuid) {
            const Error err(
                fmt::format("type {} does not have a GUID assigned to it", selfArchetype.getName()));
            return err;
        }

        // Create section.
        const auto sSectionName = fmt::format("{}.{}", sEntityId, pGuid->getGuid());

        struct Data {
            Serializable* self = nullptr;
            rfk::Class const* selfArchetype = nullptr;
            toml::value* pTomlData = nullptr;
            std::string sSectionName;
            std::optional<Error> error = {};
            std::string sEntityId;
            size_t iSubEntityId = 0;
            size_t iTotalFieldsSerialized = 0;
        };

        Data loopData{this, &selfArchetype, &tomlData, sSectionName, {}, sEntityId};

        selfArchetype.foreachField(
            [](rfk::Field const& field, void* userData) -> bool {
                const auto& fieldType = field.getType();

                if (!isFieldSerializable(field))
                    return true;

                Data* pData = static_cast<Data*>(userData);
                const auto sFieldName = field.getName();
                const auto sFieldCanonicalTypeName = std::string(field.getCanonicalTypeName());

                try {
                    // Throws if not found.
                    toml::find(*pData->pTomlData, pData->sSectionName, sFieldName);
                } catch (...) {
                    // ----------------------------------------------------------------------------
                    // No field exists with this name in this section - OK.
                    // Look at field type and save it in TOML data.
                    // ----------------------------------------------------------------------------
                    // Primitive types.
                    // ----------------------------------------------------------------------------
                    if (fieldType.match(rfk::getType<bool>())) {
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<bool>(pData->self);
                    } else if (fieldType.match(rfk::getType<int>())) {
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<int>(pData->self);
                    } else if (fieldType.match(rfk::getType<long long>())) {
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<long long>(pData->self);
                    } else if (fieldType.match(rfk::getType<float>())) {
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<float>(pData->self);
                    } else if (fieldType.match(rfk::getType<double>())) {
                        // Store double as string for better precision.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            toml::format(toml::value(field.getUnsafe<double>(pData->self)));
                    }
                    // ----------------------------------------------------------------------------
                    // STL types.
                    // ----------------------------------------------------------------------------
                    // non-reflected STL types have equal types in Refureku
                    // thus add additional checks
                    // ----------------------------------------------------------------------------
                    else if (sFieldCanonicalTypeName == sStringCanonicalTypeName) {
                        // Field type is `std::string`.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<std::string>(pData->self);
                    } else if (sFieldCanonicalTypeName.starts_with("std::vector<")) {
                        if (serializeVectorField(
                                pData->pTomlData, pData->self, &field, pData->sSectionName)) {
                            pData->error = Error(fmt::format(
                                "vector field \"{}\" (maybe inherited) of class \"{}\" has unsupported for "
                                "serialization inner type",
                                field.getName(),
                                pData->selfArchetype->getName()));
                            return false;
                        }
                    } else if (sFieldCanonicalTypeName.starts_with("std::unordered_map<")) {
                        if (serializeUnorderedMapField(
                                pData->pTomlData, pData->self, &field, pData->sSectionName)) {
                            pData->error = Error(fmt::format(
                                "unordered map field \"{}\" (maybe inherited) of class \"{}\" has "
                                "unsupported for serialization inner type",
                                field.getName(),
                                pData->selfArchetype->getName()));
                            return false;
                        }
                    }
                    // ----------------------------------------------------------------------------
                    // Custom reflected types.
                    // ----------------------------------------------------------------------------
                    else if (
                        fieldType.getArchetype() && isDerivedFromSerializable(fieldType.getArchetype())) {
                        // Field with a reflected type.
                        // Check that this type has GUID.
                        if (!fieldType.getArchetype()->getProperty<Guid>(false)) {
                            const Error err(fmt::format(
                                "type {} does not have a GUID assigned to it",
                                fieldType.getArchetype()->getName()));
                            pData->error = std::move(err);
                            return true;
                        }

                        // Add a key to specify that this value has a reflected type.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            "reflected type, see other sub-section";

                        // Serialize this field "under our ID".
                        const std::string sSubEntityIdSectionName =
                            fmt::format("{}.{}", pData->sEntityId, pData->iSubEntityId);
                        const auto pSubEntity = static_cast<Serializable*>(field.getPtrUnsafe(pData->self));
                        auto result = pSubEntity->serialize(*pData->pTomlData, sSubEntityIdSectionName);
                        if (std::holds_alternative<Error>(result)) {
                            pData->error = std::get<Error>(std::move(result));
                            pData->error->addEntry();
                            return true;
                        }
                        const auto sSubEntityFinalSectionName = std::get<std::string>(result);
                        pData->iSubEntityId += 1;

                        // Add a new key ".field_name" to this sub entity so that we will know to which
                        // field this entity should be assigned.
                        pData->pTomlData->operator[](sSubEntityFinalSectionName)
                            .
                            operator[](sSubEntityFieldNameKey) = sFieldName;
                    }
                    // ----------------------------------------------------------------------------
                    // Other.
                    // ----------------------------------------------------------------------------
                    else {
                        pData->error = Error(fmt::format(
                            "field \"{}\" (maybe inherited) of class \"{}\" has unsupported for "
                            "serialization type",
                            field.getName(),
                            pData->selfArchetype->getName()));
                        return false;
                    }

                    pData->iTotalFieldsSerialized += 1;
                    return true;
                }

                // A field with this name in this section was found.
                // If we continue it will get overwritten.
                pData->error = Error(fmt::format(
                    "found two fields with the same name \"{}\" in class \"{}\" (maybe inherited)",
                    sFieldName,
                    pData->selfArchetype->getName()));

                return false;
            },
            &loopData,
            true);

        if (loopData.error.has_value()) {
            auto err = std::move(loopData.error.value());
            err.addEntry();
            return err;
        }

        // Add at least one value to be valid TOML data.
        if (customAttributes.empty() && loopData.iTotalFieldsSerialized == 0) {
            tomlData[sSectionName][sNothingToSerializeKey] = "nothing to serialize here";
        }

        // Write custom attributes, they will be written with two dots in the beginning.
        for (const auto& [key, value] : customAttributes) {
            tomlData[sSectionName][fmt::format("..{}", key)] = value;
        }

        return sSectionName;
    }

#if defined(DEBUG)
    void Serializable::checkGuidUniqueness() {
        // Record start time.
        const auto startTime = std::chrono::steady_clock::now();

        // Map of GUIDs (key) and type names (value).
        std::unordered_map<std::string, std::string> vGuids;

        // Get GUID of this class.
        const auto& selfArchetype = staticGetArchetype();
        const auto pSelfGuid = selfArchetype.getProperty<Guid>(false);
        if (!pSelfGuid) {
            const Error err(
                fmt::format("Type {} does not have a GUID assigned to it.", selfArchetype.getName()));
            err.showError();
            throw std::runtime_error(err.getError());
        }
        vGuids[pSelfGuid->getGuid()] = selfArchetype.getName();

        collectGuids(&selfArchetype, vGuids);

        // Calculate time it took for us to do all this.
        const auto endTime = std::chrono::steady_clock::now();
        const auto durationInMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        const float timeTookInSec = static_cast<float>(durationInMs) / 1000.0f;

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

    void Serializable::collectGuids(
        const rfk::Struct* pArchetypeToAnalyze, std::unordered_map<std::string, std::string>& vAllGuids) {
        const auto vDirectSubclasses = pArchetypeToAnalyze->getDirectSubclasses();
        for (const auto& pDerivedEntity : vDirectSubclasses) {
            const auto pGuid = pDerivedEntity->getProperty<Guid>(false);
            if (!pGuid) {
                const Error err(fmt::format(
                    "Type {} does not have a GUID assigned to it.\n\n"
                    "Here is an example of how to assign a GUID to your type:\n"
                    "class RCLASS(Guid(\"00000000-0000-0000-0000-000000000000\")) MyCoolClass "
                    ": public ne::Serializable",
                    pDerivedEntity->getName()));
                err.showError();
                throw std::runtime_error(err.getError());
            }

            // Look if this GUID is already used.
            const auto it = vAllGuids.find(pGuid->getGuid());
            if (it != vAllGuids.end()) [[unlikely]] {
                const Error err(fmt::format(
                    "GUID of type {} is already used by type {}, please generate another "
                    "GUID.",
                    pDerivedEntity->getName(),
                    it->second));
                err.showError();
                throw std::runtime_error(err.getError());
            }

            // Add this GUID.
            vAllGuids[pGuid->getGuid()] = pDerivedEntity->getName();

            // Go though all children.
            collectGuids(pDerivedEntity, vAllGuids);
        }
    }
#endif

    bool Serializable::isFieldSerializable(rfk::Field const& field) {
        const auto& fieldType = field.getType();

        // Don't serialize specific types.
        if (fieldType.isConst() || fieldType.isPointer() || fieldType.isLValueReference() ||
            fieldType.isRValueReference() || fieldType.isCArray())
            return false;

        // Ignore this field if marked as DontSerialize.
        if (field.getProperty<DontSerialize>())
            return false;

        return true;
    }

    bool Serializable::isDerivedFromSerializable(rfk::Archetype const* pArchetype) {
        if (rfk::Class const* pClass = rfk::classCast(pArchetype)) {
            // Check parents.
            if (pClass->isSubclassOf(Serializable::staticGetArchetype()))
                return true;

            // Check if Serializable.
            const auto pGuid = pClass->getProperty<Guid>(false);
            if (!pGuid)
                return false;

            if (pGuid->getGuid() == Serializable::staticGetArchetype().getProperty<Guid>(false)->getGuid())
                return true;

            return false;
        } else if (rfk::Struct const* pStruct = rfk::structCast(pArchetype)) {
            // Check parents.
            if (pStruct->isSubclassOf(Serializable::staticGetArchetype()))
                return true;

            return false;
        } else {
            return false;
        }
    }

    std::optional<Error> Serializable::cloneSerializableObject(Serializable* pFrom, Serializable* pTo) {
        rfk::Class const& fromArchetype = pFrom->getArchetype();
        rfk::Class const& toArchetype = pTo->getArchetype();

        // Check if types are equal.
        const auto pFromGuid = fromArchetype.getProperty<Guid>(false);
        const auto pToGuid = toArchetype.getProperty<Guid>(false);
        if (!pFromGuid) {
            const Error err(
                fmt::format("type {} does not have a GUID assigned to it", fromArchetype.getName()));
            return err;
        }
        if (!pToGuid) {
            const Error err(
                fmt::format("type {} does not have a GUID assigned to it", toArchetype.getName()));
            return err;
        }

        if (pFromGuid->getGuid() != pToGuid->getGuid()) {
            return Error(fmt::format(
                "types \"{}\" and \"{}\" are not the same", fromArchetype.getName(), toArchetype.getName()));
        }

        struct Data {
            Serializable* pFrom;
            Serializable* pTo;
            std::optional<Error> error;
        };

        Data loopData{pFrom, pTo, std::optional<Error>{}};

        fromArchetype.foreachField(
            [](rfk::Field const& field, void* userData) -> bool {
                if (!isFieldSerializable(field))
                    return true;

                const auto& fieldType = field.getType();
                Data* pData = static_cast<Data*>(userData);
                const auto sFieldName = field.getName();
                const auto sFieldCanonicalTypeName = std::string(field.getCanonicalTypeName());

                const auto* pFieldTo =
                    pData->pTo->getArchetype().getFieldByName(sFieldName, rfk::EFieldFlags::Default, true);

                if (cloneFieldIfMatchesPrimitiveType<bool>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesPrimitiveType<int>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesPrimitiveType<long long>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesPrimitiveType<float>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesPrimitiveType<double>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;

                // ----------------------------------------------------------------------------
                // STL types.
                // ----------------------------------------------------------------------------
                // non-reflected STL types have equal types in Refureku
                // thus add additional checks
                // ----------------------------------------------------------------------------
                if (sFieldCanonicalTypeName == sStringCanonicalTypeName) {
                    // Field type is `std::string`.
                    auto value = field.getUnsafe<std::string>(pData->pFrom);
                    pFieldTo->setUnsafe<std::string>(pData->pTo, std::move(value));
                } else if (sFieldCanonicalTypeName.starts_with("std::vector<")) {
                    if (cloneVectorField(pData->pFrom, &field, pData->pTo, pFieldTo)) {
                        Error err(fmt::format(
                            "vector field \"{}\" has unsupported for serialization inner type",
                            field.getName()));
                        return false;
                    }
                } else if (sFieldCanonicalTypeName.starts_with("std::unordered_map")) {
                    if (cloneUnorderedMapField(pData->pFrom, &field, pData->pTo, pFieldTo)) {
                        Error err(fmt::format(
                            "unordered map field \"{}\" has unsupported for serialization inner type",
                            field.getName()));
                        return false;
                    }
                }
                // ----------------------------------------------------------------------------
                // Custom reflected types.
                // ----------------------------------------------------------------------------
                else if (fieldType.getArchetype() && isDerivedFromSerializable(fieldType.getArchetype())) {
                    auto optionalError = cloneSerializableObject(
                        static_cast<Serializable*>(field.getPtrUnsafe(pData->pFrom)),
                        static_cast<Serializable*>(pFieldTo->getPtrUnsafe(pData->pTo)));
                    if (optionalError.has_value()) {
                        auto err = std::move(optionalError.value());
                        err.addEntry();
                        pData->error = err;
                        return false;
                    }
                } else {
                    Error err(
                        fmt::format("field \"{}\" has unsupported for serialization type", field.getName()));
                    return false;
                }

                return true;
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

    const rfk::Struct*
    Serializable::getClassForGuid(const rfk::Struct* pArchetypeToAnalyze, const std::string& sGuid) {
        const auto vDirectSubclasses = pArchetypeToAnalyze->getDirectSubclasses();
        for (const auto& pDerivedEntity : vDirectSubclasses) {
            // Get GUID property.
            const auto pGuid = pDerivedEntity->getProperty<Guid>(false);
            if (!pGuid) {
                const Error err(fmt::format(
                    "Type {} does not have a GUID assigned to it.\n\n"
                    "Here is an example of how to assign a GUID to your type:\n"
                    "class RCLASS(Guid(\"00000000-0000-0000-0000-000000000000\")) MyCoolClass "
                    ": public ne::Serializable",
                    pDerivedEntity->getName()));
                err.showError();
                throw std::runtime_error(err.getError());
            }

            if (pGuid->getGuid() == sGuid) {
                return pDerivedEntity;
            } else {
                const auto pResult = getClassForGuid(pDerivedEntity, sGuid);
                if (pResult) {
                    return pResult;
                } else {
                    continue;
                }
            }
        }

        return nullptr;
    }

    const rfk::Class* Serializable::getClassForGuid(const std::string& sGuid) {
        // Get GUID of this class.
        const auto& selfArchetype = Serializable::staticGetArchetype();
        const auto pSelfGuid = selfArchetype.getProperty<Guid>(false);
        if (!pSelfGuid) {
            const Error err(
                fmt::format("Type {} does not have a GUID assigned to it.", selfArchetype.getName()));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        if (pSelfGuid->getGuid() == sGuid) {
            return &selfArchetype;
        }

        return getClassForGuid(&selfArchetype, sGuid);
    }

    std::variant<std::vector<DeserializedObjectInformation>, Error>
    Serializable::deserialize(const std::filesystem::path& pathToFile, const std::set<std::string>& ids) {
        // Check that specified IDs don't have dots in them.
        for (const auto& sId : ids) {
            if (sId.contains('.')) [[unlikely]] {
                return Error(
                    fmt::format("the specified object ID \"{}\" is not allowed to have dots in it", sId));
            }
        }

        // Add TOML extension to file.
        auto fixedPath = pathToFile;
        if (!fixedPath.string().ends_with(".toml")) {
            fixedPath += ".toml";
        }

        std::filesystem::path backupFile = fixedPath;
        backupFile += ConfigManager::getBackupFileExtension();

        if (!std::filesystem::exists(fixedPath)) {
            // Check if backup file exists.
            if (std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(backupFile, fixedPath);
            } else {
                return Error("file or backup file do not exist");
            }
        }

        // Load file.
        toml::value tomlData;
        try {
            tomlData = toml::parse(fixedPath);
        } catch (std::exception& exception) {
            return Error(
                fmt::format("failed to load file \"{}\", error: {}", fixedPath.string(), exception.what()));
        }

        // Deserialize.
        std::vector<DeserializedObjectInformation> deserializedObjects;
        for (const auto& sId : ids) {
            std::unordered_map<std::string, std::string> customAttributes;
            auto result = deserialize<Serializable>(tomlData, customAttributes, sId);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            const auto pObject = std::get<gc<Serializable>>(std::move(result));
            DeserializedObjectInformation objectInfo(pObject, sId, customAttributes);
            deserializedObjects.push_back(std::move(objectInfo));
        }

        return deserializedObjects;
    }

    std::variant<std::set<std::string>, Error>
    Serializable::getIdsFromFile(const std::filesystem::path& pathToFile) {
        // Add TOML extension to file.
        auto fixedPath = pathToFile;
        if (!fixedPath.string().ends_with(".toml")) {
            fixedPath += ".toml";
        }

        // Handle backup file.
        std::filesystem::path backupFile = fixedPath;
        backupFile += ConfigManager::getBackupFileExtension();

        if (!std::filesystem::exists(fixedPath)) {
            // Check if backup file exists.
            if (std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(backupFile, fixedPath);
            } else {
                return Error("file or backup file do not exist");
            }
        }

        // Load file.
        toml::value tomlData;
        try {
            tomlData = toml::parse(fixedPath);
        } catch (std::exception& exception) {
            return Error(
                fmt::format("failed to load file \"{}\", error: {}", fixedPath.string(), exception.what()));
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
            return Error(fmt::format(
                "the specified file \"{}\" has 0 sections while expected at least 1 section",
                fixedPath.string()));
        }

        // Cycle over each section and get string before first dot.
        std::set<std::string> vIds;
        for (const auto& sSectionName : vSections) {
            const auto iFirstDotPos = sSectionName.find('.');
            if (iFirstDotPos == std::string::npos) {
                return Error(fmt::format(
                    "the specified file \"{}\" does not have dots in section names (corrupted file)",
                    fixedPath.string()));
            }

            vIds.insert(sSectionName.substr(0, iFirstDotPos));
        }

        return vIds;
    }

    std::optional<Error> Serializable::serialize(
        const std::filesystem::path& pathToFile,
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
                return Error(fmt::format(
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

        // Serialize.
        toml::value tomlData;
        for (const auto& objectData : vObjects) {
            auto result = objectData.pObject->serialize(
                tomlData, objectData.sObjectUniqueId, objectData.customAttributes);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }
        }

        // Add TOML extension to file.
        auto fixedPath = pathToFile;
        if (!fixedPath.string().ends_with(".toml")) {
            fixedPath += ".toml";
        }

        std::filesystem::path backupFile = fixedPath;
        backupFile += ConfigManager::getBackupFileExtension();

        if (bEnableBackup) {
            // Check if we already have a file from previous serialization.
            if (std::filesystem::exists(fixedPath)) {
                // Make this old file a backup file.
                if (std::filesystem::exists(backupFile)) {
                    std::filesystem::remove(backupFile);
                }
                std::filesystem::rename(fixedPath, backupFile);
            }
        }

        // Save TOML data to file.
        std::ofstream file(fixedPath, std::ios::binary);
        if (!file.is_open()) {
            return Error(fmt::format("failed to open the file \"{}\"", fixedPath.string()));
        }
        file << tomlData;
        file.close();

        if (bEnableBackup) {
            // Create backup file if it does not exist.
            if (!std::filesystem::exists(backupFile)) {
                std::filesystem::copy_file(fixedPath, backupFile);
            }
        }

        return {};
    }

    bool Serializable::cloneVectorField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            // Field type is `std::vector<bool>`.
            auto value = pFromField->getUnsafe<std::vector<bool>>(pFromInstance);
            pToField->setUnsafe<std::vector<bool>>(pToInstance, std::move(value));
            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            // Field type is `std::vector<int>`.
            auto value = pFromField->getUnsafe<std::vector<int>>(pFromInstance);
            pToField->setUnsafe<std::vector<int>>(pToInstance, std::move(value));
            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<long long>") {
            // Field type is `std::vector<long long>`.
            auto value = pFromField->getUnsafe<std::vector<long long>>(pFromInstance);
            pToField->setUnsafe<std::vector<long long>>(pToInstance, std::move(value));
            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            // Field type is `std::vector<float>`.
            auto value = pFromField->getUnsafe<std::vector<float>>(pFromInstance);
            pToField->setUnsafe<std::vector<float>>(pToInstance, std::move(value));
            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            // Field type is `std::vector<double>`.
            auto value = pFromField->getUnsafe<std::vector<double>>(pFromInstance);
            pToField->setUnsafe<std::vector<double>>(pToInstance, std::move(value));
            return false;
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            // Field type is `std::vector<std::string>`.
            auto value = pFromField->getUnsafe<std::vector<std::string>>(pFromInstance);
            pToField->setUnsafe<std::vector<std::string>>(pToInstance, std::move(value));
            return false;
        }

        return true;
    }

    bool Serializable::serializeVectorField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName) {
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());
        const auto sFieldName = pField->getName();

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            // Field type is `std::vector<bool>`.
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<bool>>(pFieldOwner);

            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            // Field type is `std::vector<int>`.
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<int>>(pFieldOwner);

            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<long long>") {
            // Field type is `std::vector<long long>`.
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<long long>>(pFieldOwner);

            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            // Field type is `std::vector<float>`.
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<float>>(pFieldOwner);

            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            // Field type is `std::vector<double>`.
            const std::vector<double> vArray = pField->getUnsafe<std::vector<double>>(pFieldOwner);
            // Store double as string for better precision.
            std::vector<std::string> vStrArray;
            for (const auto& item : vArray) {
                vStrArray.push_back(toml::format(toml::value(item)));
            }
            pTomlData->operator[](sSectionName).operator[](sFieldName) = vStrArray;

            return false;
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            // Field type is `std::vector<std::string>`.
            pTomlData->operator[](sSectionName).operator[](sFieldName) =
                pField->getUnsafe<std::vector<std::string>>(pFieldOwner);

            return false;
        }

        return true;
    }

    bool Serializable::deserializeVectorField(
        toml::value* pTomlData, Serializable* pFieldOwner, const rfk::Field* pField) {
        if (!pTomlData->is_array()) {
            return true;
        }

        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        if (sFieldCanonicalTypeName == "std::vector<bool>") {
            // Field type is `std::vector<bool>`.
            auto fieldValue = pTomlData->as_array();
            std::vector<bool> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_boolean()) {
                    return true;
                }
                vArray.push_back(item.as_boolean());
            }
            pField->setUnsafe<std::vector<bool>>(pFieldOwner, std::move(vArray));

            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<int>") {
            // Field type is `std::vector<int>`.
            auto fieldValue = pTomlData->as_array();
            std::vector<int> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_integer()) {
                    return true;
                }
                vArray.push_back(static_cast<int>(item.as_integer()));
            }
            pField->setUnsafe<std::vector<int>>(pFieldOwner, std::move(vArray));

            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<long long>") {
            // Field type is `std::vector<long long>`.
            auto fieldValue = pTomlData->as_array();
            std::vector<long long> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_integer()) {
                    return true;
                }
                vArray.push_back(item.as_integer());
            }
            pField->setUnsafe<std::vector<long long>>(pFieldOwner, std::move(vArray));

            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<float>") {
            // Field type is `std::vector<float>`.
            auto fieldValue = pTomlData->as_array();
            std::vector<float> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_floating()) {
                    return true;
                }
                vArray.push_back(static_cast<float>(item.as_floating()));
            }
            pField->setUnsafe<std::vector<float>>(pFieldOwner, std::move(vArray));

            return false;
        } else if (sFieldCanonicalTypeName == "std::vector<double>") {
            // Field type is `std::vector<double>`.
            // Double is stored as a string for better precision.
            auto fieldValue = pTomlData->as_array();
            std::vector<double> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_string()) {
                    return true;
                }
                try {
                    vArray.push_back(std::stod(item.as_string().str));
                } catch (...) {
                    return true;
                }
            }
            pField->setUnsafe<std::vector<double>>(pFieldOwner, std::move(vArray));

            return false;
        } else if (sFieldCanonicalTypeName == fmt::format("std::vector<{}>", sStringCanonicalTypeName)) {
            // Field type is `std::vector<std::string>`.
            auto fieldValue = pTomlData->as_array();
            std::vector<std::string> vArray;
            for (const auto& item : fieldValue) {
                if (!item.is_string()) {
                    return true;
                }
                vArray.push_back(item.as_string());
            }
            pField->setUnsafe<std::vector<std::string>>(pFieldOwner, std::move(vArray));

            return false;
        }

        return true;
    }

    bool Serializable::serializeUnorderedMapField(
        toml::value* pTomlData,
        Serializable* pFieldOwner,
        const rfk::Field* pField,
        const std::string& sSectionName) {
        // Define a helper macro...
#define SERIALIZE_UNORDERED_MAP_TYPE(TYPEA, TYPEB)                                                           \
    if (sFieldCanonicalTypeName == fmt::format("std::unordered_map<{}, {}>", #TYPEA, #TYPEB)) {              \
        const auto originalMap = pField->getUnsafe<std::unordered_map<TYPEA, TYPEB>>(pFieldOwner);           \
        std::unordered_map<std::string, TYPEB> map;                                                          \
        for (const auto& [key, value] : originalMap) {                                                       \
            map[fmt::format("{}", key)] = value;                                                             \
        }                                                                                                    \
        pTomlData->operator[](sSectionName).operator[](sFieldName) = map;                                    \
        return false;                                                                                        \
    }
        // and another helper macro.
#define SERIALIZE_UNORDERED_MAP_TYPES(TYPE)                                                                  \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, bool)                                                                 \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, int)                                                                  \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, long long)                                                            \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, float)                                                                \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, double)                                                               \
    SERIALIZE_UNORDERED_MAP_TYPE(TYPE, std::basic_string<char>)

        const auto sFieldName = pField->getName();
        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        SERIALIZE_UNORDERED_MAP_TYPES(bool)
        SERIALIZE_UNORDERED_MAP_TYPES(int)
        SERIALIZE_UNORDERED_MAP_TYPES(long long)
        SERIALIZE_UNORDERED_MAP_TYPES(float)
        SERIALIZE_UNORDERED_MAP_TYPES(double)
        SERIALIZE_UNORDERED_MAP_TYPES(std::basic_string<char>)

        return true;
    }

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
        if (!value.is_floating()) {
            return {};
        }
        return value.as_floating();
    }

    template <> std::optional<std::string> convertTomlValueToType<std::string>(const toml::value& value) {
        if (!value.is_string()) {
            return {};
        }
        return value.as_string().str;
    }

    // ------------------------------------------------------------------------------------------------

    bool Serializable::deserializeUnorderedMapField(
        toml::value* pTomlData, Serializable* pFieldOwner, const rfk::Field* pField) {
        if (!pTomlData->is_table()) {
            return true;
        }

// Define another helper macro...
#define DESERIALIZE_UNORDERED_MAP_TYPE(TYPEA, TYPEB)                                                         \
    if (sFieldCanonicalTypeName == fmt::format("std::unordered_map<{}, {}>", #TYPEA, #TYPEB)) {              \
        auto fieldValue = pTomlData->as_table();                                                             \
        std::unordered_map<TYPEA, TYPEB> map;                                                                \
        for (const auto& [sKey, value] : fieldValue) {                                                       \
            TYPEA castedKey;                                                                                 \
            const auto optionalKey = convertStringToType<TYPEA>(sKey);                                       \
            if (!optionalKey.has_value())                                                                    \
                return true;                                                                                 \
            castedKey = optionalKey.value();                                                                 \
            TYPEB castedValue;                                                                               \
            const auto optionalValue = convertTomlValueToType<TYPEB>(value);                                 \
            if (!optionalValue.has_value())                                                                  \
                return true;                                                                                 \
            castedValue = optionalValue.value();                                                             \
            map[castedKey] = castedValue;                                                                    \
        }                                                                                                    \
        pField->setUnsafe<std::unordered_map<TYPEA, TYPEB>>(pFieldOwner, std::move(map));                    \
        return false;                                                                                        \
    }

// and another helper macro.
#define DESERIALIZE_UNORDERED_MAP_TYPES(TYPE)                                                                \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, bool)                                                               \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, int)                                                                \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, long long)                                                          \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, float)                                                              \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, double)                                                             \
    DESERIALIZE_UNORDERED_MAP_TYPE(TYPE, std::basic_string<char>)

        const auto sFieldCanonicalTypeName = std::string(pField->getCanonicalTypeName());

        DESERIALIZE_UNORDERED_MAP_TYPES(bool)
        DESERIALIZE_UNORDERED_MAP_TYPES(int)
        DESERIALIZE_UNORDERED_MAP_TYPES(long long)
        DESERIALIZE_UNORDERED_MAP_TYPES(float)
        DESERIALIZE_UNORDERED_MAP_TYPES(double)
        DESERIALIZE_UNORDERED_MAP_TYPES(std::basic_string<char>)

        return true;
    }

    bool Serializable::cloneUnorderedMapField(
        Serializable* pFromInstance,
        const rfk::Field* pFromField,
        Serializable* pToInstance,
        const rfk::Field* pToField) {
        const auto sFieldCanonicalTypeName = std::string(pFromField->getCanonicalTypeName());

// Define another helper macro...
#define CLONE_UNORDERED_MAP_TYPE(TYPEA, TYPEB)                                                               \
    if (sFieldCanonicalTypeName == fmt::format("std::unordered_map<{}, {}>", #TYPEA, #TYPEB)) {              \
        auto value = pFromField->getUnsafe<std::unordered_map<TYPEA, TYPEB>>(pFromInstance);                 \
        pToField->setUnsafe<std::unordered_map<TYPEA, TYPEB>>(pToInstance, std::move(value));                \
        return false;                                                                                        \
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

        return true;
    }

} // namespace ne
