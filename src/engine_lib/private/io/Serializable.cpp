﻿#include "io/Serializable.h"

// STL.
#include <ranges>

// Custom.
#include "io/Logger.h"

// External.
#include "Refureku/Refureku.h"

#include "Serializable.generated_impl.h"

namespace ne {
    std::optional<Error> Serializable::serialize(const std::filesystem::path& pathToFile) {
        toml::value tomlData;
        auto optionalError = serialize(tomlData);
        if (optionalError.has_value()) {
            auto err = std::move(optionalError.value());
            err.addEntry();
            return err;
        }

        // Add TOML extension to file.
        auto fixedPath = pathToFile;
        if (!fixedPath.string().ends_with(".toml")) {
            fixedPath += ".toml";
        }

        // Save TOML data to file.
        std::ofstream file(fixedPath, std::ios::binary);
        if (!file.is_open()) {
            return Error(std::format("failed to open file \"{}\"", fixedPath.string()));
        }
        file << tomlData;
        file.close();

        return {};
    }

    std::optional<Error> Serializable::serialize(toml::value& tomlData, size_t iEntityUniqueId) {
        rfk::Class const& selfArchetype = getArchetype();

        struct Data {
            Serializable* self;
            rfk::Class const* selfArchetype;
            toml::value* pTomlData;
            size_t iEntityUniqueId;
            std::optional<Error> error;
        };

        Data loopData{this, &selfArchetype, &tomlData, iEntityUniqueId, {}};

        selfArchetype.foreachField(
            [](rfk::Field const& field, void* userData) -> bool {
                const auto& fieldType = field.getType();

                // Don't serialize specific types.
                if (fieldType.isConst() || fieldType.isPointer() || fieldType.isLValueReference() ||
                    fieldType.isRValueReference() || fieldType.isCArray())
                    return true;

                // Ignore this field if marked as DontSerialize.
                if (field.getProperty<DontSerialize>())
                    return true;

                Data* pData = static_cast<Data*>(userData);
                const auto sEntityId = std::format(
                    "{}.{}", pData->iEntityUniqueId, std::to_string(pData->selfArchetype->getId()));
                const auto sFieldName = field.getName();

                try {
                    // Throws if not found.
                    toml::find(*pData->pTomlData, sEntityId, sFieldName);
                } catch (...) {
                    // No field exists with this name in this section - OK.
                    // Look at field type and save it in TOML data.
                    if (fieldType.match(rfk::getType<bool>())) {
                        pData->pTomlData->operator[](sEntityId).operator[](sFieldName) =
                            field.getUnsafe<bool>(pData->self);
                    } else if (fieldType.match(rfk::getType<int>())) {
                        pData->pTomlData->operator[](sEntityId).operator[](sFieldName) =
                            field.getUnsafe<int>(pData->self);
                    } else if (fieldType.match(rfk::getType<long long>())) {
                        pData->pTomlData->operator[](sEntityId).operator[](sFieldName) =
                            field.getUnsafe<long long>(pData->self);
                    } else if (fieldType.match(rfk::getType<float>())) {
                        pData->pTomlData->operator[](sEntityId).operator[](sFieldName) =
                            field.getUnsafe<float>(pData->self);
                    } else if (fieldType.match(rfk::getType<double>())) {
                        // Store double as string for better precision.
                        pData->pTomlData->operator[](sEntityId).operator[](sFieldName) =
                            toml::format(toml::value(field.getUnsafe<double>(pData->self)));
                    } else if (fieldType.match(rfk::getType<std::string>())) {
                        pData->pTomlData->operator[](sEntityId).operator[](sFieldName) =
                            field.getUnsafe<std::string>(pData->self);
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
                // This should never happen because Refureku fails the compilation when we have 2 reflected
                // fields with the same name. Adding this if something in Refureku will change and it will
                // compile without any issues.
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

        return {};
    }

    std::variant<std::unique_ptr<Serializable>, Error>
    Serializable::deserialize(const std::filesystem::path& pathToFile) {
        // Add TOML extension to file.
        auto fixedPath = pathToFile;
        if (!fixedPath.string().ends_with(".toml")) {
            fixedPath += ".toml";
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
    Serializable::deserialize(toml::value& tomlData, size_t iEntityUniqueId) {
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
        std::string sTargetSection;
        size_t iClassId = 0;
        std::string sEntityId = std::to_string(iEntityUniqueId);
        // Each entity section has the following format: [entityId.classId]
        for (const auto& sSectionName : vSections) {
            const auto iDotPos = sSectionName.find('.');
            if (iDotPos == std::string::npos) [[unlikely]] {
                return Error("provided toml value does not contain entity ID");
            }
            if (iDotPos + 1 == sSectionName.size()) [[unlikely]] {
                return Error("provided toml value does not contain class ID");
            }

            const auto iSectionEntityId = std::stoull(sSectionName.substr(0, iDotPos));
            if (iSectionEntityId == iEntityUniqueId) {
                sTargetSection = sSectionName;

                // Get class ID.
                iClassId = std::stoull(sSectionName.substr(iDotPos + 1));
                break;
            }
        }

        // Check if anything was found.
        if (sTargetSection.empty()) {
            return Error(std::format("could not find entity with ID \"{}\"", iEntityUniqueId));
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
        rfk::UniquePtr<Serializable> pInstance = pClass->makeUniqueInstance<Serializable>();
        if (!pInstance) {
            return Error(
                std::format("unable to make a Serializable object from class \"{}\"", pClass->getName()));
        }

        for (const auto& sFieldName : vKeys) {
            // Read value from TOML.
            toml::value value;
            try {
                value = toml::find(section, sFieldName);
            } catch (std::exception& exception) {
                Logger::get().error(
                    std::format(
                        "field \"{}\" was not found in the specified toml value: {}",
                        sFieldName,
                        exception.what()),
                    "");
                continue;
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
                double fieldValue = std::stod(value.as_string().str);
                pField->setUnsafe<double>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<std::string>()) && value.is_string()) {
                auto fieldValue = value.as_string().str;
                pField->setUnsafe<std::string>(pInstance.get(), std::move(fieldValue));
            } else {
                Logger::get().error(
                    std::format("field \"{}\" has unknown type and was not deserialized", sFieldName), "");
            }
        }

        return pInstance;
    }
} // namespace ne
