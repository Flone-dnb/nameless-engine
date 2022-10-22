#pragma once

// Custom.
#include "io/Serializable.h"
#include "io/DontSerializeProperty.h"

#include "ReflectionTest.generated.h"

using namespace ne;

struct RSTRUCT(Guid("550ea9f9-dd8a-4089-a717-0fe4e351a688")) ReflectionTestStruct : public Serializable {
public:
    ReflectionTestStruct() = default;
    virtual ~ReflectionTestStruct() override = default;

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

    // vectors

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

    RPROPERTY()
    std::vector<int> vEmpty;

    // maps

    RPROPERTY()
    std::unordered_map<bool, bool> mapBoolBool;

    RPROPERTY()
    std::unordered_map<bool, int> mapBoolInt;

    RPROPERTY()
    std::unordered_map<bool, long long> mapBoolLongLong;

    RPROPERTY()
    std::unordered_map<bool, float> mapBoolFloat;

    RPROPERTY()
    std::unordered_map<bool, double> mapBoolDouble;

    RPROPERTY()
    std::unordered_map<bool, std::string> mapBoolString;

    RPROPERTY()
    std::unordered_map<int, bool> mapIntBool;

    RPROPERTY()
    std::unordered_map<long long, bool> mapLongLongBool;

    RPROPERTY()
    std::unordered_map<float, bool> mapFloatBool;

    RPROPERTY()
    std::unordered_map<double, bool> mapDoubleBool;

    RPROPERTY()
    std::unordered_map<std::string, bool> mapStringBool;

    RPROPERTY()
    std::unordered_map<bool, bool> mapEmpty;

    ReflectionTestStruct_GENERATED
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
    ReflectionTestStruct entity;

    ReflectionOuterTestClass_GENERATED
};

File_ReflectionTest_GENERATED
