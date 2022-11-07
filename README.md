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
    RPROPERTY()
    bool bBoolValue = false;

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
// Reflected fields will be serialized to file and deserialized from file.
// ---------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "io/Serializable.h"

#include "PlayerSaveData.generated.h"

/// Holds information about player's inventory.
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

/// Holds information about player's data.
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

File_PlayerSaveData_GENERATED

// --------------------------------------------

{
    // Somewhere in the game code.
    gc<PlayerSaveData> pPlayerSaveData;

    // ... if the user creates a new player profile ...
    pPlayerSaveData = gc_new<PlayerSaveData>();

    // Fill save data with some information.
    pPlayerSaveData->sCharacterName = "Player 1";
    pPlayerSaveData->iCharacterLevel = 42;
    pPlayerSaveData->iExperiencePoints = 200;
    pPlayerSaveData->vAbilities = {241, 3122, 22};
    pPlayerSaveData->inventory.addOneItem(42);
    pPlayerSaveData->inventory.addOneItem(42); // now have two items with ID "42"
    pPlayerSaveData->inventory.addOneItem(102);

    // Prepare new profile file name.
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned short int> uid(0, USHRT_MAX);
    std::string sNewProfileFilename;
    const auto vExistingProfiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);
    bool bContinue = false;
    do {
        bContinue = false;
        sNewProfileFilename = std::to_string(uid(gen));
        for (const auto& sProfile : vExistingProfiles) {
            if (sProfile == sNewProfileFilename) {
                // This profile name is already used, generate another one.
                bContinue = true;
                break;
            }
        }
    } while (bContinue);

    // Serialize.
    const auto pathToFile =
        ConfigManager::getCategoryDirectory(ConfigCategory::PROGRESS) / sNewProfileFilename;
    const auto optionalError = pPlayerSaveData->serialize(pathToFile, true);
    if (optionalError.has_value()) {
        // process error
    }
}

// ... when the game is started next time ...

{
    // Get all save files.
    const auto vProfiles = ConfigManager::getAllFiles(ConfigCategory::PROGRESS);

    // ... say the user picks the first profile ...
    const auto sProfileName = vProfiles[0];

    // Deserialize.
    const auto pathToFile = ConfigManager::getCategoryDirectory(ConfigCategory::PROGRESS) / sProfileName;
    const auto result = Serializable::deserialize<PlayerSaveData>(pathToFile);
    if (std::holds_alternative<Error>(result)) {
        // process error
    }

    gc<PlayerSaveData> pPlayerSaveData = std::get<gc<PlayerSaveData>>(result);

    // Everything is loaded:
    assert(pPlayerSaveData->sCharacterName == "Player 1");
    assert(pPlayerSaveData->iCharacterLevel == 42);
    assert(pPlayerSaveData->iExperiencePoints == 200);
    assert(pPlayerSaveData->vAbilities == std::vector<unsigned long long>{241, 3122, 22});
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
- [ ] Rendering
    - [ ] DirectX 12
        - [ ] Forward rendering
            - [ ] Transparent materials
            - [ ] MSAA (Multisample anti-aliasing)
            - [ ] Frustum culling
            - [ ] Instancing
            - [ ] Light sources
                - [ ] Point light
                - [ ] Spot light
                - [ ] Directional light
                - [ ] Rectangular light
            - [ ] Shadow mapping (for all light sources)
            - [ ] Cascading shadow maps
            - [ ] SSAO (Screen space ambient occlusion)
            - [ ] Post-processing effects
            - [ ] PBR (Physically based rendering)
            - [ ] HDR (High dynamic range)
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
            - [ ] SSAO (Screen space ambient occlusion)
            - [ ] Post-processing effects
            - [ ] PBR (Physically based rendering)
            - [ ] HDR (High dynamic range)
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
            - [ ] SSAO (Screen space ambient occlusion)
            - [ ] Post-processing effects
            - [ ] PBR (Physically based rendering)
            - [ ] HDR (High dynamic range)
        - [ ] GUI
    - [ ] Vulkan
        - [ ] Forward rendering
            - [ ] Transparent materials
            - [ ] MSAA (Multisample anti-aliasing)
            - [ ] Frustum culling
            - [ ] Instancing
            - [ ] Light sources
                - [ ] Point light
                - [ ] Spot light
                - [ ] Directional light
                - [ ] Rectangular light
            - [ ] Shadow mapping (for all light sources)
            - [ ] Cascading shadow maps
            - [ ] SSAO (Screen space ambient occlusion)
            - [ ] Post-processing effects
            - [ ] PBR (Physically based rendering)
            - [ ] HDR (High dynamic range)
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
            - [ ] SSAO (Screen space ambient occlusion)
            - [ ] Post-processing effects
            - [ ] PBR (Physically based rendering)
            - [ ] HDR (High dynamic range)
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
            - [ ] SSAO (Screen space ambient occlusion)
            - [ ] Post-processing effects
            - [ ] PBR (Physically based rendering)
            - [ ] HDR (High dynamic range)
        - [ ] GUI
- [ ] Editor
    - [ ] Content management
    - [ ] Script debugging
    - [ ] Script syntax highlighting, scripting helpers
- [ ] Scripting
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
- [ ] Skeletal animations
- [ ] AI and pathfinding
- [ ] Particle effects

Once these base features will be implemented I will create a separate repository for examples and add a link to it here.

# Setup (Build)

Prerequisites:

- compiler that supports C++23
- [CMake](https://cmake.org/download/)
- [Doxygen](https://doxygen.nl/download.html)
- [Go](https://go.dev/dl/)
- prerequisites for Linux:
    - `libtinfo.so` might not be installed but is required (on Archlinux can be found in [ncurses5-compat-libs](https://aur.archlinux.org/packages/ncurses5-compat-libs) AUR package)

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

This will generate engine editor project files that you will use for development.

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

Mostly engine's code style is controlled though `clang-format` and `clang-tidy`, configuration for both of these is located in the root of this repository, make sure you have those enabled. Nevertheless, there are few things that those two can't control, which are:

- for `bool` variables the prefix is `b`, example: `bIsEnabled`,
- for integer variables (`int`, `size_t`, etc.) the prefix is `i`, example: `iSwapChainBufferCount`,
- for string variables (`std::string`, `std::string_view`, etc.) the prefix is `s`, example: `sNodeName`,
- for vector variables (`std::vector`, `std::array`, etc.) the prefix is `v`, example: `vActionEvents`.

Make sure you are naming your variables according to this when writing engine code.