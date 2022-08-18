﻿#include "io/Serializable.h"

// STL.
#include <ranges>

// Custom.
#include "io/Logger.h"

// External.
#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include "toml11/toml.hpp"
#include "Refureku/Refureku.h"

#include "Serializable.generated_impl.h"

namespace ne {
    void Serializable::serialize(const std::filesystem::path& pathToFile) {
        rfk::Class const& selfArchetype = getArchetype();

        toml::value tomlData;

        struct Data {
            Serializable* self;
            rfk::Class const* selfArchetype;
            toml::value* pTomlData;
        };

        Data loopData{this, &selfArchetype, &tomlData};

        selfArchetype.foreachField(
            [](rfk::Field const& field, void* userData) -> bool {
                const Data* pData = static_cast<Data*>(userData);

                // TODO: don't serialize const values

                const auto sEntityInfo = std::to_string(pData->selfArchetype->getId());

                // Look at field type and save it in TOML data.
                if (field.getType().match(rfk::getType<int>())) {
                    pData->pTomlData->operator[](sEntityInfo).operator[](field.getName()) =
                        field.getUnsafe<int>(pData->self);
                } else if (field.getType().match(rfk::getType<std::string>())) {
                    pData->pTomlData->operator[](sEntityInfo).operator[](field.getName()) =
                        field.getUnsafe<std::string>(pData->self);
                } else {
                    Logger::get().error(
                        std::format("field \"{}\" has unknown type and was not serialized", field.getName()),
                        "");
                }

                return true;
            },
            &loopData,
            true);

        // Add TOML extension to file.
        auto fixedPath = pathToFile;
        if (!fixedPath.string().ends_with(".toml")) {
            fixedPath += ".toml";
        }

        // Save TOML data to file.
        std::ofstream file(fixedPath, std::ios::binary);
        if (!file.is_open()) {
            Logger::get().error(std::format("failed to open file \"{}\"", fixedPath.string()), "");
        }
        file << tomlData;
        file.close();
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

        // Read all sections.
        std::vector<std::string> vSections;
        const auto fileTable = tomlData.as_table();
        for (const auto& [key, value] : fileTable) {
            if (value.is_table()) {
                vSections.push_back(key);
            }
        }

        // Check that we only have 1 section.
        if (vSections.size() != 1) {
            return Error(std::format(
                "file \"{}\", has {} sections while expected 1 section",
                fixedPath.string(),
                vSections.size()));
        }

        // Get all keys (field names) from this section.
        const std::string sSection = vSections[0];
        toml::value section;
        try {
            section = toml::find(tomlData, sSection);
        } catch (std::exception& ex) {
            return Error(std::format("no section \"{}\" was found ({})", sSection, ex.what()));
        }

        if (!section.is_table()) {
            return Error(
                std::format("found \"{}\" is not a section in file \"{}\"", sSection, fixedPath.string()));
        }

        const auto sectionTable = section.as_table();
        std::vector<std::string> vKeys;
        for (const auto& key : sectionTable | std::views::keys) {
            vKeys.push_back(key);
        }

        const size_t iId = std::stoull(sSection);
        rfk::Class const* pClass = rfk::getDatabase().getClassById(iId);
        rfk::UniquePtr<Serializable> pInstance = pClass->makeUniqueInstance<Serializable>();

        for (const auto& sFieldName : vKeys) {
            const toml::value value = toml::find_or(section, sFieldName, toml::value());

            rfk::Field const* pField =
                pClass->getFieldByName(sFieldName.c_str(), rfk::EFieldFlags::Default, true);
            if (!pField) {
                Logger::get().error(std::format("field \"{}\" is not found", sFieldName), "");
                continue;
            }

            if (value.is_integer()) {
                int iValue = static_cast<int>(value.as_integer());
                pField->setUnsafe<int>(pInstance.get(), std::move(iValue));
            } else if (value.is_string()) {
                auto sValue = value.as_string().str;
                pField->setUnsafe<std::string>(pInstance.get(), std::move(sValue));
            } else {
                Logger::get().error(
                    std::format("field \"{}\" has unknown type and was not deserialized", sFieldName), "");
            }
        }

        return pInstance;
    }
} // namespace ne
