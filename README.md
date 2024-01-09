# nameless-engine

TODO: screenshot game/editor

### Supported platforms

* Windows
* Linux

# Core features

Click on any line below to show an example:

<details>
<summary>Unreal Engine-like reflection via code generation</summary>
  
```C++
// MyClass.cpp

#include "MyClass.generated_impl.h"

// MyClass.h

// other includes

#include "MyClass.generated.h" // should be included last
 
class RCLASS() MyClass {
public:
    MyClass() = default;
    virtual ~MyClass() override = default;

    RFUNCTION()
    void Foo();

private:
    RPROPERTY()
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

/// Stores character's inventory data that we will save to file and load from file.
class RCLASS(Guid("a34a8047-d7b4-4c70-bb9a-429875a8cd26")) InventorySaveData : public Serializable {
public:
    InventorySaveData() = default;
    virtual ~InventorySaveData() override = default;
    
    /// Contains item ID as a key and item amount (in the inventory) as a value.
    /// This data might not be your actual in-game inventory instead it may store some minimal data
    /// that you can use to create an actual inventory when loading user's progress.
    RPROPERTY(Serialize)
    std::unordered_map<unsigned long long, unsigned long long> items;

    InventorySaveData_GENERATED
};

/// Type that we will serialize and deserialize.
/// Contains objects of `InventorySaveData` type plus some additional data.
class RCLASS(Guid("36063853-79b1-41e6-afa6-6923c8b24815")) PlayerSaveData : public Serializable {
public:
    PlayerSaveData() = default;
    virtual ~PlayerSaveData() override = default;

    RPROPERTY(Serialize)
    std::string sCharacterName;

    RPROPERTY(Serialize)
    unsigned long long iCharacterLevel = 0;

    /// We can serialize fields of custom user types if they also derive from `Serializable`.
    RPROPERTY(Serialize)
    InventorySaveData inventory;

    PlayerSaveData_GENERATED
};

File_PlayerSaveData_GENERATED

// ---------------------------------------

{
    // Somewhere in the game code.
    // (we can use either `std::shared_ptr` or `gc` pointer for deserialized data, you will see below)
    std::shared_ptr<PlayerSaveData> pPlayerSaveData = nullptr;

    // ... if the user creates a new player profile ...
    pPlayerSaveData = std::make_shared<PlayerSaveData>();

    // Fill save data with some information.
    pPlayerSaveData->sCharacterName = "Player 1";
    pPlayerSaveData->iCharacterLevel = 42;
    pPlayerSaveData->inventory[42] = 2;

    // Prepare a new unique file name for the new save file.
    const std::string sNewProfileName = ConfigManager::getFreeProgressProfileName();

    // Serialize.
    const auto pathToFile =
        ConfigManager::getCategoryDirectory(ConfigCategory::PROGRESS) / sNewProfileName;
    const auto optionalError = pPlayerSaveData->serialize(pathToFile, true); // `true` to enable backup file
    if (optionalError.has_value()) [[unlikely]] {
        // ... handle error ...
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
    const auto result =
        Serializable::deserialize<std::shared_ptr, PlayerSaveData>(pathToFile, foundCustomAttributes);
    if (std::holds_alternative<Error>(result)) [[unlikely]] {
        // ... handle error ...
    }

    const auto pPlayerSaveData = std::get<std::shared_ptr<PlayerSaveData>>(result);

    // Everything is deserialized:
    assert(pPlayerSaveData->sCharacterName == "Player 1");
    assert(pPlayerSaveData->iCharacterLevel == 42);
    assert(pPlayerSaveData->inventory.find(42)->second == 2);
}
```
</details>

<details>
<summary>Automatic shader caching and cache invalidation</summary>
  
