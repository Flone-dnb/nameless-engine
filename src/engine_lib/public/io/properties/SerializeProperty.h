﻿#pragma once

// External.
#include "Refureku/Properties/PropertySettings.h"

#include "SerializeProperty.generated.h"

namespace ne RNAMESPACE() {
    /**
     * Defines how a field should be serialized.
     *
     * @remark Intentionally not using `enum class` to make `RPROPERTY` settings to look short and compact.
     */
    enum FieldSerializationType {
        FST_WITH_OWNER,       ///< Field is serialized in the same file as the owner object.
        FST_AS_EXTERNAL_FILE, ///< Field is serialized in a separate file located next to the file of owner
                              ///< object. Only fields of types that derive from `Serializable` can be marked
                              ///< with this type. External file will have the following naming:
                              ///< "owner_file_name_without_extension.id.field_name.toml", where "id" is the
                              ///< section name (without GUID) of the owner object.
                              ///< For example, the resulting external file name might look like this:
                              ///< "savedata.0.1.test.toml".
        FST_AS_EXTERNAL_BINARY_FILE, ///< Same as "AS_EXTERNAL_FILE" but when serializing the engine will look
                                     ///< for a binary serializer that supports the type of the field, so
                                     ///< the field will be serialized into a separate binary file (smaller
                                     ///< size, faster deserialization but sacrificing readability of the
                                     ///< file).
    };

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
        Serialize() : Serialize(FieldSerializationType::FST_WITH_OWNER) {}

        virtual ~Serialize() override = default;

        /**
         * Initializes the property.
         *
         * @param serializationType Defined how this field should be serialized.
         */
        Serialize(FieldSerializationType serializationType);

        /**
         * Returns how this field should be serialized.
         *
         * @return Serialization type.
         */
        FieldSerializationType getSerializationType() const;

    private:
        /** Defines how to serialize this property. */
        FieldSerializationType serializationType;

        ne_Serialize_GENERATED
    };
} // namespace ne RNAMESPACE()

File_SerializeProperty_GENERATED
