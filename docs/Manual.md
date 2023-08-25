# Manual

This is a manual - a step-by-step guide to introduce you to various aspects of the engine. More specific documentation can be always found in the class/function/variable documentation in the source code or on this page (see left panel for navigation), every code entity is documented so you should not get lost.

Note that this manual contains many sections and instead of scrolling this page you can click on a little arrow button in the left (navigation) panel, the little arrow button becomes visible once you hover your mouse cursor over a section name, by clicking on that litte arrow you can expand the section and see its subsections and quickly jump to needed sections.

# Prerequisites

First of all, check out repository's `README.md` file, specifically the `Prerequisites` section, make sure you have all required things installed.

The engine relies on CMake as its build system. CMake is a very common build system for C++ projects so generally most of the developers should be familiar with it. If you don't what CMake is or haven't used it before it's about the time to learn the basics of CMake right now. Your project will be represented as several CMake targets: one target for standalone game, one target for tests, one target for editor and etc. So you will work on `CMakeLists.txt` files as with usual C++! There are of course some litte differences compared to the usual CMake usage, for example if you want to define external dependencing in your CMake file you need to do one additional step (compared to the usual CMake usage) that will be covered later in a separate section.

# Creating a new project

## Preparing project path (only for Windows users)

Because Windows limits paths to ~255 characters it's impossible to operate on files that have long paths, thus, you need to make sure that path to your project directory will not be very long, this does not mean that you need to create your project in disk root (for ex. C:\\myproject) - there's no need for that but the shorter the path to the project the most likely you won't have weird issues with your project. If you need exact numbers a path with 30-40 characters will be good but you can try using longer paths. Just don't forget about this path limitation thing.

## Project generator

Currently we don't have a proper project generator but it's planned. Once the project generator will be implemented this section will be updated.

Right now you can clone the repository and don't forget to update submodules. Once you have a project with `CMakeLists.txt` file in the root directory you can open it in your IDE. For example Qt Creator or Visual Studio allow opening `CMakeLists.txt` files as C++ projects, other IDEs also might have this functionality.

