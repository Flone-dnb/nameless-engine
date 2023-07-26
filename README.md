# nameless-engine

TODO: screenshot game/editor

### Supported platforms

* Windows
* Linux

# Features

Click on any line below to show an example:

<details>
<summary>Unreal Engine-like reflection via code generation</summary>
  
```C++
// MyClass.h

// other includes

#include "MyClass.generated.h" // should be included last

class RCLASS(Guid("550ea9f9-dd8a-4089-a717-0fe4e351a681")) MyClass : public Serializable {
public:
    MyClass() = default;
    virtual ~MyClass() override = default;

    RFUNCTION()
    void Foo();

private:
    RPROPERTY(Serialize)
    int iAnswer = 42;

    RPROPERTY()
    bool bValue = false;

    MyClass_GENERATED
};

File_MyClass_GENERATED
```
</details>

<details>
<summary>Garbage collector for resolving cyclic references</summary>
  
```C++
class Foo;
class Bar;

class Foo {
public:
    gc<Bar> pBar; // garbage collected smart pointer similar to `std::shared_ptr`
};

class Bar {
public:
    gc<Foo> pFoo;
};

{
    auto pFoo = gc_new<Foo>();
    auto pBar = gc_new<Bar>();

    pFoo->pBar = pBar;
    pBar->pFoo = pFoo;
}

// `Foo` and `Bar` still exist and were not freed, waiting for GC...
gc_collector()->collect(); // this will be run regularly somewhere in the engine code so you don't have to care about it
// `Foo` and `Bar` were freed.
```
</details>

<details>
<summary>Easy to use serialization and deserialization for custom user types</summary>
  
```C++
// PlayerSaveData.h
// ---------------------------------------------------------------------------------
// Example below shows a sample code for saving/loading player's data (such as name, level, inventory).
// Class fields will be serialized to file and deserialized from file.
// ---------------------------------------------------------------------------------

// PlayerSaveData.cpp

#include "PlayerSaveData.generated_impl.h"

// PlayerSaveData.h

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "io/Serializable.h"

#include "PlayerSaveData.generated.h"

using namespace ne;

class RCLASS(Guid("a34a8047-d7b4-4c70-bb9a-429875a8cd26")) InventorySaveData : public Serializable {
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

/// Some in-game character ability.
class RCLASS(Guid("36063853-79b1-41e6-afa6-6923c8b24811")) Ability : public Serializable {
public:
    Ability() = default;
    virtual ~Ability() override = default;

    Ability(const std::string& sAbilityName) { this->sAbilityName = sAbilityName; }

    RPROPERTY(Serialize)
    std::string sAbilityName;

    // ...

    Ability_GENERATED
};

class RCLASS(Guid("36063853-79b1-41e6-afa6-6923c8b24815")) PlayerSaveData : public Serializable {
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

    // Can also store types that derive from `Ability` without any serialization/deserialization issues.
    RPROPERTY(Serialize)
    std::vector<std::shared_ptr<Ability>> vAbilities;

    PlayerSaveData_GENERATED
};

File_PlayerSaveData_GENERATED

// ---------------------------------------

{
    // Somewhere in the game code.
    gc<PlayerSaveData> pPlayerSaveData;

    // ... if the user creates a new player profile ...
    pPlayerSaveData = gc_new<PlayerSaveData>();

    // Fill save data with some information.
    pPlayerSaveData->sCharacterName = "Player 1";
    pPlayerSaveData->iCharacterLevel = 42;
    pPlayerSaveData->iExperiencePoints = 200;
    pPlayerSaveData->vAbilities = {std::make_shared<Ability>("Fire"), std::make_shared<Ability>("Wind")};
    pPlayerSaveData->inventory.addOneItem(42);
    pPlayerSaveData->inventory.addOneItem(42); // now have two items with ID "42"
    pPlayerSaveData->inventory.addOneItem(102);

    // Prepare new file name.
    const std::string sNewProfileName = ConfigManager::getFreeProgressProfileName();

    // Serialize.
    const auto pathToFile =
        ConfigManager::getCategoryDirectory(ConfigCategory::PROGRESS) / sNewProfileName;
    const auto optionalError = pPlayerSaveData->serialize(pathToFile, true);
    if (optionalError.has_value()) {
        // process error
    }
}

// ... when the game is started next time ...

{
    // Get all save files.
    const auto vProfiles = ConfigManager::getAllFileNames(ConfigCategory::PROGRESS);

    // ... say the user picks the first profile ...
    const auto sProfileName = vProfiles[0];

    // Deserialize.
    const auto pathToFile = ConfigManager::getCategoryDirectory(ConfigCategory::PROGRESS) / sProfileName;
    std::unordered_map<std::string, std::string> foundCustomAttributes;
    const auto result = Serializable::deserialize<PlayerSaveData>(pathToFile, foundCustomAttributes);
    if (std::holds_alternative<Error>(result)) {
        // process error
    }

    gc<PlayerSaveData> pPlayerSaveData = std::get<gc<PlayerSaveData>>(result);

    // Everything is loaded:
    assert(pPlayerSaveData->sCharacterName == "Player 1");
    assert(pPlayerSaveData->iCharacterLevel == 42);
    assert(pPlayerSaveData->iExperiencePoints == 200);
    assert(pPlayerSaveData->vAbilities.size() == 2);
    assert(pPlayerSaveData->vAbilities[0]->sAbilityName == "Fire");
    assert(pPlayerSaveData->vAbilities[1]->sAbilityName == "Wind");
    assert(pPlayerSaveData->inventory.getItemAmount(42) == 2);
    assert(pPlayerSaveData->inventory.getItemAmount(102) == 1);
}
```
</details>

