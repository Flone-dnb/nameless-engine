#include "io/FieldSerializerManager.h"

// Custom.
#include "io/serializers/PrimitiveFieldSerializer.h"
#include "io/serializers/StringFieldSerializer.h"
#include "io/serializers/VectorFieldSerializer.h"
#include "io/serializers/UnorderedMapFieldSerializer.h"
#include "io/serializers/SerializableObjectFieldSerializer.h"
#include "io/serializers/GlmVecFieldSerializer.h"
#include "io/serializers/MaterialFieldSerializer.h"

namespace ne {

    void FieldSerializerManager::registerEngineFieldSerializers() {
        addFieldSerializer(std::make_unique<PrimitiveFieldSerializer>());
        addFieldSerializer(std::make_unique<StringFieldSerializer>());
        addFieldSerializer(std::make_unique<VectorFieldSerializer>());
        addFieldSerializer(std::make_unique<UnorderedMapFieldSerializer>());
        addFieldSerializer(std::make_unique<SerializableObjectFieldSerializer>());
        addFieldSerializer(std::make_unique<GlmVecFieldSerializer>());
        addFieldSerializer(std::make_unique<MaterialFieldSerializer>());
    }

    void FieldSerializerManager::addFieldSerializer(std::unique_ptr<IFieldSerializer> pFieldSerializer) {
        std::scoped_lock guard(mtxFieldSerializers.first);

        // Look if this serializer is already added.
        for (const auto& pSerializer : mtxFieldSerializers.second) {
            auto& addedSerializer = *pSerializer;
            auto& newSerializer = *pFieldSerializer;
            if (typeid(addedSerializer) == typeid(newSerializer)) {
                return;
            }
        }

        mtxFieldSerializers.second.push_back(std::move(pFieldSerializer));
    }

    std::vector<IFieldSerializer*> FieldSerializerManager::getFieldSerializers() {
        std::scoped_lock guard(mtxFieldSerializers.first);

        std::vector<IFieldSerializer*> vSerializers(mtxFieldSerializers.second.size());
        for (size_t i = 0; i < mtxFieldSerializers.second.size(); i++) {
            vSerializers[i] = mtxFieldSerializers.second[i].get();
        }

        return vSerializers;
    }

} // namespace ne