Note for Visual Studio users:
> If you use Visual Studio the proper way to work on `CMakeLists.txt` files as C++ projects is to open up Visual Studio without any code then press `File` -> `Open` -> `Cmake` and select the `CMakeLists.txt` file in the root directory (may be changed in the new VS versions). Then a tab called `CMake Overview Pages` should be opened in which you might want to click `Open CMake Settings Editor` and inside of that change `Build root` to just `${projectDir}\build\` to store built data inside of the `build` directory (because by default Visual Studio stores build in an unusual path `out/<build mode>/`). When you open `CMakeLists.txt` in Visual Studio near the green button to run your project you might see a text `Select Startup Item...`, you should press a litte arrow near this text to expand a list of available targets to use. Then select a target that you want to build/use and that's it, you are ready to work on the project.

Note for Windows 10 users:
> Windows 10 users need to run their IDE with admin privileges when building the project for the first time (only for the first build) when executing a post-build script the engine creates a symlink next to the built executable that points to the directory with engine/editor/game resources (called `res`). Creating symlinks on Windows 10 requires admin privileges. When releasing your project we expect you to put an actual copy of your `res` directory next to the built executable but we will discuss this topic later in a separate section.

Before you go ahead and dive into the engine yourself make sure to read a few more sections, there is one really important section further in the manual that you have to read, it contains general tips about things you need to keep an eye out!

## Project structure

Let's looks at the directories/files that a typical project will have:
- `build` this directory generally used in CMake projects to store built binaries.
- `docs` this directory stores documentation, generally it contains a `Doxyfile` config that `doxygen` (generates documentation pages from source code comments) uses and maybe a hand-written documentation page (a manual, like the one your are reading right now).
- `ext` this directory is used to store external dependencies, generally it stores git submodules but sometimes it also stores non-submodule directories.
- `res` this directory stores all resources (shaders, images, 3D objects, sound, etc.), if you look in this directory you might see subdirectories like `editor`, `engine`, `game` and `test`, each one stores resources for one specific thing, `editor` stores editor resources, `game` stores resources that your game needs, `test` stores resources used in automated testing and etc.
- `src` this directory stores all source code: source code for the editor, engine, your game, etc.
- `.clang-format` this file is used by `clang-format` a code formatter that automatically formats our source code according to the config file, we will talk about code formatting in one of the next sections.
- `.clang-tidy` this file is used by `clang-tidy` a static analyzer that checks our code for common mistakes/bugs and also controls some code style, we will talk about static analyzers in one of the next sections.
- `CMakeLists.txt` is a top-level CMake file that we open in our IDEs, it adds other CMake files from the `src` directory.

The `src` directory contains a directory per CMake target, for example: `editor` (executable CMake target), `editor_lib` (library CMake target) and some additional helper directories such as `.cmake` (contains helper CMake functions) and `.scripts` (contains Go scripts that we use in our CMake files).

Inside of each CMake target directory will be a target-specific `CMakeLists.txt` file (for example: `src/editor_lib/CMakeLists.txt`), directories for source code and maybe a `.generated` directory that contains reflection code (we will talk about source code directories and reflection in one of the next sections).

Your game target `CMakeLists.txt` is fully yours, it will have some configuration code inside of it but you are free to change it as you want (you can disable/change `doxygen`, `clang-tidy`, reflection generation and anything else you don't like).

# What header files to include and what not to include

## General

`src/engine_lib` is divided into 2 directories: public and private. You are free to include anything from the `public` directory, you can also include header files from the `private` directory in some special/advanced cases but generally there should be no need for that. Note that this division (public/private) is only conceptual, your project already includes both directories because some engine `public` headers use `private` headers and thus both included in cmake targets that use the engine (in some cases it may cause your IDE to suggest lots of headers when attempting to include some header from some directory so it may be helpful to just look at the specific public directory to look for the header name if you feel overwhelmed).

Inside of the `public` directory you will see other directories that group header files by their purpose, for example `io` directory stands for `in/out` which means that this directory contains files for working with disk (loading/saving configuration files, logging information to log files that are stored on disk, etc.).

You might also notice that some header files have `.h` extension and some have `.hpp` extension. The difference here is only conceptual: files with `.hpp` extension don't have according `.cpp` file, this is just a small hint for developers.

You are not required to use the `private`/`public` directory convention, moreover directories that store executable cmake targets just use `src` directory (you can look into `engine_tests` or `editor` directories to see an example) so you can group your source files as you want.

## Math headers

The engine uses GLM (a well known math library, hosted on https://github.com/g-truc/glm). Although you can include original GLM headers it's highly recommended to include the header `math/GLMath.hpp` instead of the original GLM headers when math is needed. This header includes main GLM headers and defines engine specific macros so that GLM will behave as the engine expects.

You should always prefer to include `math/GLMath.hpp` instead of the original GLM headers and only if this header does not have needed functionality you can include original GLM headers afterwards.

# Automatic code formatters and static analyzers

The engine uses `clang-format` and `clang-tidy` a classic pair of tools that you will commonly find in C++ projects. If you don't know what they do it's a good time to read about them on the Internet.

The engine does not require you to use them but their usage is highly recommended.

`clang-format` can be used in your IDE to automatically format your code (for example) each time you press Ctrl+S. If you want to make sure that your IDE is using our `.clang-format` config you can do the following check: in your source code create 2 or more consecutive empty lines, since our `.clang-format` config contains a rule `MaxEmptyLinesToKeep: 1` after you format the file only 1 empty line should remain. The action with which you format your source code depends on your IDE settings that you might want to configure, generally IDEs have a shortcut to "format" your source code but some have option to automatically use "format" action when you are saving your file.

`clang-tidy` has a lot of checks enabled and is generally not that fast as you might expect, because of this we have `clang-tidy` enabled only in release builds to speed up build times for debug builds. This means that if your game builds in debug mode it may very well fail to build in release mode due to some `clang-tidy` warnings that are treated as errors. Because of this it's highly recommended to regularly (say once or twice a week) build your project in release mode to check for `clang-tidy` warnings/errors.

# Node system

Have you used Godot game engine? If the answer is "yes" you're in luck, this engine uses a similar node system for game entities. We have a base class `Node` and derived nodes like `SpatialNode`, `MeshNode` and etc.

If you don't know what node system is or haven't used Godot, here is a small introduction to the node system:
> Generally game engines have some sort of ECS (Entity component system) in use. There are various forms of ECS like data-driven, object-oriented and etc. Entities can be represented in different ways in different engines: an entity might be a complex class like Unreal Engine's `Actor` that contains both data and logic or an entity might be just a number (a unique identifier). Components can also be represented in different ways: a component might be a special class that implements some specific logic like Unreal Engine's `UCharacterMovementComponent` that implements functionality for your entity to be able to move, swim, fly and etc. so it contains both data and logic or a component may just group some data and have no logic at all. Node system is an ECS-like system that is slightly similar to how Unreal Engine's ECS works. If you worked with Unreal Engine imagine that there are no actors but only components and world consists of only components - that's roughly how node system works. In node system each entity in the game is a node. A node contains both logic and data. The base class `Node` implements some general node functionality such as: being able to attach child nodes or attach to some parent node, determining if the node should receive user input or if it should be called every frame to do some per-frame logic and etc. We have various types of nodes like `SpatialNode` that has a location/rotation/scale in 3D space or `MeshNode` that derives from `SpatialNode` but adds functionality to display a 3D geometry. Nodes attach other nodes as child nodes thus creating a node hierarchy or a node tree. A node tree is a game level or a game map. Your game's character is also a node tree because it will most likely be built of a mesh node, a camera node, a collision node, maybe your custom derived node that handles some character specific logic and maybe something else. You game's UI is also a node tree because it will most likely have a container node, various button nodes and text nodes. When you combine everything together you attach your character's node tree and your UI node tree to your game map's node tree thus creating a game level. That's how node system works.

Nodes are generally used to take place in the node hierarchy, to be part of some parent node for example. Nodes are special and they use special garbage collected smart pointers (we will talk about them in one of the next sections) but not everything should be a node. If you want to implement some class that does not need to take a place in the node hierarchy, does not belong to some node or does not interact with nodes, for example a class to send bug reports, there's really no point in deriving you bug reporter class from `Node`, although nobody is stopping you from using nodes for everything and it's perfectly fine to do so, right now we want to smoothly transition to other important thing in your game. Your class/object might exist apart from node system and you can use it outside of the node system. For example, you can store your class/object in a `GameInstance`.

# Game instance

> If you used Unreal Engine the `GameInstance` class works similarly (but not exactly) to how Unreal Engine's `UGameInstance` class works.

In order to start your game you create a `Window`, a window creates `GameManager` - the heart of your game, the most important manager of your game, but generally you don't interact with `GameManager` directly since it's located in the `src/engine_lib/private` directory, the `GameManager` creates essential game objects such as renderer, physics engine, audio manager and your `GameInstance`. While the window that your have created is not closed the `GameInstance` lives and your game runs. Once the user closes your window or your game code submits a "close window" command the `GameManager` starts to destroy all created systems: `World` (which despawns and destroys all nodes), `GameInstance`, renderer and other things.

When `GameInstance` is created and everything is setup for the game to start `GameInstance::onGameStarted` is called. In this function you generally create/load a game world and spawn some nodes. Generally there is no need to store pointers to nodes in your `GameInstance` since nodes are self-contained and each node knows what to do except that nodes sometimes ask `GameInstance` for various global information by using a static function `Node::getGameInstance`.

So "global" (world independent) classes/objects are generally stored in `GameInstance` but sometimes they can be represented by a node.

# Reflection basics

## General

Have you used other game engines? Generally game engines have an editor application that allows placing your custom objects on the map and construct levels. The general process goes like this: you create a new class in the code, compile the code, open up the editor and the editor is able to see you newly created class and allows you to place objects of the class on the level/map. The ability to see/analyze your code is what reflection is about.

Generally reflection comes down to writing additional code to your class, this additional code allows other systems to see/analyze your class/objects (see functions/fields, even private).

Reflection helps us save time and ease up some processes. Let's consider an example: you want to have a save file for your game where you store player's progress. For this you can use a `ConfigManager` and save your variables like this:

```Cpp
ConfigManager manager;