```C++
const auto vertexShader = ShaderDescription(
    "game.test.vs",                    // shader name
    "res/game/shaders/test.hlsl",      // path to shader file
    ShaderType::VERTEX_SHADER,         // shader type
    "main",                            // shader entry function name
    {});                               // defined shader macros

const auto pixelShader = ShaderDescription(
    "game.test.fs",                    // shader name
    "res/game/shaders/test.hlsl",      // path to shader file
    ShaderType::FRAGMENT_SHADER,       // shader type (`FRAGMENT_SHADER` is pixel shader for HLSL)
    "main",                            // shader entry function name
    {{"FOO", "42"}});                  // defined shader macros

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

# Roadmap

Currently the engine is in a very early development state and right now it's still impossible to make games with this engine.

Here is the list of base features that needs to be implemented in order for you to be able to make games:

- [X] Reflection system
- [X] Serialization/deserialization for game assets
- [X] Garbage collector
- [X] Handling user input events
- [X] Config management (progress, settings, etc.)
- [X] Node system (Godot-like ECS alternative)
- [X] Profiler
- [X] GLTF/GLB import
- [ ] Rendering (features for both DirectX 12 and Vulkan renderers)
    - [X] Automatically test and pick the most suitable renderer/GPU based on the OS/hardware capabilities/user preferences at runtime
    - [X] MSAA (Multisample anti-aliasing)
    - [X] Transparent materials
    - [X] Diffuse textures
    - [X] Multiple materials per mesh
    - [X] Frustum culling
    - [X] Simple API for using custom vertex/pixel/fragment/compute shaders
    - [X] Z-prepass
    - [X] Forward+ (Tiled Forward) rendering
    - [X] Light sources
        - [X] Point light
        - [X] Spot light
        - [X] Directional light
    - [ ] Shadow mapping (for all light sources)
    - [ ] Cascading shadow maps
    - [ ] Space partitioning and multi-threading for frustum culling
    - [ ] Post-processing effects
    - [ ] HDR (High dynamic range)
    - [ ] Normal mapping
    - [ ] Cube mapping
    - [ ] Instancing
    - [ ] SSAO (Screen space ambient occlusion)
    - [ ] PBR (Physically based rendering)
    - [ ] Decals
    - [ ] Order independent transparency
    - [ ] Occlusion culling
    - [ ] Emissive materials
    - [ ] Light culling improvements
    - [ ] Multi-threaded command list/buffer generation
    - [ ] Shadow caster culling
    - [ ] GUI
- [ ] Skeletal animations
- [ ] Rewrite garbage collector [tgc2](https://github.com/Flone-dnb/tgc2) (to fix some major issues)
- [ ] Minimal scripting using [AngelScript](https://www.angelcode.com/angelscript/)
- [ ] Editor (integrate [ImGui](https://github.com/ocornut/imgui))
    - [ ] Content management
    - [ ] Script debugging
    - [ ] Script syntax highlighting, scripting helpers
- [ ] Audio engine (integrate [SoLoud](https://github.com/jarikomppa/soloud))
    - [ ] 2D audio
    - [ ] 3D audio
    - [ ] Audio streaming
    - [ ] Sound effects
- [ ] Physics engine (integrate [Jolt](https://github.com/jrouwe/JoltPhysics) or [Bullet3](https://github.com/bulletphysics/bullet3))
    - [ ] Rigid body
    - [ ] Ragdoll
    - [ ] Soft body
    - [ ] Joints
- [ ] Automatic LODs (Level of details)
- [ ] AI and pathfinding
- [ ] Particle effects

Once all base features will be implemented I will create a separate repository for examples and add a link to it here.

# Documentation

Documentation for this engine consists of 2 parts: API reference (generated from C++ comments) and the manual (at `docs/Manual.md`), generated documentation includes both API reference and a manual (copied from `docs/Manual.md`).

In order to generate the documentation you need to have [Doxygen](https://www.doxygen.nl/index.html) installed.

The documentation can be generated by executing the `doxygen` command while being in the `docs` directory. If Doxygen is installed, this will be done automatically on each build.

Generated documentation will be located at `docs/gen/html`, open the `index.html` file from this directory to view the documentation.

# Setup (Build)

Prerequisites:

- compiler that supports C++23 (latest MSVC/Clang)
- [CMake](https://cmake.org/download/)
- [Doxygen](https://doxygen.nl/download.html)
- [LLVM](https://github.com/llvm/llvm-project/releases/latest)
- [Go](https://go.dev/dl/)
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
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

# Code style

The following is a code style rules for engine developers not for game developers (although if you prefer you can follow these rules even if you're not writing engine code but making a game).

Mostly engine code style is controlled though `clang-format` and `clang-tidy` (although `clang-tidy` is only enabled for release builds so make sure your changes compile in release mode), configuration for both of these is located in the root directory of this repository. Nevertheless, there are several things that those two can't control, which are:

## Use prefixes for variables/fields

Some prefixes are not controlled by `clang-tidy`:

- for `bool` variables the prefix is `b`, example: `bIsEnabled`,
- for integer variables (`int`, `size_t`, etc.) the prefix is `i`, example: `iSwapChainBufferCount`,
- for string variables (`std::string`, `std::string_view`, etc.) the prefix is `s`, example: `sNodeName`,
- for vector variables (`std::vector`, `std::array`, etc.) the prefix is `v`, example: `vActionEvents`,
- additionally, if you're using a mutex to guard specific field(s) use `std::pair` if possible, here are some examples:

```C++
std::pair<std::recursive_mutex, gc<Node>> mtxParentNode;

