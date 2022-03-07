#include "UniqueValueGenerator.h"

namespace dxe {
    UniqueValueGenerator::~UniqueValueGenerator() {}

    UniqueValueGenerator &UniqueValueGenerator::get() {
        static UniqueValueGenerator generator;
        return generator;
    }

    std::string UniqueValueGenerator::getUniqueWindowClassName() {
        iWindowCounter++;
        return std::string("Window~") + std::to_string(iWindowCounter);
    }
} // namespace dxe
