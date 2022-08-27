#include "io/Serializable.h"

#include "Reflection_impl.hpp"

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
