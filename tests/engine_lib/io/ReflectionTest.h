#pragma once

// Custom.
#include "io/Serializable.h"
#include "io/DontSerializeProperty.h"

#include "ReflectionTest.generated.h"

using namespace ne;

class RCLASS(Guid("550ea9f9-dd8a-4089-a717-0fe4e351a688")) ReflectionTestClass : public Serializable {
public:
    ReflectionTestClass() = default;
    virtual ~ReflectionTestClass() override = default;

    RPROPERTY()
    bool bBoolValue = false;

    RPROPERTY()
    int iIntValue = 0;

    RPROPERTY()
    long long iLongLongValue = 0;

    RPROPERTY()
    float floatValue = 0.0f;

    RPROPERTY()
    double doubleValue = 0.0;

    RPROPERTY()
    std::string sStringValue;

    RPROPERTY()
    std::vector<bool> vBoolVector;

    RPROPERTY()
    std::vector<int> vIntVector;

    RPROPERTY()
    std::vector<long long> vLongLongVector;

    RPROPERTY()
    std::vector<float> vFloatVector;

    RPROPERTY()
    std::vector<double> vDoubleVector;

    RPROPERTY()
    std::vector<std::string> vStringVector;

    ReflectionTestClass_GENERATED
};

class RCLASS(Guid("550ea9f9-dd8a-4089-a717-0fe4e351a689")) ReflectionOuterTestClass : public Serializable {
public:
    ReflectionOuterTestClass() = default;
    virtual ~ReflectionOuterTestClass() override = default;

    RPROPERTY(DontSerialize)
    int iIntNotSerialized = 0;

    RPROPERTY()
    bool bBoolValue = false;

    RPROPERTY()
    ReflectionTestClass entity;

    ReflectionOuterTestClass_GENERATED
};

File_ReflectionTest_GENERATED
