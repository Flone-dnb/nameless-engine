#pragma once

// STL.
#include <string>
#include <vector>
#include <unordered_map>

// Custom.
#include "io/Serializable.h"
#include "game/nodes/Node.h"
#include "io/SerializeProperty.h"

#include "ReflectionTest.generated.h"

using namespace ne;

class RCLASS(Guid("550ea9f9-dd8a-4089-a717-0fe4e351a686")) ReflectionTestEntity : public Serializable {
public:
    ReflectionTestEntity() = default;
    virtual ~ReflectionTestEntity() override = default;

    RPROPERTY(Serialize)
    int iIntValue1 = 0;

    RPROPERTY(Serialize)
    int iIntValue2 = 0;

    RPROPERTY(Serialize)
    std::vector<std::string> vVectorValue1;

    RPROPERTY(Serialize)
    std::vector<std::string> vVectorValue2;

    ReflectionTestEntity_GENERATED
};

class RCLASS(Guid("550ea9f9-dd8a-4089-a717-0fe4e351a687")) ReflectionTestNode1 : public Node {
public:
    ReflectionTestNode1() = default;
    virtual ~ReflectionTestNode1() override = default;

    RPROPERTY(Serialize)
    bool bBoolValue1 = false;

    RPROPERTY(Serialize)
    bool bBoolValue2 = false;

    RPROPERTY(Serialize)
    ReflectionTestEntity entity;

    ReflectionTestNode1_GENERATED
};

struct RSTRUCT(Guid("550ea9f9-dd8a-4089-a717-0fe4e351a688")) ReflectionTestStruct : public Serializable {
public:
    ReflectionTestStruct() = default;
    virtual ~ReflectionTestStruct() override = default;

    RPROPERTY(Serialize)
    bool bBoolValue = false;

    RPROPERTY(Serialize)
    int iIntValue = 0;

    RPROPERTY(Serialize)
    unsigned int iUnsignedIntValue = 0;

    RPROPERTY(Serialize)
    long long iLongLongValue = 0;

    RPROPERTY(Serialize)
    unsigned long long iUnsignedLongLongValue = 0;

    RPROPERTY(Serialize)
    float floatValue = 0.0f;

    RPROPERTY(Serialize)
    double doubleValue = 0.0;

    RPROPERTY(Serialize)
    std::string sStringValue;

    // vectors

    RPROPERTY(Serialize)
    std::vector<bool> vBoolVector;

    RPROPERTY(Serialize)
    std::vector<int> vIntVector;

    RPROPERTY(Serialize)
    std::vector<unsigned int> vUnsignedIntVector;

    RPROPERTY(Serialize)
    std::vector<long long> vLongLongVector;

    RPROPERTY(Serialize)
    std::vector<unsigned long long> vUnsignedLongLongVector;

    RPROPERTY(Serialize)
    std::vector<float> vFloatVector;

    RPROPERTY(Serialize)
    std::vector<double> vDoubleVector;

    RPROPERTY(Serialize)
    std::vector<std::string> vStringVector;

    RPROPERTY(Serialize)
    std::vector<int> vEmpty;

    // maps

    RPROPERTY(Serialize)
    std::unordered_map<bool, bool> mapBoolBool;

    RPROPERTY(Serialize)
    std::unordered_map<bool, int> mapBoolInt;

    RPROPERTY(Serialize)
    std::unordered_map<bool, unsigned int> mapBoolUnsignedInt;

    RPROPERTY(Serialize)
    std::unordered_map<bool, long long> mapBoolLongLong;

    RPROPERTY(Serialize)
    std::unordered_map<bool, unsigned long long> mapBoolUnsignedLongLong;

    RPROPERTY(Serialize)
    std::unordered_map<bool, float> mapBoolFloat;

    RPROPERTY(Serialize)
    std::unordered_map<bool, double> mapBoolDouble;

    RPROPERTY(Serialize)
    std::unordered_map<bool, std::string> mapBoolString;

    RPROPERTY(Serialize)
    std::unordered_map<int, bool> mapIntBool;

    RPROPERTY(Serialize)
    std::unordered_map<unsigned int, bool> mapUnsignedIntBool;

    RPROPERTY(Serialize)
    std::unordered_map<long long, bool> mapLongLongBool;

    RPROPERTY(Serialize)
    std::unordered_map<unsigned long long, bool> mapUnsignedLongLongBool;

    RPROPERTY(Serialize)
    std::unordered_map<float, bool> mapFloatBool;

    RPROPERTY(Serialize)
    std::unordered_map<double, bool> mapDoubleBool;

    RPROPERTY(Serialize)
    std::unordered_map<std::string, bool> mapStringBool;

    RPROPERTY(Serialize)
    std::unordered_map<bool, bool> mapEmpty;

    ReflectionTestStruct_GENERATED
};

class RCLASS(Guid("550ea9f9-dd8a-4089-a717-0fe4e351a689")) ReflectionOuterTestClass : public Serializable {
public:
    ReflectionOuterTestClass() = default;
    virtual ~ReflectionOuterTestClass() override = default;

    RPROPERTY()
    int iIntNotSerialized = 0;

    RPROPERTY(Serialize)
    bool bBoolValue = false;

    RPROPERTY(Serialize)
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
    RPROPERTY(Serialize)
    std::unordered_map<unsigned long long, unsigned long long> items;

    InventorySaveData_GENERATED
};

class RCLASS(Guid("36063853-79b1-41e6-afa6-6923c8b24815")) PlayerSaveData : public ne::Serializable {
public:
    PlayerSaveData() = default;
    virtual ~PlayerSaveData() override = default;

    RPROPERTY(Serialize)
    std::string sCharacterName;

    RPROPERTY(Serialize)
    unsigned long long iCharacterLevel = 0;

    RPROPERTY(Serialize)
    unsigned long long iExperiencePoints = 0;

    RPROPERTY(Serialize)
    InventorySaveData inventory;

    /// Stores IDs of player abilities.
    RPROPERTY(Serialize)
    std::vector<unsigned long long> vAbilities;

    PlayerSaveData_GENERATED
};

File_ReflectionTest_GENERATED
