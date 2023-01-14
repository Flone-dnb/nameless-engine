#pragma once

// External.
#include "Refureku/Properties/PropertySettings.h"

#include "SerializeAsExternalProperty.generated.h"

namespace ne RNAMESPACE() {
    /**
     * Add this property to your reflected field to make it serializable
     * (i.e. it will be serialized and deserialized) when you serialize/deserialize
     * the object.
     *
     * Additionally, this property makes the marked field to be serialized as external file.
     * This means that when you use `serialize` functions you will get two files: one is the
     * main file that contains all object data except the field marked with this property
     * and another file that will only contain data of this field. The main file will contain
     * a reference to the external file so that in deserialization everything can be fully deserialized
     * (including the field marked with this property).
     *
     * External file will be located next to the main file and will have the following naming:
     * "main_file_name_without_extension.id.field_name.toml", where "id" is the section
     * name without GUID of the main object (that is being serialized). For example,
     * the resulting external file name might look like this: "savedata.0.1.test.toml".
     *
     * @remark Only fields of type that derive from `Serializable` can be marked with this property.
     *
     * Example:
     * @code
     * RPROPERTY(ne::SerializeAsExternal)
     * MySerializable test;
     * @endcode
     */
    class RCLASS(rfk::PropertySettings(rfk::EEntityKind::Field, false, false)) SerializeAsExternal
        : public rfk::Property {
    public:
        SerializeAsExternal() = default;
        virtual ~SerializeAsExternal() override = default;

        ne_SerializeAsExternal_GENERATED
    };
} // namespace ne RNAMESPACE()

File_SerializeAsExternalProperty_GENERATED
