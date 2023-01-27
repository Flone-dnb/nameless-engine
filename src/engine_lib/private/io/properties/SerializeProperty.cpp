#include "io/properties/SerializeProperty.h"

#include "SerializeProperty.generated_impl.h"

namespace ne {
    Serialize::Serialize(FieldSerializationType serializationType) {
        this->serializationType = serializationType;
    }

    FieldSerializationType Serialize::getSerializationType() const { return serializationType; }

} // namespace ne
