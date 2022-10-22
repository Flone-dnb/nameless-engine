#pragma once

// STL.
#include <string>
#include <vector>
#include <unordered_map>

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
    unsigned int iUnsignedIntValue = 0;

    RPROPERTY()
    long long iLongLongValue = 0;

    RPROPERTY()
    unsigned long long iUnsignedLongLongValue = 0;

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
    std::vector<unsigned int> vUnsignedIntVector;

    RPROPERTY()
    std::vector<long long> vLongLongVector;

    RPROPERTY()
    std::vector<unsigned long long> vUnsignedLongLongVector;

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
    std::unordered_map<bool, unsigned int> mapBoolUnsignedInt;

    RPROPERTY()
    std::unordered_map<bool, long long> mapBoolLongLong;

    RPROPERTY()
    std::unordered_map<bool, unsigned long long> mapBoolUnsignedLongLong;

    RPROPERTY()
    std::unordered_map<bool, float> mapBoolFloat;

    RPROPERTY()
    std::unordered_map<bool, double> mapBoolDouble;

    RPROPERTY()
    std::unordered_map<bool, std::string> mapBoolString;

    RPROPERTY()
    std::unordered_map<int, bool> mapIntBool;

    RPROPERTY()
    std::unordered_map<unsigned int, bool> mapUnsignedIntBool;

    RPROPERTY()
    std::unordered_map<long long, bool> mapLongLongBool;

    RPROPERTY()
    std::unordered_map<unsigned long long, bool> mapUnsignedLongLongBool;

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

class RCLASS(Guid("a34a8047-d7b4-4c70-bb9a-429875a8cd26")) InventorySaveData : public ne::Serializable {
public:
    InventorySaveData() = default;
    virtual ~InventorySaveData() override = default;

    /// Adds a specific item instance to the inventory.
    void addOneItem(unsigned long long iItemId) {
        const auto it = items.find(iItemId);

        if (it == items.end()) {
            items[iItemId] = 1;
            return;
        }

        it->second += 1;
    }

    /// Removes a specific item instance from the inventory.
    void removeOneItem(unsigned long long iItemId) {
        const auto it = items.find(iItemId);
        if (it == items.end())
            return;

        if (it->second <= 1) {
            items.erase(it);
            return;
        }

        it->second -= 1;
    }

    /// Returns amount of specific items in the inventory.
    unsigned long long getItemAmount(unsigned long long iItemId) {
        const auto it = items.find(iItemId);
        if (it == items.end())
            return 0;

        return it->second;
    }

private:
    /// Contains item ID as a key and item amount (in the inventory) as a value.
    RPROPERTY()
    std::unordered_map<unsigned long long, unsigned long long> items;

    InventorySaveData_GENERATED
};

class RCLASS(Guid("36063853-79b1-41e6-afa6-6923c8b24815")) PlayerSaveData : public ne::Serializable {
public:
    PlayerSaveData() = default;
    virtual ~PlayerSaveData() override = default;

    RPROPERTY()
    std::string sCharacterName;

    RPROPERTY()
    unsigned long long iCharacterLevel = 0;

    RPROPERTY()
    unsigned long long iExperiencePoints = 0;

    RPROPERTY()
    InventorySaveData inventory;

    /// Stores IDs of player abilities.
    RPROPERTY()
    std::vector<unsigned long long> vAbilities;

    PlayerSaveData_GENERATED
};

File_ReflectionTest_GENERATED
