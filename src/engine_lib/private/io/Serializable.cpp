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
                    else if (field.getCanonicalTypeName() == sStringCanonicalTypeName) {
                        // Field type is `std::string`.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<std::string>(pData->self);
                    } else if (field.getCanonicalTypeName() == sVectorBoolCanonicalTypeName) {
                        // Field type is `std::vector<bool>`.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<std::vector<bool>>(pData->self);
                    } else if (field.getCanonicalTypeName() == sVectorIntCanonicalTypeName) {
                        // Field type is `std::vector<int>`.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<std::vector<int>>(pData->self);
                    } else if (field.getCanonicalTypeName() == sVectorLongLongCanonicalTypeName) {
                        // Field type is `std::vector<long long>`.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<std::vector<long long>>(pData->self);
                    } else if (field.getCanonicalTypeName() == sVectorFloatCanonicalTypeName) {
                        // Field type is `std::vector<float>`.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<std::vector<float>>(pData->self);
                    } else if (field.getCanonicalTypeName() == sVectorDoubleCanonicalTypeName) {
                        // Field type is `std::vector<double>`.
                        const std::vector<double> vArray = field.getUnsafe<std::vector<double>>(pData->self);
                        // Store double as string for better precision.
                        std::vector<std::string> vStrArray;
                        for (const auto& item : vArray) {
                            vStrArray.push_back(toml::format(toml::value(item)));
                        }
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) = vStrArray;
                    } else if (field.getCanonicalTypeName() == sVectorStringCanonicalTypeName) {
                        // Field type is `std::vector<std::string>`.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<std::vector<std::string>>(pData->self);
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
                if (field.getCanonicalTypeName() == sStringCanonicalTypeName) {
                    // Field type is `std::string`.
                    auto value = field.getUnsafe<std::string>(pData->pFrom);
                    pFieldTo->setUnsafe<std::string>(pData->pTo, std::move(value));
                } else if (field.getCanonicalTypeName() == sVectorBoolCanonicalTypeName) {
                    // Field type is `std::vector<bool>`.
                    auto value = field.getUnsafe<std::vector<bool>>(pData->pFrom);
                    pFieldTo->setUnsafe<std::vector<bool>>(pData->pTo, std::move(value));
                } else if (field.getCanonicalTypeName() == sVectorIntCanonicalTypeName) {
                    // Field type is `std::vector<int>`.
                    auto value = field.getUnsafe<std::vector<int>>(pData->pFrom);
                    pFieldTo->setUnsafe<std::vector<int>>(pData->pTo, std::move(value));
                } else if (field.getCanonicalTypeName() == sVectorLongLongCanonicalTypeName) {
                    // Field type is `std::vector<long long>`.
                    auto value = field.getUnsafe<std::vector<long long>>(pData->pFrom);
                    pFieldTo->setUnsafe<std::vector<long long>>(pData->pTo, std::move(value));
                } else if (field.getCanonicalTypeName() == sVectorFloatCanonicalTypeName) {
                    // Field type is `std::vector<float>`.
                    auto value = field.getUnsafe<std::vector<float>>(pData->pFrom);
                    pFieldTo->setUnsafe<std::vector<float>>(pData->pTo, std::move(value));
                } else if (field.getCanonicalTypeName() == sVectorDoubleCanonicalTypeName) {
                    // Field type is `std::vector<double>`.
                    auto value = field.getUnsafe<std::vector<double>>(pData->pFrom);
                    pFieldTo->setUnsafe<std::vector<double>>(pData->pTo, std::move(value));
                } else if (field.getCanonicalTypeName() == sVectorStringCanonicalTypeName) {
                    // Field type is `std::vector<std::string>`.
                    auto value = field.getUnsafe<std::vector<std::string>>(pData->pFrom);
                    pFieldTo->setUnsafe<std::vector<std::string>>(pData->pTo, std::move(value));
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

} // namespace ne