// Save.
manager.setValue<std::string>("player progress", "player name", playerConfig.getName());
manager.setValue<int>("player progress", "player level", playerConfig.getLevel());

// Load.
auto sPlayerName = manager.getValue<std::string>("player progress", "player name", "default name");
auto iPlayerLevel = manager.setValue<int>("player progress", "player level", 1);
```

but each time you need to save a new variable in the save file you need to append new code to save/load functions. With reflection you don't need to care about this. Here is the same example as above but it uses reflection:

```Cpp
// PlayerConfig.cpp

#include "PlayerConfig.generated_impl.h"

// PlayerConfig.h ------------------------

#include "io/Serializable.h" // base class for types that use reflection to be able to
                             // save/load on disk
// ... your other includes here ...

#include "PlayerConfig.generated.h" // must be included last, contains generated reflection code

using namespace ne; // engine namespace, all engine types are located in this namespace

namespace mygame RNAMESPACE(){

/// Stores player save data.
class RCLASS(Guid("9ae433d9-2cba-497a-8061-26f2683b4f35")) PlayerConfig : public Serializable {
public:
    PlayerConfig() = default;
    virtual ~PlayerConfig() override = default;

    RPROPERTY(Serialize)
    std::string sPlayerName;

    RPROPERTY(Serialize)
    int iPlayerLevel = 1;

    mygame_PlayerConfig_GENERATED
};

}

File_PlayerConfig_GENERATED
```

now when you add a new variable you just need to mark it as `RPROPERTY(Serialize)` and that's it, it will be saved and loaded from save file on the disk. Thanks to reflection the engine is able to see fields marked as `RPROPERTY(Serialize)` and when you ask the engine to serialize an object of `PlayerConfig` type it will save all `RPROPERTY(Serialize)` fields to the save file.

We will talk about serialization (using various ways) in more details with more detailed examples in one of the next sections.

Note that reflection that we use relies on code generation, this means that when you create a class and add reflection specific code (include generated header, add `RPROPERTY` and etc.) your IDE will mark them as errors since the generated header is not created yet and thus macros like `RPROPERTY` are unknown. You need to ignore those errors, finish writing reflection specific code and compile your program. During the compilation, as one of the first build steps the reflection generator will analyze all source files in your project and see if some "generated" headers needs to be generated, then it generates them in `src/*yourtarget*/.generated` directory and after the reflection generator your compiler comes in and compiles your program, it will find no issues since the generated headers now exist and all included macros are known. After the compilation your IDE should also start to see new generated headers and will hide previously shown errors.

Let's analyze the example from above:

- `#include "PlayerConfig.generated.h"`
    - an include with name `*yourheadername*.generated.h` needs to be included as the last `#include` in your header, it contains reflection macros/code
    - as you may have noticed you also need to include `#include "*yourheadername*.generated_impl.h"` in your .cpp file
- `namespace mygame RNAMESPACE()`
    - here `RNAMESPACE` is a reflection macro (R stands for Reflected) that is used to mark a namespace to be added to reflection database
