#pragma once

// External.
#include "Refureku/Properties/PropertySettings.h"

#include "DontSerializeProperty.generated.h"

/**
 * Add this property to your reflected field to ignore it when serializing an object.
 *
 * Example:
 * @code
 * NEPROPERTY(DontSerialize)
 * int iKey = 42; // will be ignored and not serialized
 * @endcode
 */
class NECLASS(rfk::PropertySettings(rfk::EEntityKind::Field, false, true)) DontSerialize
    : public rfk::Property {
public:
    DontSerialize_GENERATED
};