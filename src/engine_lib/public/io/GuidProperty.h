#pragma once

// STL.
#include <string>
#include <string_view>

// External.
#include "Refureku/Properties/PropertySettings.h"

#include "GuidProperty.generated.h"

namespace ne NENAMESPACE() {
    /**
     * GUID property for classes and structs that inherit Serializable class.
     * This property is required for all derived classes/structs of Serializable class.
     * GUID defined a unique identifier for your class/struct and is used in serialization/deserialization.
     *
     * Usage example:
     * @code
     * class NECLASS(ne::Guid("00000000-0000-0000-0000-000000000000")) MyCoolClass : public ne::Serializable
     * @endcode
     *
     * You can generate a random GUID by just googling "generate GUID". Fun fact: if you search
     * "generate GUID" in DuckDuckGo it will show you a randomly generated GUID on top of all search results.
     *
     * Uniqueness of all GUIDs is checked by the engine on startup in DEBUG builds, so you don't need to check
     * if all of your GUIDs are unique or not, this is done automatically and if something is not unique
     * you will get a message box with an error on engine startup saying where and what is not unique.
     */
    class NECLASS(rfk::PropertySettings(rfk::EEntityKind::Class | rfk::EEntityKind::Struct, false, true)) Guid
        : public rfk::Property {
    public:
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