- `class RCLASS(Guid("9ae433d9-2cba-497a-8061-26f2683b4f35")) PlayerConfig`
    - here `RCLASS` is a reflection macro that is used to mark class to be added to reflection database, if you have a struct you need to use `RSTRUCT` macro
    - `Guid` is a property of `RCLASS` macro that is used to specify a unique GUID that the class should be associated with, for example when you serialize an object of this class into a file on the disk this GUID will be saved in the file to mark that `PlayerConfig` class is saved here, you can generate a GUID by simply searching something like "generate GUID" on the Internet, any GUID will be fine but if some type already uses this GUID the engine will let you know at startup, in debug builds the engine checks for GUID uniqueness
- `RPROPERTY(Serialize)`
    - `RPROPERTY` is a macro that marks a field to be added to reflection database as part of the class, you can have this macro defined without other properties but in this case we use a property named `Serialize` - this property tells the engine that when an object of this class is being serialized the value of this field should also be serialized, additionally when you deserialize a file that value will be deserialized into this field
- `mygame_PlayerConfig_GENERATED`
    - a macro `*namespace*_*typename*_GENERATED` should be specified in the end of your type for reflection to work correctly, if you use nested types/namespaces this macro name will contain them all: `*outerouter*_*outerinner*_*typename*_GENERATED`
- `File_PlayerConfig_GENERATED`
    - a macro `File_*headername*_GENERATED` should be specfiied in the end of your header file for reflection to work correctly

Note that in order to use reflection you don't need to derive from `Serializable` in this example we derive from `Serializable` for serialization/deserialization functionality. Also note that `Node` class derives from `Serializable`, this means that all `Node` and derived classes have serialization/deserialization functionality.

For more information about reflection generator see our reflection generator fork: https://github.com/Flone-dnb/Refureku

Documentation for the original reflection generator: https://github.com/jsoysouvanh/Refureku/wiki

## Using reflection for your new type

Let's sum up what you need to do in order to use reflection:

1. Include `*filename*.generated.h` as the last include in your .h file (where *filename* is file name without extension).
2. Include `*filename*.generated_impl.h`  as the last include in your .cpp file (where *filename*  is file name without extension).
3. If you have a namespace add a `RNAMESPACE()` macro, for example: `namespace mygame RNAMESPACE()`.
4. Mark your class/struct with `RCLASS`/`RSTRUCT` and add a `Guid` property with some random GUID, for example: `class RCLASS(Guid("9ae433d9-2cba-497a-8061-26f2683b4f35")) PlayerConfig`.
5. In the end of your type (as the last line of your type) add a macro `*namespace*_*typename*_GENERATED` or just `*typename*_GENERATED` if you don't have a namespace, if you use nested types/namespaces this macro name will contain them all: `*outerouter*_*outerinner*_*typename*_GENERATED`, for example: `mygame_PlayerConfig_GENERATED`.
6. In the end of your header file add a macro `File_*filename* _GENERATED` (where *filename*  is file name without extension), for example: `File_PlayerConfig_GENERATED`.

After this you can use reflection macros like `RPROPERTY`/`RFUNCTION`. We will talk about reflection macros/properties in more details in one of the next sections.

For more examples see `src/engine_tests/io/ReflectionTest.h`.

## Common mistakes with reflection

If you changed something in your header file and your IDE now shows errors in `GENERATED` macros just compile your project, it will run reflection generator and update reflection code, then the errors should disappear.

If you created/changes something in your header file related to reflection (renamed reflection macro, changed class name, etc.) and your compiler gives you and error about reflection files/macros, look at the build log and make sure that the reflection generator was actually run. Some IDEs like JetBrains Rider (maybe fixed) have an issue where they don't run pre-build steps of CMake targets (Qt Creator and Visual Studio 2022 were tested and they run reflection generator correctly). It's essential that reflection generator is run before the compiler, see the build log, when reflection generator is running you will see something like this in the build log:

```
[2/109 1.0/sec] engine_lib: running reflection generator...
[Info] Working Directory: ...\nameless-engine\ext\Refureku\build\Bin
[Info] [TOML] Load cppVersion: c++20
[Info] [TOML] Load shouldParseAllNamespaces: false
[Info] [TOML] Load shouldParseAllClasses: false
[Info] [TOML] Load compiler: clang++
[Info] [TOML] Load new project include directory: C:\Prog\ne\nameless-engine\ext\Refureku\build\Include
[Info] [TOML] Load new project include directory: C:\Prog\ne\nameless-engine\src\engine_lib\.generated
...
... // some output here
...
[Info] (Re)generated metadata for 0 file(s) in 0.005000 seconds.
[Info] Metadata of 1 file(s) up-to-date.
```

In some rare cases you need to manually delete generated reflection files. For example if you made a typo and wrote `RPROPERTY(Serializable)` (while the correct macro/property is `RPROPERTY(Serialize)`), started compiling your project, the reflection generator run but there is a compilation error related to the reflection code. In this case even if you rename your macro/property to be correct you still might not be able to compile your project, if this is the case, go to the directory with generated code at `src/*yourtarget*/.generated` find a generated file named after your header file and delete 2 files `*yourfile*.generated.h` and `*yourfile*.generated_impl.h` (where *yourfile* is the file in which you had a mistake) then try to compile your project, it should succeed.

You can delete `.generated` directory but before building your project you need to re-run CMake configuration so that CMake will create a new `.generated` directory with a fresh new config files inside of it.

# Error handling

