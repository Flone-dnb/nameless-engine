#pragma once

// Standard.
#include <string>
#include <memory>

namespace ne {
    /**
     * Used to generate unique values and names.
     */
    class UniqueValueGenerator {
    public:
        UniqueValueGenerator(const UniqueValueGenerator&) = delete;
        UniqueValueGenerator& operator=(const UniqueValueGenerator&) = delete;
        virtual ~UniqueValueGenerator();

        /**
         * Returns a reference to the generator instance.
         * If no instance was created yet, this function will create it
         * and return a reference to it.
         *
         * @return Reference to the generator instance.
         */
        static UniqueValueGenerator& get();

        /**
         * Returns a unique window name.
         *
         * @return Unique window name.
         */
        [[nodiscard]] std::string getUniqueWindowName();

    private:
        UniqueValueGenerator() = default;

        /**
         * Used to make sure that window class names are unique.
         */
        unsigned long long iWindowCounter = 0;
    };
} // namespace ne
