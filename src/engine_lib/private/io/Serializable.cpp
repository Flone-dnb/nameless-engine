#include "io/Serializable.h"

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
                const auto& fieldType = field.getType();

                // Don't serialize specific types.
                if (fieldType.isConst() || fieldType.isPointer() || fieldType.isLValueReference() ||
                    fieldType.isRValueReference() || fieldType.isCArray())
                    return true;

                const Data* pData = static_cast<Data*>(userData);
                const auto sEntityId = std::to_string(pData->selfArchetype->getId());

                // Look at field type and save it in TOML data.
                if (fieldType.match(rfk::getType<bool>())) {
                    pData->pTomlData->operator[](sEntityId).operator[](field.getName()) =
                        field.getUnsafe<bool>(pData->self);
                } else if (fieldType.match(rfk::getType<int>())) {
                    pData->pTomlData->operator[](sEntityId).operator[](field.getName()) =
                        field.getUnsafe<int>(pData->self);
                } else if (fieldType.match(rfk::getType<long long>())) {
                    pData->pTomlData->operator[](sEntityId).operator[](field.getName()) =
                        field.getUnsafe<long long>(pData->self);
                } else if (fieldType.match(rfk::getType<float>())) {
                    pData->pTomlData->operator[](sEntityId).operator[](field.getName()) =
                        field.getUnsafe<float>(pData->self);
                } else if (fieldType.match(rfk::getType<double>())) {
                    // Store double as string for better precision.
                    pData->pTomlData->operator[](sEntityId).operator[](field.getName()) =
                        toml::format(toml::value(field.getUnsafe<double>(pData->self)));
                } else if (fieldType.match(rfk::getType<std::string>())) {
                    pData->pTomlData->operator[](sEntityId).operator[](field.getName()) =
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
            toml::value value;
            try {
                value = toml::find(section, sFieldName);
            } catch (std::exception& exception) {
                Logger::get().error(
                    std::format(
                        "field \"{}\" was not found in the file \"{}\": {}",
                        sFieldName,
                        fixedPath.string(),
                        exception.what()),
                    "");
                continue;
            }

            rfk::Field const* pField =
                pClass->getFieldByName(sFieldName.c_str(), rfk::EFieldFlags::Default, true);
            if (!pField) {
                Logger::get().error(std::format("field \"{}\" is not found", sFieldName), "");
                continue;
            }
            const auto& fieldType = pField->getType();

            // Set field value depending on field type.
            if (fieldType.match(rfk::getType<bool>())) {
                auto fieldValue = value.as_boolean();
                pField->setUnsafe<bool>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<int>())) {
                auto fieldValue = static_cast<int>(value.as_integer());
                pField->setUnsafe<int>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<long long>())) {
                long long fieldValue = value.as_integer();
                pField->setUnsafe<long long>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<float>())) {
                auto fieldValue = static_cast<float>(value.as_floating());
                pField->setUnsafe<float>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<double>())) {
                // Double is stored as a string for better precision.
                double fieldValue = std::stod(value.as_string().str);
                pField->setUnsafe<double>(pInstance.get(), std::move(fieldValue));
            } else if (fieldType.match(rfk::getType<std::string>())) {
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