The engine uses the `Error` class (`misc/Error.h`) to handle and propagate errors. Some engine functions return it and you should know how to use them.

The `Error` class stores 2 things:
  - initial error message (description of what went wrong)
  - "error call stack" (not an actual call stack, see below)

This class has some handy constructors, for example:
  - (most commonly used) you can construct an `Error` object from a message string, it will be used as "initial error message",
  - you can construct an `Error` object from a Windows `HRESULT` object, in such constructor the `Error` class will convert it to an error description, append the error code of the `HRESULT` object and use it as "initial error message",
  - you can construct an `Error` object from an error code returned by Windows `GetLastError` function, just like with `HRESULT`, it will be converted to an error description.

When you construct an `Error` object it will use `std::source_location` to capture the name of the source file it was constructed from, plus a line where this `Error` object was constructed.

After constructing an `Error` object you can use `Error::addEntry` to capture the name of the source file this function is called from, plus a line where this function was called from.

To get the string that contains the initial error message and all captured source "entries" you can use `Error::getFullErrorMessage`.

To show a message box with the full error message and additionally print it to the log you can use `Error::showError`.

Let's see how this works together in an example of window creation:

```Cpp
// inside of your main.cpp

using namespace ne;

auto result = Window::getBuilder()
                  .withTitle("My window")
                  .withMaximizedState(true)
                  .withIcon("res/game/mylogo.png")
                  .build(); // returns `std::variant<std::unique_ptr<Window>, Error>`
if (std::holds_alternative<Error>(result)) {
    // An error occurred while creating the window.
    Error error = std::get<Error>(std::move(result));
    error.addEntry(); // add this line to the error stack

    // Since here we won't propagate the error up (because we are in the `main.cpp` file) we show an error message.
    // If we want to propagate the error up we can return the error object after using the `Error::addEntry`.
    error.showError();

    // ... and throw an exception to show an unrecoverable error.
    throw std::runtime_error(error.getFullErrorMessage());
}

// Window was created successfully.
const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
```

# Logging

The engine has a simple `Logger` class (`io/Logger.h`) that you can use to write to log files and to console (note that logging to console is disabled in release builds).

On Windows log files are located at `%localappdata%/nameless-engine/*yourtargetname*/logs`.
On Linux log files are located at `~/.config/nameless-engine/*yourtargetname*/logs`.

Here is an example of `Logger` usage:

```Cpp
#include "misc/Logger.h"

void foo(){
    Logger::get().info("some information");
    Logger::get().warn("some warning");
    Logger::get().error("some error");
}
```

You can also combine `std::format` or `fmt::format` with logger:

```Cpp
#include "misc/Logger.h"
#include "fmt/core.h" // or #include <format> if your compiler supports it

void bar(){
    int iAnswer = 42;
    Logger::get().info(fmt::format("the value of the variable is {}", iAnswer));
}
```

Then your log file might look like this:

```
[16:14:49] [info] [MyFile.cpp, 31] some information
[16:14:49] [warning] [MyFile.cpp, 32] some warning
[16:14:49] [error] [MyFile.cpp, 33] some error
[16:14:49] [info] [MyFile.cpp, 46] the value of the variable is 42
```

As you can see each log entry has a file name and a line number included.

Looking at your logs and finding if there were any warnings/errors might be tiresome if you have a big game with lots of systems (even if you just use Ctrl+F), to instantly identify if your game had warnings/errors in the log when you close your game the last log entry will be summary of total warnings/errors logged (if there was any, if there was no warnings/errors nothing will be logged in the end), it might look like this:

```
[16:14:50] [info]
---------------------------------------------------
Total WARNINGS produced: 1.
Total ERRORS produced: 1.
---------------------------------------------------
```

You will see this in the console as the last printed text in debug builds and also in the end of your log files if there were any warnings/errors logged. So pay attention to the logs/console after closing your game.

# Project paths

In order for your game to be able to access files in the `res` directory when you build your project the engine creates symlinks to the `res` directory next to the build binaries of you game. This means that if you need to access some file from the `res` directory, in your app you can just type `"res/game/somepath"`. For release builds the engine will not create symlinks and will require you to copy your `res` directory manually but we will talk about this in more details in one of the next sections.

Instead of hardcoding you paths like `"res/game/somepath"` you should use `ProjectPaths` (`misc/ProjectPaths.h`). This class provides static functions to access various paths, here are a few examples:

```Cpp
// same as "res/game/myfile.png"
const auto absolutePathToMyFile = ProjectPaths::getPathToResDirectory(ResourceDirectory::GAME) / "myfile.png";

// same as "%localappdata%/nameless-engine/*targetname*/logs" on Windows
const auto pathToLogsDir = ProjectPaths::getPathToLogsDirectory();

// same as "%localappdata%/nameless-engine/*targetname*/progress" on Windows
const auto pathToLogsDir = ProjectPaths::getPathToPlayerProgressDirectory();
```

See `misc/ProjectPaths.h` for more.

# Memory leaks and garbage collection

## Memory leak checks

By default in Debug builds memory leak checks are enabled, look for the output/debugger output tab of your IDE after running your project. If any leaks occurred it should print about them. You can test whether the memory leak checks are enabled or not by doing something like this:
```Cpp
new int(42);
// don't `delete`
```
run your program that runs this code and after your program is finished you should see a message about the memory leak in the output/debugger output tab of your IDE.

