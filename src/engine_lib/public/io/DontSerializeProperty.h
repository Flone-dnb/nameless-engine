#pragma once

// External.
#include "Refureku/Properties/PropertySettings.h"

#include "DontSerializeProperty.generated.h"

namespace ne RNAMESPACE() {
    /**
     * Add this property to your reflected field to ignore it when serializing an object.
     *
     * Example:
     * @code
     * // #include here
     * // ...
     * RPROPERTY(ne::DontSerialize)
     * int iKey = 42; // will be ignored and not serialized
     * @endcode
     */
    class RCLASS(rfk::PropertySettings(rfk::EEntityKind::Field, false, false)) DontSerialize
        : public rfk::Property {
    public:
        DontSerialize() = default;
        virtual ~DontSerialize() override = default;

        ne_DontSerialize_GENERATED
    };
} // namespace )

File_DontSerializeProperty_GENERATED
