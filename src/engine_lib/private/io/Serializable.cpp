#include "io/Serializable.h"

// STL.
#include <ranges>

// Custom.
#include "io/Logger.h"
#include "io/ConfigManager.h"

// External.
#include "Refureku/Refureku.h"

#include "Serializable.generated_impl.h"

namespace ne {
    std::optional<Error>
    Serializable::serialize(const std::filesystem::path& pathToFile, bool bEnableBackup) {
        toml::value tomlData;
        auto result = serialize(tomlData);
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
                if (std::filesystem::exists(backupFile)) {
                    std::filesystem::remove(backupFile);
                }
                std::filesystem::rename(fixedPath, backupFile);
            }
        }

        // Save TOML data to file.
        std::ofstream file(fixedPath, std::ios::binary);
        if (!file.is_open()) {
            return Error(std::format("failed to open the file \"{}\"", fixedPath.string()));
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

    std::variant<std::string, Error> Serializable::serialize(toml::value& tomlData, std::string sEntityId) {
        rfk::Class const& selfArchetype = getArchetype();
        if (sEntityId.empty()) {
            // Put something as entity ID so it would not look weird.
            sEntityId = "0";
        }

        const auto sSectionName = std::format("{}.{}", sEntityId, std::to_string(selfArchetype.getId()));

        struct Data {
            Serializable* self = nullptr;
            rfk::Class const* selfArchetype = nullptr;
            toml::value* pTomlData = nullptr;
            std::string sSectionName;
            std::optional<Error> error = {};
            std::string sEntityId;
            size_t iSubEntityId = 0;
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
                    // No field exists with this name in this section - OK.
                    // Look at field type and save it in TOML data.
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
                    } else if (fieldType.match(rfk::getType<std::string>())) {
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            field.getUnsafe<std::string>(pData->self);
                    } else if (
                        fieldType.getArchetype() && isDerivedFromSerializable(fieldType.getArchetype())) {
                        // Field with a reflected type.
                        // Add a key to specify that this value has a reflected type.
                        pData->pTomlData->operator[](pData->sSectionName).operator[](sFieldName) =
                            "reflected type, see other sub-section";

                        // Serialize this field "under our ID".
                        const std::string sSubEntityIdSectionName =
                            std::format("{}.{}", pData->sEntityId, pData->iSubEntityId);
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
                    } else {
                        pData->error = Error(std::format(
                            "field \"{}\" (maybe inherited) of class \"{}\" has unsupported for "
                            "serialization type",
                            field.getName(),
                            pData->selfArchetype->getName()));
                        return false;
                    }

                    return true;
                }

                // A field with this name in this section was found.
                // If we continue it will get overwritten.
                pData->error = Error(std::format(
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

        return sSectionName;
    }

    std::variant<std::unique_ptr<Serializable>, Error>
    Serializable::deserialize(const std::filesystem::path& pathToFile) {
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
                std::format("failed to load file \"{}\", error: {}", fixedPath.string(), exception.what()));
        }

        // Deserialize.
        auto result = deserialize(tomlData);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        } else {
            return result;
        }
    }

    std::variant<std::unique_ptr<Serializable>, Error>
    Serializable::deserialize(toml::value& tomlData, std::string sEntityId) {
        if (sEntityId.empty()) {
            // Put something as entity ID so it would not look weird.
            sEntityId = "0";
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
            return Error("provided toml value has zero sections while expected at least one section");
        }

        // Find a section that starts with the specified entity ID.
        // Look for a value between dots.
        // We can't just use sSectionName.starts_with(sEntityId) because we might make a mistake in the
        // following situation: [100.10.1014674670888563010] with entity ID equal to "10".
        std::string sTargetSection;
        size_t iClassId = 0;
        // Each entity section has the following format: [entityId.classId]
        // For sub entities (field with reflected type) format: [parentEntityId.childEntityId.childClassId]
        for (const auto& sSectionName : vSections) {
            const auto iIdEndDotPos = sSectionName.rfind('.');
            if (iIdEndDotPos == std::string::npos) [[unlikely]] {
                return Error("provided toml value does not contain entity ID");
            }
            if (iIdEndDotPos + 1 == sSectionName.size()) [[unlikely]] {
                return Error(std::format("section name \"{}\" does not have a class ID", sSectionName));
            }
            if (iIdEndDotPos == 0) [[unlikely]] {
                return Error(std::format("section \"{}\" is not full", sSectionName));
            }

            // Get ID chain (either entity ID or something like "parentEntityId.childEntityId").
            const auto sIdChain = sSectionName.substr(0, iIdEndDotPos);
            if (sIdChain == sEntityId) {
                sTargetSection = sSectionName;

                // Get this entity's class ID.
                try {
                    iClassId = std::stoull(sSectionName.substr(iIdEndDotPos + 1));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "failed to convert string to unsigned long long when retrieving class ID for section "
                        "\"{}\": {}",
                        sSectionName,
                        ex.what()));
                }
                break;
            }
        }

        // Check if anything was found.
        if (sTargetSection.empty()) {
            return Error(std::format("could not find entity with ID \"{}\"", sEntityId));
        }

        // Get all keys (field names) from this section.
        toml::value section;
        try {
            section = toml::find(tomlData, sTargetSection);
        } catch (std::exception& ex) {
            return Error(std::format("no section \"{}\" was found ({})", sTargetSection, ex.what()));
        }

        if (!section.is_table()) {
            return Error(std::format("found \"{}\" section is not a section", sTargetSection));
        }

        const auto sectionTable = section.as_table();
        std::vector<std::string> vKeys;
        for (const auto& key : sectionTable | std::views::keys) {
            vKeys.push_back(key);
        }

        rfk::Class const* pClass = rfk::getDatabase().getClassById(iClassId);
        if (!pClass) {
            return Error(std::format("no class found in the reflection database by ID {}", iClassId));
        }
        if (!isDerivedFromSerializable(pClass)) {
            return Error(std::format(
                "deserialized class with ID {} does not derive from {}",
                iClassId,
                staticGetArchetype().getName()));
        }

        rfk::UniquePtr<Serializable> pInstance = pClass->makeUniqueInstance<Serializable>();
        if (!pInstance) {
            return Error(
                std::format("unable to make a Serializable object from class \"{}\"", pClass->getName()));
        }

        for (auto& sFieldName : vKeys) {
            if (sFieldName == sSubEntityFieldNameKey) {
                // This field is used as section metadata and tells us what field of parent entity
                // this section describes.
                continue;
            }

            // Read value from TOML.
            toml::value value;
            try {
                value = toml::find(section, sFieldName);
            } catch (std::exception& exception) {
                return Error(std::format(
                    "field \"{}\" was not found in the specified toml value: {}",
                    sFieldName,
                    exception.what()));
            }

            // Get field by name.
            rfk::Field const* pField =
                pClass->getFieldByName(sFieldName.c_str(), rfk::EFieldFlags::Default, true);
            if (!pField) {
                Logger::get().warn(
                    std::format(
                        "field name \"{}\" exists in the specified toml value but does not exist in the "
                        "actual object (if you removed this reflected field from your "
                        "class - ignore this warning)",
                        sFieldName),
                    "");
                continue;
            }
            const auto& fieldType = pField->getType();

            if (!isFieldSerializable(*pField))
                continue;

            // Set field value depending on field type.
            if (fieldType.match(rfk::getType<bool>()) && value.is_boolean()) {
                auto fieldValue = value.as_boolean();
                pField->setUnsafe<bool>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<int>()) && value.is_integer()) {
                auto fieldValue = static_cast<int>(value.as_integer());
                pField->setUnsafe<int>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<long long>()) && value.is_integer()) {
                long long fieldValue = value.as_integer();
                pField->setUnsafe<long long>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<float>()) && value.is_floating()) {
                auto fieldValue = static_cast<float>(value.as_floating());
                pField->setUnsafe<float>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<double>()) && value.is_string()) {
                // Double is stored as a string for better precision.
                try {
                    double fieldValue = std::stod(value.as_string().str);
                    pField->setUnsafe<double>(pInstance.get(), std::move(fieldValue));
                } catch (std::exception& ex) {
                    return Error(std::format(
                        "failed to convert string to double for field \"{}\": {}", sFieldName, ex.what()));
                }
            } else if (fieldType.match(rfk::getType<std::string>()) && value.is_string()) {
                auto fieldValue = value.as_string().str;
                pField->setUnsafe<std::string>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.getArchetype()) {
                // Field with a reflected type.
                // Find a section that has the key ".field_name = *our field name*".
                std::string sSectionNameForField;
                // This will have minimum value of 1 where the dot separates IDs from class ID.
                const auto iNumberOfDotsInTargetSectionName =
                    std::ranges::count(sTargetSection.begin(), sTargetSection.end(), '.');
                for (const auto& sSectionName : vSections) {
                    if (sSectionName == sTargetSection)
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
                        return Error(std::format("section name \"{}\" is corrupted", sSectionName));
                    }
                    // Will be something like: "entityId.subEntityId", "entityId.subEntityId.subSubEntityId"
                    // or etc.
                    auto sEntityIdChain = sSectionName.substr(0, iLastDotPos);

                    // Remove last entity id from chain because we don't know it.
                    iLastDotPos = sEntityIdChain.rfind('.');
                    if (iLastDotPos == std::string::npos) {
                        return Error(std::format("section name \"{}\" is corrupted", sSectionName));
                    }
                    sEntityIdChain = sEntityIdChain.substr(0, iLastDotPos);

                    // Check that this is indeed our sub entity.
                    if (sEntityIdChain != sEntityId) {
                        continue;
                    }

                    // Get this section.
                    toml::value entitySection;
                    try {
                        entitySection = toml::find(tomlData, sSectionName);
                    } catch (std::exception& ex) {
                        return Error(
                            std::format("no section \"{}\" was found ({})", sTargetSection, ex.what()));
                    }

                    if (!section.is_table()) {
                        return Error(std::format("found \"{}\" section is not a section", sTargetSection));
                    }

                    // Look for a key that holds field name.
                    toml::value fieldKey;
                    try {
                        fieldKey = toml::find(entitySection, sSubEntityFieldNameKey);
                    } catch (...) {
                        // Not found, go to next section.
                        continue;
                    }

                    if (!fieldKey.is_string()) {
                        return Error(std::format(
                            "found field name key \"{}\" is not a string", sSubEntityFieldNameKey));
                    }

                    if (fieldKey.as_string() == sFieldName) {
                        sSectionNameForField = sSectionName;
                        break;
                    }
                }

                if (sSectionNameForField.empty()) {
                    return Error(
                        std::format("could not find a section that represents field \"{}\"", sFieldName));
                }

                // Cut field's class ID from the section name.
                // The section name could look something like this: [entityId.subEntityId.subEntityClassId].
                const auto iSubEntityClassIdDotPos = sSectionNameForField.rfind('.');
                if (iSubEntityClassIdDotPos == std::string::npos) [[unlikely]] {
                    return Error(std::format(
                        "sub entity does not have a class ID (section: \"{}\")", sSectionNameForField));
                }
                if (iSubEntityClassIdDotPos + 1 == sSectionNameForField.size()) [[unlikely]] {
                    return Error(
                        std::format("section name \"{}\" does not have a class ID", sSectionNameForField));
                }
                if (iSubEntityClassIdDotPos == 0) [[unlikely]] {
                    return Error(std::format("section \"{}\" is not full", sSectionNameForField));
                }
                const auto sSubEntityId = sSectionNameForField.substr(0, iSubEntityClassIdDotPos);

                // Deserialize section into an object.
                auto result = deserialize(tomlData, sSubEntityId);
                if (std::holds_alternative<Error>(result)) {
                    auto err = std::get<Error>(std::move(result));
                    err.addEntry();
                    return err;
                }
                auto pSubEntity = std::get<std::unique_ptr<Serializable>>(std::move(result));

                // Move object to field.
                cloneSerializableObject(
                    pSubEntity.get(), static_cast<Serializable*>(pField->getPtrUnsafe(pInstance.get())));
            } else {
                return Error(std::format("field \"{}\" has unknown type", sFieldName));
            }
        }

        return pInstance;
    }

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
            if (pClass->getId() == Serializable::staticGetArchetype().getId())
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

        if (fromArchetype.getId() != toArchetype.getId()) {
            return Error(std::format(
                "classes \"{}\" and \"{}\" are not the same",
                fromArchetype.getName(),
                toArchetype.getName()));
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

                if (cloneFieldIfMatchesType<bool>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesType<int>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesType<long long>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesType<float>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesType<double>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;
                if (cloneFieldIfMatchesType<std::string>(pData->pFrom, field, pData->pTo, pFieldTo))
                    return true;

                if (fieldType.getArchetype() && isDerivedFromSerializable(fieldType.getArchetype())) {
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
                        std::format("field \"{}\" has unsupported for serialization type", field.getName()));
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
} // namespace ne