<details>
<summary>Automatic shader caching and cache invalidation</summary>
  
```C++
const auto vertexShader = ShaderDescription(
    "engine.default.vs",               // shader name
    "res/engine/shaders/default.hlsl", // path to shader file
    ShaderType::VERTEX_SHADER,         // shader type
    "vsDefault",                       // shader entry function name
    {});                               // defined shader macros

const auto pixelShader = ShaderDescription(
    "engine.default.ps",               // shader name
    "res/engine/shaders/default.hlsl", // path to shader file
    ShaderType::PIXEL_SHADER,          // shader type
    "psDefault",                       // shader entry function name
    {});                               // defined shader macros

std::vector vShaders = {vertexShader, pixelShader};

auto onProgress = [](size_t iCompiledShaderCount, size_t iTotalShadersToCompile) {
    // show progress here
};
auto onError = [](ShaderDescription shaderDescription, std::variant<std::string, Error> error) {
    if (std::holds_alternative<std::string>(error)){
        // shader compilation error
    }else{
        // internal error
    }
}
auto onCompleted = []() {
    // do final logic here
};

// shader compilation is done using multiple threads via a thread pool
getWindow()->getRenderer()->getShaderManager()->compileShaders(
    vShaders,
    onProgress,
    onError,
    onCompleted
);

// you can now reference these shaders by their name

// ... on the next program start ...
// once requested to compile these shaders again (re-run the same code from above)
// they will be retrieved from the cache
// if shader's .hlsl/.glsl code or any of shader's included files were not changed
// otherwise it will be recompiled
```
</details>

# Current Development Status

Currently the engine is in a very early development state and right now it's still impossible to make games with this engine.

Here is the list of base features that needs to be implemented in order for you to be able to make games:

- [X] Reflection system
- [X] Serialization/Deserialization for game assets
- [X] Garbage collector
- [X] Handling user input events
- [X] Config management (progress, settings, etc.)
- [X] Node system (Godot-like ECS alternative)
- [ ] Profiler
- [ ] GLTF/GLB import
- [ ] Rendering (features for both DirectX 12 and Vulkan renderers)
    - [ ] Forward rendering
        - [ ] Transparent materials
        - [ ] MSAA (Multisample anti-aliasing)
        - [ ] Frustum culling
        - [ ] Some post-processing effects
    - [ ] Forward+ rendering
        - [ ] Transparent materials
        - [ ] AA (Anti-aliasing)
        - [ ] Frustum culling
        - [ ] Instancing
        - [ ] Light sources
            - [ ] Point light
            - [ ] Spot light
            - [ ] Directional light
            - [ ] Rectangular light
        - [ ] Shadow mapping (for all light sources)
        - [ ] Cascading shadow maps
        - [ ] Normal mapping
        - [ ] Cube mapping
        - [ ] Z-prepass
        - [ ] SSAO (Screen space ambient occlusion)
        - [ ] Post-processing effects
        - [ ] PBR (Physically based rendering)
        - [ ] HDR (High dynamic range)
        - [ ] Decals
        - [ ] Transparent objects drawing order fix (either draw back-to-front or use some order-independent method)
        - [ ] Emissive materials
    - [ ] Deferred rendering
        - [ ] Transparent materials
        - [ ] AA (Anti-aliasing)
        - [ ] Frustum culling
        - [ ] Instancing
        - [ ] Light sources
            - [ ] Point light
            - [ ] Spot light
            - [ ] Directional light
            - [ ] Rectangular light
        - [ ] Shadow mapping (for all light sources)
        - [ ] Cascading shadow maps
        - [ ] Normal mapping
        - [ ] Cube mapping
        - [ ] Z-prepass
        - [ ] SSAO (Screen space ambient occlusion)
        - [ ] Post-processing effects
        - [ ] PBR (Physically based rendering)
        - [ ] HDR (High dynamic range)
        - [ ] Decals
        - [ ] Transparent objects drawing order fix (either draw back-to-front or use some order-independent method)
        - [ ] Emissive materials
    - [ ] GUI
