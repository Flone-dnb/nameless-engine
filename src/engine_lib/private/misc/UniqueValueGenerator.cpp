﻿#include "UniqueValueGenerator.h"

namespace ne {
    UniqueValueGenerator::~UniqueValueGenerator() {}

    UniqueValueGenerator& UniqueValueGenerator::get() {
        static UniqueValueGenerator generator;
        return generator;
    }

    std::string UniqueValueGenerator::getUniqueWindowName() {
        iWindowCounter++;
        return std::string("Window~") + std::to_string(iWindowCounter);
    }
} // namespace ne