struct LocalSpaceInformation {
    glm::mat4x4 relativeRotationMatrix = glm::identity<glm::mat4x4>();
    glm::quat relativeRotationQuaternion = glm::identity<glm::quat>();
};

std::pair<std::recursive_mutex, LocalSpaceInformation> mtxLocalSpace;
```

## Sort included headers

Sort your `#include`s and group them like this:

```C++
// Standard.
#include <variant>
#include <memory>

// Custom.
#include "render/general/resources/GpuResource.h"

// External.
#include "vulkan/vulkan.h"
```

where `Standard` refers to headers from the standard library, `Custom` refers to headers from the engine/game and `External` refers to headers from external dependencies.

## Directory/file naming

Directories in the `src` directory are named with 1 lowercase word (preferably without underscores), for example:

```
render\general\resources
```

used to store `render`-specific source code, `general` means API-independent (DirectX/Vulkan) and `resources` refers to GPU resources.

Files are named using CamelCase, for example: `EditorGameInstance.h`.

## Some header rules

- if you're using `friend class` specify it in the beginning of the `class` with a small description, for example:

```C++
/**
 * Represents a descriptor (to a resource) that is stored in a descriptor heap.
 * Automatically marked as unused in destructor.
 */
class DirectXDescriptor {
    // We notify the heap about descriptor being no longer used in destructor.
    friend class DirectXDescriptorHeap;

    // ... then goes other code ...
```

generally `friend class` is used to hide some internal object communication functions from public section to avoid public API user from shooting himself in the foot and causing unexpected behaviour.

- don't duplicate access modifiers in your class/struct, so don't write code like this:

```C++
class Foo{
public:
    // ... some code here ...

private:
    // ... some code here ...

public: // <- don't do that as `public` was already specified earlier
    // ... some code here...
};
```

- specify access modifiers in the following order: `public`, `protected` and finally `private`, for example:

```C++
class Foo{
public:
    // ... some code here ...

protected:
    // ... some code here ...

private:
    // ... some code here...
};
```

- don't mix function with fields: specify all functions first and only then fields, for example:

```C++
class Foo{
public:
    void foo();

protected:
    void bar();

private:
    void somefunction1();

    void somefunction2();

    // now all functions were specified and we can specify all fields

    int iAnswer = 42;

    static constexpr bool bEnable = false;
};
```