- [ ] Skeletal animations
- [ ] Minimal scripting using AngelScript
- [ ] Editor
    - [ ] Content management
    - [ ] Script debugging
    - [ ] Script syntax highlighting, scripting helpers
- [ ] Audio engine
    - [ ] 2D audio
    - [ ] 3D audio
    - [ ] Audio streaming
    - [ ] Sound effects
- [ ] Physics engine
    - [ ] Rigid body
    - [ ] Ragdoll
    - [ ] Soft body
    - [ ] Joints
- [ ] Automatic LODs (Level of details)
- [ ] AI and pathfinding
- [ ] Particle effects

Once base renderers will be implemented I will publish a manual that contains general documentation and a step by step guide for working with the engine. Also once all base features will be implemented I will create a separate repository for examples and add a link to it here.

# Setup (Build)

Prerequisites:

- compiler that supports C++23 (latest MSVC/Clang)
- [CMake](https://cmake.org/download/)
- [Doxygen](https://doxygen.nl/download.html)
- [LLVM](https://github.com/llvm/llvm-project/releases/latest)
- [Go](https://go.dev/dl/)
- prerequisites for Linux:
    - `libtinfo.so` might not be installed on your system but is required
    - `libclang` (needed for reflection code generator), **after** CMake is configured `ext/Refureku/build/Bin/` will contain needed libraries, you would need to create the file `/etc/ld.so.conf.d/nameless-engine.conf` with the path to this directory and run `sudo ldconfig` so that these libraries will be found by the reflection generator

First, clone this repository:

```
git clone https://github.com/Flone-dnb/nameless-engine
cd nameless-engine
git submodule update --init --recursive
```

Then, if you've never used CMake before:

Create a `build` directory next to this file, open created `build` directory and type `cmd` in Explorer's address bar. This will open up a console in which you need to type this:

```
cmake -DCMAKE_BUILD_TYPE=Debug .. // for debug mode
cmake -DCMAKE_BUILD_TYPE=Release .. // for release mode
```

This will generate project files that you will use for development.

# Update

To update this repository:

```
git pull
git submodule update --init --recursive
```

# Documentation

In order to generate the documentation you need to have [Doxygen](https://www.doxygen.nl/index.html) installed.

The documentation can be generated by executing the `doxygen` command while being in the `docs` directory. If Doxygen is installed, this will be done automatically on each build.

The generated documentation will be located at `docs/gen/html`, open the `index.html` file from this directory to see the documentation.

# Code style

Mostly engine code style is controlled though `clang-format` and `clang-tidy` (although `clang-tidy` is only enabled for release builds), configuration for both of these is located in the root directory of this repository. Nevertheless, there are few things that those two can't control, which are:

- for `bool` variables the prefix is `b`, example: `bIsEnabled`,
- for integer variables (`int`, `size_t`, etc.) the prefix is `i`, example: `iSwapChainBufferCount`,
- for string variables (`std::string`, `std::string_view`, etc.) the prefix is `s`, example: `sNodeName`,
- for vector variables (`std::vector`, `std::array`, etc.) the prefix is `v`, example: `vActionEvents`,
- additionally, if you're using mutex to guard specific field(s) use `std::pair` if possible, here are some examples:

```C++
std::pair<std::recursive_mutex, gc<Node>> mtxParentNode;

struct LocalSpaceInformation {
    glm::mat4x4 relativeRotationMatrix = glm::identity<glm::mat4x4>();
    glm::quat relativeRotationQuaternion = glm::identity<glm::quat>();
};

std::pair<std::recursive_mutex, LocalSpaceInformation> mtxLocalSpace;
```

Make sure you are naming your variables according to this when writing engine code.