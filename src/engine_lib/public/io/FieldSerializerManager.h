#pragma once

// Standard.
#include <mutex>

// Custom.
#include "io/serializers/IFieldSerializer.hpp"
#include "io/serializers/IBinaryFieldSerializer.hpp"

namespace ne {
    /** Stores all enabled field serializers. */
    class FieldSerializerManager {
    public:
        FieldSerializerManager() = delete;

        /** Uses @ref addFieldSerializer to add all field serializers implemented in the engine. */
        static void registerEngineFieldSerializers();

        /**
         * Adds a field serializer that will be automatically used in serialization/deserialization
         * to support specific field types. Use @ref getFieldSerializers to get array of added serializers.
         *
         * @remark If the serializer of the specified type was already added previously it will not be
         * added again so it is safe to call this function multiple times with the same serializer.
         *
         * @param pFieldSerializer Field serializer to add.
         */
        static void addFieldSerializer(std::unique_ptr<IFieldSerializer> pFieldSerializer);

        /**
         * Adds a field serializer that will be automatically used in serialization/deserialization
         * to support specific field types. Use @ref getBinaryFieldSerializers to get array of added
         * serializers.
         *
         * @remark If the serializer of the specified type was already added previously it will not be
         * added again so it is safe to call this function multiple times with the same serializer.
         *
         * @param pBinaryFieldSerializer Field serializer to add.
         */
        static void addBinaryFieldSerializer(std::unique_ptr<IBinaryFieldSerializer> pBinaryFieldSerializer);

        /**
         * Returns available field serializers that will be automatically used in
         * serialization/deserialization.
         *
         * @return Array of available field serializers. Do not delete (free) returned pointers.
         */
        static std::vector<IFieldSerializer*> getFieldSerializers();

        /**
         * Returns available binary field serializers that will be automatically used in
         * serialization/deserialization.
         *
         * @return Array of available field serializers. Do not delete (free) returned pointers.
         */
        static std::vector<IBinaryFieldSerializer*> getBinaryFieldSerializers();

    private:
        /** Serializers used to serialize/deserialize fields. */
        static inline std::pair<std::mutex, std::vector<std::unique_ptr<IFieldSerializer>>>
            mtxFieldSerializers;

        /** Serializers used to serialize/deserialize fields into binary files. */
        static inline std::pair<std::mutex, std::vector<std::unique_ptr<IBinaryFieldSerializer>>>
            mtxBinaryFieldSerializers;
    };
} // namespace ne
