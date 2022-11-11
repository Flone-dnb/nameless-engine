#pragma once

// External.
#include "Refureku/Properties/PropertySettings.h"

#include "SerializeProperty.generated.h"

namespace ne RNAMESPACE() {
    /**
     * Add this property to your reflected field to make it serializable
     * (i.e. it will be serialized and deserialized) when you serialize/deserialize
     * the object.
     *
     * Example:
     * @code
     * RPROPERTY(ne::Serialize)
     * int iKey = 42; // will be serialized and deserialized
     * @endcode
     */
    class RCLASS(rfk::PropertySettings(rfk::EEntityKind::Field, false, false)) Serialize
        : public rfk::Property {
    public:
        Serialize() = default;
        virtual ~Serialize() override = default;

        ne_Serialize_GENERATED
    };
} // namespace )

File_SerializeProperty_GENERATED
