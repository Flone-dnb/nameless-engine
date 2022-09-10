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
    class NECLASS(rfk::PropertySettings(rfk::EEntityKind::Field, false, false)) DontSerialize
        : public rfk::Property {
    public:
        DontSerialize() = default;
        virtual ~DontSerialize() override = default;

        ne_DontSerialize_GENERATED
    };
} // namespace )

File_DontSerializeProperty_GENERATED