## About raw pointers

Some engine functions return raw pointers. Generally, when the engine returns a raw pointer to you this means that you should not free/delete it and it is guaranteed to be valid for the (most) time of its usage. For more information read the documentation for the functions you are using.

## Garbage collector overview

The engine has a garbage collector: https://github.com/Flone-dnb/tgc2. The main reason why we need a garbage collector is to resolve cyclic references. The game has a node hierarchy that can change dynamically, nodes can reference other nodes that exist in the world and cyclic references could occur easily.

The garbage collector library provides a smart pointer `tgc2::gc<T>` that acts as a `std::shared_ptr<T>`, the library also has `gc_collector()->collect()` function that is used to resolve cyclic references and free objects that are stuck in cyclic references. By default the engine runs garbage collection regularly so you don't have to care about it (and you don't need to call `gc_collector()->collect()` from your game code). Here is an example on how to use those `gc` pointers:

```Cpp
const gc<MyNode> pNode = gc_new<MyNode>(); // it's like `std::make_shared<MyNode>()`
```

And here is a cyclic reference example that will be resolved by the garbage collector:

```Cpp
#include "misc/GC.hpp" // include for `gc` pointers

using namespace ne;
class Foo;
class Bar;

class Foo {
public:
    ~Foo() { Logger::get().info("~Foo()", ""); }
    gc<Bar> pBar;
};

class Bar {
public:
    ~Bar() { Logger::get().info("~Bar()", ""); }
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

Because the engine runs garbage collection regularly we want to minimize the amount of `gc` pointers to minimize the amount of work that the garbage collector will do. If we would have used only `gc` pointers and used them a lot, the garbage collection would probably cause stutters or freezes that players would not appreciate.

"When should I use `gc` pointers" you might ask? The answer is simple: start by thinking with `std::unique_ptr`s, if you need more than just one owner think of `std::shared_ptr`s for the part of the code you are planning/developing, if you know that cyclic reference can occur - use `gc` pointers, otherwise - don't, because there would be no need for that. For example, we use `gc` pointers in the node system because we know that nodes can reference other nodes and can dynamically change references so cyclic references could occur easily.

Of course, not everything will work magically. There is one or two cases where garbage collector can fail (leak memory), but don't worry, if this will happen the engine will let you know by logging an error message with possible reasons on why that happend so pay attention to the logs and always read the documentation for the functions you are using.

## Storing GC pointers in containers

Probably the most common mistake that could cause the garbage collection to fail is related to containers, because it's the most common mistake that developers could make, let's see how to avoid it. Imagine you want to store an array of `gc` pointers or store them in another container like `std::unordered_map`, you could write something like this:

```Cpp
std::vector<gc<MyNode>> vMyNodes; // DON'T DO THIS, this may leak
```

but due to garbage collector limitations this might cause memory leaks. Don't worry, when these leaks will happen the engine will log an error message with possible reasons on why that happend and possible solutions, so you will instantly know what to do. The right way to store `gc` pointers in a container will be the following:

```Cpp
gc_vector<MyNode> vMyNodes = gc_new_vector<MyNode>(); // you have to initialize it like this
                                                      // otherwise you will crash when using it
```

under the hood `gc_vector` is a `gc` pointer to the `std::vector` object, so you need to use it as a pointer (i.e. instead of `vMyNodes.push_back` use `vMyNodes->push_back`). Because it's a pointer you can copy this `gc_vector` easily.

Remember that you have to initialize `gc_vector`s correctly (like shown above), otherwise expect crashes in vector operations.

Not all STL containers have `gc` alternative, here is a list of supported STL wrappers for storing `gc` pointers:

- `gc_vector` for `std::vector`,
- `gc_map` for `std::map`,
- `gc_unordered_map` for `std::unordered_map`,
- `gc_deque` for `std::deque`,
- `gc_list` for `std::list`,
- `gc_set` for `std::set`,
- `gc_unordered_set` for `std::unordered_set`.
- (see garbage collector's GitHub page for up to date information)

## Capturing GC pointers in std::function

There is also an `std::function` wrapper called `gc_function` for some special cases where you want to capture `gc` pointers in it:

```Cpp
auto pObj = tgc2::gc_new<int>(0);
gc_function<void()> gc_callback = [pObj](){};
```

Otherwise there will be a leak:

```Cpp
class MyNode{
  std::function<void()> callback;
}

// somewhere else

gc<MyNode> pMyNode = ...;
pMyNode->callback = [pMyNode](){} // not OK, leaks
```

or crashes in GC code (if there was a garbage collection for captured in `std::function` `gc` pointers).

## Casting GC pointers

If you need to cast `gc` pointers there are also functions for that:

- use `gc_static_pointer_cast<To>(pFrom)` for `static_cast`,
- use `gc_dynamic_pointer_cast<To>(pFrom)` for `dynamic_cast`, example:

```Cpp
// note that `gc_dynamic_pointer_cast` takes `gc<T>&` and not `const gc<T>&`
gc<ParentClass> pParent = gc_new<ParentClass>();
gc<ChildClass> pChild = gc_dynamic_pointer_cast<ChildClass>(pParent); // OK because `pParent` is not `const`
// you have 2 GC pointers now
// ==========================================================
const gc<ParentClass> pParent = gc_new<ParentClass>();
gc<ChildClass> pChild = gc_dynamic_pointer_cast<ChildClass>(pParent); // compilation error because `pParent` is `const`
```

## Comparing GC pointers with pointer types

If you need to compare a raw pointer with a `gc` pointer, do the following:

```Cpp
if (&*pGcPointer == pRawPointer){
  // Both pointers are pointing to the same address.
}
```

## Examples of storing GC fields in various places

Here are some typical `gc` pointer use-cases where you might ask "is this OK?" and the answer is "yes":

```Cpp
class Collected {};

