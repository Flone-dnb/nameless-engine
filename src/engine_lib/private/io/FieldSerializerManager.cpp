#include "io/FieldSerializerManager.h"

// Custom.
#include "io/serializers/PrimitiveFieldSerializer.h"
#include "io/serializers/StringFieldSerializer.h"
#include "io/serializers/VectorFieldSerializer.h"
#include "io/serializers/UnorderedMapFieldSerializer.h"
#include "io/serializers/SerializableObjectFieldSerializer.h"
#include "io/serializers/GlmVecFieldSerializer.h"
#include "io/serializers/MeshDataBinaryFieldSerializer.h"

namespace ne {

    void FieldSerializerManager::registerEngineFieldSerializers() {
        // Add usual serializers.
        addFieldSerializer(std::make_unique<PrimitiveFieldSerializer>());
        addFieldSerializer(std::make_unique<StringFieldSerializer>());
        addFieldSerializer(std::make_unique<VectorFieldSerializer>());
        addFieldSerializer(std::make_unique<UnorderedMapFieldSerializer>());
        addFieldSerializer(std::make_unique<SerializableObjectFieldSerializer>());
        addFieldSerializer(std::make_unique<GlmVecFieldSerializer>());

        // Add binary serializers.
        addBinaryFieldSerializer(std::make_unique<MeshDataBinaryFieldSerializer>());
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

    void FieldSerializerManager::addBinaryFieldSerializer(
        std::unique_ptr<IBinaryFieldSerializer> pBinaryFieldSerializer) {
        std::scoped_lock guard(mtxBinaryFieldSerializers.first);

        // Look if this serializer is already added.
        for (const auto& pSerializer : mtxBinaryFieldSerializers.second) {
            auto& addedSerializer = *pSerializer;
            auto& newSerializer = *pBinaryFieldSerializer;
            if (typeid(addedSerializer) == typeid(newSerializer)) {
                return;
            }
        }

        mtxBinaryFieldSerializers.second.push_back(std::move(pBinaryFieldSerializer));
    }

    std::vector<IFieldSerializer*> FieldSerializerManager::getFieldSerializers() {
        std::scoped_lock guard(mtxFieldSerializers.first);

        std::vector<IFieldSerializer*> vSerializers(mtxFieldSerializers.second.size());
        for (size_t i = 0; i < mtxFieldSerializers.second.size(); i++) {
            vSerializers[i] = mtxFieldSerializers.second[i].get();
        }

        return vSerializers;
    }

    std::vector<IBinaryFieldSerializer*> FieldSerializerManager::getBinaryFieldSerializers() {
        std::scoped_lock guard(mtxBinaryFieldSerializers.first);

        std::vector<IBinaryFieldSerializer*> vSerializers(mtxBinaryFieldSerializers.second.size());
        for (size_t i = 0; i < mtxBinaryFieldSerializers.second.size(); i++) {
            vSerializers[i] = mtxBinaryFieldSerializers.second[i].get();
        }

        return vSerializers;
    }

} // namespace ne