- don't store fields in the `public`/`protected` section, generally you should specify all fields in the `private` section and provide getters/setters in other sections
    - but there are some exceptions to this rule such as `struct`s that just group a few variables - you don't need to hide them in `private` section and provide getters although if you think this will help or you want to do something in your getters then you can do that
    - for inheritance generally you should still put all base class fields in the `private` section and for derived classes provide a `protected` getter/setter if you need, although there are also some exceptions to this

- put all `static constexpr` / `static inline const` fields in the bottom of your or class, for example:

```C++
private:
    // ... some code here ...

    /** Index of the root parameter that points to `cbuffer` with frame constants. */
    static constexpr UINT iFrameConstantBufferRootParameterIndex = 0;
}; // end of class
```

- if your function can fail use engine's `Error` class as a return type (instead of logging an error and returning nothing), for example:

```C++
static std::variant<std::unique_ptr<VulkanResource>, Error> create(...);

[[nodiscard]] std::optional<Error> initialize(Renderer* pRenderer) override;
```

- if your function returns `std::optional<Error>` mark it as `[[nodiscard]]`

## Some implementation rules

- avoid nesting, for example:

bad:

```C++
// Make sure the specified file exists.
if (std::filesystem::exists(shaderDescription.pathToShaderFile)) [[likely]] {
    // Make sure the specified path is a file.
    if (!std::filesystem::is_directory(shaderDescription.pathToShaderFile)) [[likely]] {
        // Create shader cache directory if needed.
        if (!std::filesystem::exists(shaderCacheDirectory)) {
            std::filesystem::create_directory(shaderCacheDirectory);
        }

        // ... some other code ...
    }else{
        return Error(fmt::format(
            "the specified shader path {} is not a file", shaderDescription.pathToShaderFile.string()));
    }
}else{
    return Error(fmt::format(
        "the specified shader file {} does not exist", shaderDescription.pathToShaderFile.string()));
}
```

good:

```C++
// Make sure the specified file exists.
if (!std::filesystem::exists(shaderDescription.pathToShaderFile)) [[unlikely]] {
    return Error(fmt::format(
        "the specified shader file {} does not exist", shaderDescription.pathToShaderFile.string()));
}

// Make sure the specified path is a file.
if (std::filesystem::is_directory(shaderDescription.pathToShaderFile)) [[unlikely]] {
        return Error(fmt::format(
            "the specified shader path {} is not a file", shaderDescription.pathToShaderFile.string()));
}

// Create shader cache directory if needed.
if (!std::filesystem::exists(shaderCacheDirectory)) {
    std::filesystem::create_directory(shaderCacheDirectory);
}
```

- prefer to group your implementation code into small chunks without empty lines with a comment in the beginning, all chunks should be separated by 1 empty line, for example:

```C++
// Specify defined macros for this shader.
auto vParameterNames = convertShaderMacrosToText(macros);
for (const auto& sParameter : vParameterNames) {
    currentShaderDescription.vDefinedShaderMacros.push_back(sParameter);
}

// Add hash of the configuration to the shader name for logging.
const auto sConfigurationText = ShaderMacroConfigurations::convertConfigurationToText(macros);
currentShaderDescription.sShaderName += sConfigurationText;

// Add hash of the configuration to the compiled shader file name
// so that all shader variants will be stored in different files.
auto currentPathToCompiledShader = pathToCompiledShader;
currentPathToCompiledShader += sConfigurationText;

// Try to load the shader from cache.
auto result = Shader::createFromCache(
    pRenderer,
    currentPathToCompiledShader,
    currentShaderDescription,
    shaderDescription.sShaderName,
    cacheInvalidationReason);
if (std::holds_alternative<Error>(result)) {
    // Shader cache is corrupted or invalid. Delete invalid cache directory.
    std::filesystem::remove_all(
        ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName);

    // Return error that specifies that cache is invalid.
    auto err = std::get<Error>(std::move(result));
    err.addCurrentLocationToErrorStack();
    return err;
}

// Save loaded shader to shader pack.
pShaderPack->mtxInternalResources.second.shadersInPack[macros] =
    std::get<std::shared_ptr<Shader>>(result);
```
