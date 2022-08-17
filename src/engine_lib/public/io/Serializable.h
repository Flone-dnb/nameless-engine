#pragma once

// STL.
#include <filesystem>
#include <memory>
#include <variant>

// Custom.
#include "misc/Error.h"

// External.
#include "Refureku/Object.h"

#include "Serializable.generated.h"

namespace ne NENAMESPACE() {
    /**
     * Base class for making a serializable type.
     *
     * Inherit your class from this type to add a 'serialize' function which will
     * serialize the type and all reflected fields (even inherited) into a file.
     */
    class NECLASS() Serializable : public rfk::Object {
    public:
        Serializable() = default;
        virtual ~Serializable() override = default;

        /**
         * Serializes the type and all reflected fields (including inherited) into a file.
         *
         * @param pathToFile File to write reflected fields to.
         */
        NEFUNCTION()
        void serialize(const std::filesystem::path& pathToFile);

        NEFUNCTION()
        static std::variant<std::unique_ptr<Serializable>, Error>
        deserialize(const std::filesystem::path& pathToFile);

    private:
        /** test property */
        NEPROPERTY()
        int iMyCoolField = 42;

        ne_Serializable_GENERATED
    };
}; // namespace )

File_Serializable_GENERATED