class Inner {
public:
    gc<Collected> pCollected;
};

class Outer {
public:
    Inner inner; // not wrapping into a `gc`
};

{
    Outer outer;
    outer.inner.pCollected = gc_new<Collected>();

    // `pCollected` is alive
} // goes out of scope but still alive, waiting for GC

// ... say the engine runs GC now (happens regularly, don't care about it) ...

// `pCollected` is freed now, everything run as expected
```

Another example:

```Cpp
class Collected {};

struct MyData {
    void allocate() { pCollected = gc_new<Collected>(); }

private:
    gc<Collected> pCollected;
};

{
    std::vector<MyData> vMyData; // intentionally not using `gc_vector` (because not storing `gc<MyData>`)

    constexpr size_t iDataSize = 10;
    for (size_t i = 0; i < iDataSize; i++) {
        MyData data;
        data.allocate();
        vMyData.push_back(std::move(data));
    }

    // Array objects are alive
} // go out of scope but still alive, waiting for GC

// ... say the engine runs GC now (happens regularly, don't care about it) ...

// all `gc` fields were freed now, everything run as expected
```

Moreover, storing gc pointers and gc vectors in `std::pair` should also be safe (since our nodes are doing that without any leaks).

You can find more examples in the file `src/engine_tests/misc/GC.cpp`.

## Interacting with garbage collector

`GameInstance` class has function to interact with the garbage collector, such as:

- `getGarbageCollectorRunIntervalInSec` to get the time interval after which the GC runs again.
- `setGarbageCollectorRunInterval` to set the time interval after which the GC runs again.
- and probably more, see the `GameInstance` class for more details...

Just note that you don't need to directly use `gc_collector()` (if you maybe saw this somewhere in examples), the engine will use it, you on the other hand should use `GameInstance` functions for garbage collection.

## Garbage collector limitations

Note that currently used version of garbage collection library does not support multiple inheritance, so the following example will produce a compilation error:

```Cpp
class Foo {};

class Bar {};

class Wow : public Foo, public Bar {};

gc<Wow> pWow = gc_new<Wow>();
gc<Foo> pFoo = gc_dynamic_pointer_cast<Foo>(pWow); // triggers a `static_assert` "multiple inheritance is not supported"
```

But that's not all, the following example also not going to work:

```Cpp
class SomeInterface {
public:
    virtual void foo() = 0;
};

class MyDerivedNode : public Node, public SomeInterface {
public:
    MyDerivedNode() = default;
    virtual ~MyDerivedNode() override = default;

    virtual void foo() override { sSomePrivateString = "It seems to work."; }

protected:
    std::string sSomePrivateString = "Hello!";
};

gc<SomeInterface> pMyInterface;
{
  auto pMyNode = gc_new<MyDerivedNode>();

  pMyInterface = pMyNode;
  // or
  pMyInterface = gc<SomeInterface>(dynamic_cast<SomeInterface>(&*pMyNode)); // most likely will throw exception
}

// `pMyInterface` is alive and holds some object, but...

