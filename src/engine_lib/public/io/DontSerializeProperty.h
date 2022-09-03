#pragma once

// External.
#include "Refureku/Properties/PropertySettings.h"

#include "DontSerializeProperty.generated.h"

namespace ne NENAMESPACE() {
    /**
     * Add this property to your reflected field to ignore it when serializing an object.
     *
     * Example:
     * @code
     * // #include here
     * // ...
     * NEPROPERTY(ne::DontSerialize)
     * int iKey = 42; // will be ignored and not serialized
     * @endcode
     */
    class NECLASS(rfk::PropertySettings(rfk::EEntityKind::Field, false, true)) DontSerialize
        : public rfk::Property {
    public:
        ne_DontSerialize_GENERATED
    };
} // namespace ne NENAMESPACE()

File_DontSerializeProperty_GENERATED