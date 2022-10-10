#pragma once

// STL.
#include <string>
#include <string_view>

// External.
#include "Refureku/Properties/PropertySettings.h"

#include "GuidProperty.generated.h"

namespace ne RNAMESPACE() {
    /**
     * GUID property for classes and structs that inherit Serializable class.
     * This property is required for all derived classes/structs of Serializable class.
     * GUID defined a unique identifier for your class/struct and is used in serialization/deserialization.
     *
     * Usage example:
     * @code
     * class RCLASS(ne::Guid("00000000-0000-0000-0000-000000000000")) MyCoolClass : public ne::Serializable
     * @endcode
     *
     * You can generate a random GUID by just googling "generate GUID" and using any site/tool to generate it.
     *
     * Uniqueness of all GUIDs is checked by the engine on startup in DEBUG builds, so you don't need to check
     * if all of your GUIDs are unique or not, this is done automatically and if something is not unique
     * you will get a message box with an error on engine startup saying where and what is not unique.
     */
    class RCLASS(rfk::PropertySettings(rfk::EEntityKind::Class | rfk::EEntityKind::Struct, false, false)) Guid
        : public rfk::Property {
    public:
        Guid() = default;
        virtual ~Guid() override = default;

        /**
         * Initializes the GUID of the entity.
         *
         * @param pGuid GUID of the entity.
         */
        Guid(const char* pGuid);

        /**
         * Returns entity's GUID.
         *
         * @return Entity's GUID.
         */
        std::string getGuid() const;

    private:
        /** Entity's GUID. */
        std::string sGuid;

        ne_Guid_GENERATED
    };
} // namespace )

File_GuidProperty_GENERATED