// say the engine runs garbage collection now - it will crash during garbage collection process
```

Don't try to trick this system as it will probably lead to exceptions, leaks and crashes.

So if you're planning to use `gc` pointers on some type T, make sure this type T is not using multiple inheritance. Our reflection system can also have issues with multiple inheritance (in some special cases which we will talk about later), just note that.

# Deferred tasks and thread pool

The engine has 2 kind tasks you might be interested in:

- a `deferred task` is a function/lambda that is executed on the main thread,
    - you can submit deferred tasks from non-main thread to run some logic on the main thread,
    - you can also submit deferred tasks while being on the main thread, this will just queue your task and execute in slightly later on the main thread
    - deferred tasks are processed every time before a new frame is drawn

- a `thread pool task` is a function/lambda that is executed inside of a thread pool (non-main thread), you can submit a function/lambda to a thread pool to run asynchronous logic

The engine guarantees that:
    - all deferred tasks and all thread pool tasks will be finished before your `GameInstance` is destroyed so you don't need to check whether the game is closing or not in your deferred/thread pool tasks
    - all deferred tasks submitted from the main thread will be finished before garbage collector runs, this means that if you submit deferred tasks from the main thread you can pass raw node pointers (such as `Node*`) and use them in deferred tasks without risking to hit deleted memory

Submitting a deferred task from a `non-main thread` where in deferred task you operate on a `gc` controlled object such as Node can be dangerous as you may operate on a deleted (freed) memory. The engine roughly speaking does the following:

```Cpp
executeDeferredTasks();
runGarbageCollector();
```

and if you are submitting a deferred task from a non-main thread you might get into the following situation:

```Cpp
executeDeferredTasks();
// your non-main thread added a new deferred task here
runGarbageCollector();
// or your non-main thread added a new deferred task here
```

In this case use additional checks in the beginning of your deferred task to check if the node you want to use is still valid:

```Cpp
// We are on a non-main thread inside of a node:
addDeferredTask([this, iNodeId](){ // capturing `this` to use `Node` (self) functions, also capturing self ID
    // We are inside of a deferred task (on the main thread) and we don't know if the node (`this`)
    // was garbage collected or not because we submitted our task from a non-main thread.
    // REMEMBER: we can't capture `gc` pointers in `std::function`, this is not supported
    // and will cause memory leaks/crashes!

    const auto pGameManager = GameManager::get(); // using engine's private class `GameManager`

    // `pGameManager` is guaranteed to be valid inside of a deferred task.
    // Otherwise, if running this code outside of a deferred task you need to do 2 checks:
    // if (pGameManager == nullptr) return;
    // if (pGameManager->isBeingDestroyed()) return;

    if (!pGameManager->isNodeSpawned(iNodeId)){
        // Node was despawned and it may be dangerous to run the callback.
        return;
    }

    // Can safely interact with `this` (self) - we are on the main thread.
});
```

# World creation

## World axes and world units

The engine uses a left handed coordinate system. +X is world "forward" direction, +Y is world "right" direction and +Z is world "up" direction. These directions are stored in `Globals::WorldDirection` (`misc/Globals.h`).

Rotations are applied in the following order: ZYX, so "yaw" is applied first, then "pitch" and then "roll". If you need to do manual math with rotations you can use `MathHelpers::buildRotationMatrix` that builds a rotation matrix with correct rotation order.

1 world unit is expected to be equal to 1 meter in your game.

## Creating a world using C++

Let's first make sure you know how to create a window, your `main.cpp` should generally look like this:

```Cpp
// Standard.
#include <stdexcept>

// Custom.
#include "MyGameInstance.h"
#include "game/Window.h"
#include "misc/ProjectPaths.h"

int main() {
    using namespace ne;

    // Create a game window.
    auto result =
        Window::getBuilder()
            .withTitle("My Game")
            .withMaximizedState(true)
            .withIcon(
                ProjectPaths::getPathToResDirectory(ResourceDirectory::GAME) / "my_game_icon.png")
            .build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }
    auto pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));

    // Run game loop.
    pMainWindow->processEvents<MyGameInstance>();

    return 0;
}
```

And your game instance would generally look like this:

```Cpp
#pragma once

// Custom.
#include "game/GameInstance.h"

class Window;
class GameManager;
class InputManager;

class MyGameInstance : public GameInstance {
public:
    MyGameInstance(Window* pWindow, GameManager* pGameManager, InputManager* pInputManager);
    virtual ~MyGameInstance() override = default;

protected:
    virtual void onGameStarted() override;
};
```

Now let's see how we can create a world in `onGameStarted`:

```Cpp
#include "MyGameInstance.h"

// Custom.
#include "game/nodes/MeshNode.h"
#include "misc/PrimitiveMeshGenerator.h"

MyGameInstance::MyGameInstance(
    Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
    : GameInstance(pWindow, pGameManager, pInputManager) {}

void MyGameInstance::onGameStarted(){
    // Create world.
    createWorld([this](const std::optional<Error>& optionalWorldError) {
        // Show error if failed.
        if (optionalWorldError.has_value()) [[unlikely]] {
            auto error = optionalWorldError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Spawn sample mesh.
        const auto pMeshNode = gc_new<MeshNode>();
        const auto mtxMeshData = pMeshNode->getMeshData();
        {
            std::scoped_lock guard(*mtxMeshData.first);
            (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
        }
        getWorldRootNode()->addChildNode(pMeshNode);

        // Set mesh node location.
        pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));
    });
}
```

The code from above creates a new world with just 2 nodes: a root node and a mesh node. As you can see you specify a callback function that will be called once the world is created.

Unfortunatelly you would also need a camera to see your world but we will discuss this in the next section, for now let's talk about world creation.

If you instead want to load some level/map as you new world you need to use `loadNodeTreeAsWorld` instead of `createWorld`, see an example:

```Cpp
#include "MyGameInstance.h"

// Custom.
#include "game/nodes/MeshNode.h"
#include "misc/PrimitiveMeshGenerator.h"

MyGameInstance::MyGameInstance(
    Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
    : GameInstance(pWindow, pGameManager, pInputManager) {}

void MyGameInstance::onGameStarted(){
    // Prepare path to your node tree to load.
    const auto pathToMyLevel = ProjectPaths::getPathToResDirectory(ResourceDirectory::GAME) / "mylevel.toml";

    // Deserialize world.
    loadNodeTreeAsWorld(
        [this](const std::optional<Error>& optionalWorldError) {
            // Show error if failed.
            if (optionalWorldError.has_value()) [[unlikely]] {
                auto error = optionalWorldError.value();
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            
            // World is loaded.
        },
        pathToMyLevel);
}
```

After your world is created you can create your player node with a camera.

# Creating a first person character node

## Creating a character node
