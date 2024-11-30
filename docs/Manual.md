# Manual

This is a manual - a step-by-step guide to introduce you to various aspects of the engine. More specific documentation can be always found in the class/function/variable documentation in the source code or on this page (see left panel for navigation), every code entity is documented so you should not get lost.

Note that this manual contains many sections and instead of scrolling this page you can click on a little arrow button in the left (navigation) panel, the little arrow button becomes visible once you hover your mouse cursor over a section name, by clicking on that litte arrow you can expand the section and see its subsections and quickly jump to needed sections.

This manual expects that you have a solid knowledge in writing programs using C++ language.

# Introduction to the engine

## Prerequisites

First of all, read the repository's `README.md` file, specifically the `Prerequisites` section, make sure you have all required things installed.

The engine relies on CMake as its build system. CMake is a very common build system for C++ projects so generally most of the developers should be familiar with it. If you don't know what CMake is or haven't used it before it's about the time to learn the basics of CMake right now. Your project will be represented as several CMake targets: one target for standalone game, one target for tests, one target for editor and etc. So you will work on `CMakeLists.txt` files as with usual C++! There are of course some litte differences compared to the usual CMake usage, for example if you want to define external dependencing in your CMake file you need to do one additional step (compared to the usual CMake usage) that will be covered later in a separate section.

## Creating a new project

### Preparing project path (only for Windows users)

Because Windows limits paths to ~255 characters it's impossible to operate on files that have long paths, thus, you need to make sure that path to your project directory will not be very long, this does not mean that you need to create your project in disk root (for ex. C:\\myproject) - there's no need for that but the shorter the path to the project the most likely you won't have weird issues with your project. If you need exact numbers a path with 30-40 characters will be good but you can try using longer paths. Just don't forget about this path limitation thing.

### Project generator

Currently we don't have a proper project generator but it's planned. Once the project generator will be implemented this section will be updated.

Right now you can clone the repository and don't forget to update submodules. Once you have a project with `CMakeLists.txt` file in the root directory you can open it in your IDE. For example Qt Creator or Visual Studio allow opening `CMakeLists.txt` files as C++ projects, other IDEs also might have this functionality.

Note for Visual Studio users:
> If you use Visual Studio the proper way to work on `CMakeLists.txt` files as C++ projects is to open up Visual Studio without any code then press `File` -> `Open` -> `Cmake` and select the `CMakeLists.txt` file in the root directory (may be changed in the new VS versions). Then a tab called `CMake Overview Pages` should be opened in which you might want to click `Open CMake Settings Editor` and inside of that change `Build root` to just `${projectDir}\build\` to store built data inside of the `build` directory (because by default Visual Studio stores build in an unusual path `out/<build mode>/`). When you open `CMakeLists.txt` in Visual Studio near the green button to run your project you might see a text `Select Startup Item...`, you should press a litte arrow near this text to expand a list of available targets to use. Then select a target that you want to build/use and that's it, you are ready to work on the project.

Note for Windows users:
> Windows users need to run their IDE with admin privileges when building the project for the first time (only for the first build) when executing a post-build script the engine creates a symlink next to the built executable that points to the directory with engine/editor/game resources (called `res`). Creating symlinks on Windows requires admin privileges. When releasing your project we expect you to put an actual copy of your `res` directory next to the built executable but we will discuss this topic later in a separate section.

Before you go ahead and dive into the engine yourself make sure to read a few more sections, there is one really important section further in the manual that you have to read, it contains general tips about things you need to keep an eye out!

### Project structure

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

Note that generally you should add all functionality of your game into a library target (so that it can be used in other executable targets such as `tests`) while executable targets will generally will only have `main.cpp`.

Your game target `CMakeLists.txt` is fully yours, it will have some configuration code inside of it but you are free to change it as you want (you can disable/change `doxygen`, `clang-tidy`, reflection generation and anything else you don't like).

## Which header files to include and which not to include

### General

`src/engine_lib` is divided into 2 directories: public and private. You are free to include anything from the `public` directory, you can also include header files from the `private` directory in some special/advanced cases but generally there should be no need for that. Note that this division (public/private) is only conceptual, your project already includes both directories because some engine `public` headers use `private` headers and thus both included in cmake targets that use the engine (in some cases it may cause your IDE to suggest lots of headers when attempting to include some header from some directory so it may be helpful to just look at the specific public directory to look for the header name if you feel overwhelmed).

Inside of the `public` directory you will see other directories that group header files by their purpose, for example `io` directory stands for `in/out` which means that this directory contains files for working with disk (loading/saving configuration files, logging information to log files that are stored on disk, etc.).

You might also notice that some header files have `.h` extension and some have `.hpp` extension. The difference here is only conceptual: files with `.hpp` extension don't have according `.cpp` file, this is just a small hint for developers.

You are not required to use the `private`/`public` directory convention, moreover directories that store executable cmake targets just use `src` directory (you can look into `engine_tests` or `editor` directories to see an example) so you can group your source files as you want.

### Math headers

The engine uses GLM (a well known math library, hosted at https://github.com/g-truc/glm). Although you can include original GLM headers it's highly recommended to include the header `math/GLMath.hpp` instead of the original GLM headers when math is needed. This header includes main GLM headers and defines engine specific macros so that GLM will behave as the engine expects.

You should always prefer to include `math/GLMath.hpp` instead of the original GLM headers and only if this header does not have needed functionality you can include original GLM headers afterwards.

## Automatic code formatters and static analyzers

The engine uses `clang-format` and `clang-tidy` a classic pair of tools that you will commonly find in C++ projects. If you don't know what they do it's a good time to read about them on the Internet.

The engine does not require you to use them but their usage is highly recommended.

`clang-format` can be used in your IDE to automatically format your code (for example) each time you press Ctrl+S. If you want to make sure that your IDE is using our `.clang-format` config you can do the following check: in your source code create 2 or more consecutive empty lines, since our `.clang-format` config contains a rule `MaxEmptyLinesToKeep: 1` after you format the file only 1 empty line should remain. The action with which you format your source code depends on your IDE settings that you might want to configure, generally IDEs have a shortcut to "format" your source code but some have option to automatically use "format" action when you are saving your file.

`clang-tidy` has a lot of checks enabled and is generally not that fast as you might expect, because of this we have `clang-tidy` enabled only in release builds to speed up build times for debug builds. This means that if your game builds in debug mode it may very well fail to build in release mode due to some `clang-tidy` warnings that are treated as errors. Because of this it's highly recommended to regularly (say once or twice a week) build your project in release mode to check for `clang-tidy` warnings/errors.

## Node system

Have you used Godot game engine? If the answer is "yes" you're in luck, this engine uses a similar node system for game entities. We have a base class `Node` and derived nodes like `SpatialNode`, `MeshNode` and etc.

If you don't know what node system is or haven't used Godot, here is a small introduction to the node system:
> Generally game engines have some sort of ECS (Entity component system) in use. There are various forms of ECS like data-driven, object-oriented and etc. Entities can be represented in different ways in different engines: an entity might be a complex class like Unreal Engine's `Actor` that contains both data and logic or an entity might be just a number (a unique identifier). Components can also be represented in different ways: a component might be a special class that implements some specific logic like Unreal Engine's `CharacterMovementComponent` that implements functionality for your entity to be able to move, swim, fly and etc. so it contains both data and logic or a component may just group some data and have no logic at all. Node system is an ECS-like system that is slightly similar to how Unreal Engine's entity-component framework works. If you worked with Unreal Engine imagine that there are no actors but only components and world consists of only components - that's roughly how node system works. In node system each entity in the game is a node. A node contains both logic and data. The base class `Node` implements some general node functionality such as: being able to attach child nodes or attach to some parent node, determining if the node should receive user input or if it should be called every frame to do some per-frame logic and etc. We have various types of nodes like `SpatialNode` that has a location/rotation/scale in 3D space or `MeshNode` that derives from `SpatialNode` but adds functionality to display a 3D geometry. Nodes attach other nodes as child nodes thus creating a node hierarchy or a node tree. A node tree is a game level or a game map. Your game's character is also a node tree because it will most likely be built of a mesh node, a camera node, a collision node, maybe your custom derived node that handles some character specific logic and maybe something else. Your game's UI is also a node tree because it will most likely have a container node, various button nodes and text nodes. When you combine everything together you attach your character's node tree and your UI node tree to your game map's node tree thus creating a game level. That's how node system works.

Nodes are generally used to take place in the node hierarchy, to be part of some parent node for example. Nodes are special and they use special garbage collected smart pointers (we will talk about them in one of the next sections) but not everything should be a node. If you want to implement some class that does not need to take a place in the node hierarchy, does not belong to some node or does not interact with nodes, for example a class to send bug reports, there's really no point in deriving you bug reporter class from `Node`, although nobody is stopping you from using nodes for everything and it's perfectly fine to do so, right now we want to smoothly transition to other important thing in your game. Your class/object might exist apart from node system and you can use it outside of the node system. For example, you can store your class/object in a `GameInstance`.

## Game instance

> If you used Unreal Engine the `GameInstance` class works similarly (but not exactly) to how Unreal Engine's `UGameInstance` class works.

In order to start your game you create a `Window`, a window creates `GameManager` - the heart of your game, the most important manager of your game, but generally you don't interact with `GameManager` directly since it's located in the `src/engine_lib/private` directory, the `GameManager` creates essential game objects such as renderer, physics engine, audio manager and your `GameInstance`. While the window that your have created is not closed the `GameInstance` lives and your game runs. Once the user closes your window or your game code submits a "close window" command the `GameManager` starts to destroy all created systems: `World` (which despawns and destroys all nodes), `GameInstance`, renderer and other things.

When `GameInstance` is created and everything is setup for the game to start `GameInstance::onGameStarted` is called. In this function you generally create/load a game world and spawn some nodes. Generally there is no need to store pointers to nodes in your `GameInstance` since nodes are self-contained and each node knows what to do except that nodes sometimes ask `GameInstance` for various global information by using the static function `Node::getGameInstance`.

So "global" (world independent) classes/objects are generally stored in `GameInstance` but sometimes they can be represented by a node.

## Reflection basics

### General

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

### Using reflection for your new type

Let's sum up what you need to do in order to use reflection:

1. Include `*filename*.generated.h` as the last include in your .h file (where *filename* is file name without extension).
2. Include `*filename*.generated_impl.h`  as the last include in your .cpp file (where *filename*  is file name without extension).
3. If you have a namespace add a `RNAMESPACE()` macro, for example: `namespace mygame RNAMESPACE()`.
4. Mark your class/struct with `RCLASS`/`RSTRUCT`, for example: `class RCLASS() PlayerConfig`.
5. In the end of your type (as the last line of your type) add a macro `*namespace*_*typename*_GENERATED` or just `*typename*_GENERATED` if you don't have a namespace, if you use nested types/namespaces this macro name will contain them all: `*outerouter*_*outerinner*_*typename*_GENERATED`, for example: `mygame_PlayerConfig_GENERATED`.
6. In the end of your header file add a macro `File_*filename* _GENERATED` (where *filename*  is file name without extension), for example: `File_PlayerConfig_GENERATED`.

After this you can use reflection macros like `RPROPERTY`/`RFUNCTION`. We will talk about reflection macros/properties in more details in one of the next sections.

Note
> Steps from above describe basic reflection usage and generally this will not be enough for engine-related things such as serialization and some editor features. Other sections of this manual will describe additional steps that you need to apply to your type in order to use various engine-related features that use reflection.

For more examples see `src/engine_tests/io/ReflectionTest.h`.

### Common mistakes when using reflection

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

If you get a compilation error like `Cannot open include file: '....generated.h': No such file or directory` this might either mean that the reflection generator was not run (see solution from the above) or that your CMake target does not have reflection generator enabled. Open your `CMakeLists.txt` file and make sure it has `add_refureku` command if there is no such command then reflection generator is not enabled and you might want to look at `CMakeLists.txt` of other targets to see how they use this command (not all CMake targets use reflection generator, generally only library targets use it).

If you added the reflection code but your project fails to compile at the linking stage make sure your `.cpp` file includes `*filename*.generated_impl.h` as its last include file.

In some rare cases you need to manually delete generated reflection files. For example if you made a typo and wrote `RPROPERTY(Serializable)` (while the correct macro/property is `RPROPERTY(Serialize)`), started compiling your project, the reflection generator run but there is a compilation error related to the reflection code. In this case even if you rename your macro/property to be correct you still might not be able to compile your project, if this is the case, go to the directory with generated code at `src/*yourtarget*/.generated` find a generated file named after your header file and delete 2 files `*yourfile*.generated.h` and `*yourfile*.generated_impl.h` (where *yourfile* is the file in which you had a mistake) then try to compile your project, it should succeed.

You can delete `.generated` directory but before building your project you need to re-run CMake configuration so that CMake will create a new `.generated` directory with a fresh new config files inside of it.

## Error handling

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

## Logging

The engine has a simple `Logger` class (`io/Logger.h`) that you can use to write to log files and to console (note that logging to console is disabled in release builds).

On Windows log files are located at `%%localappdata%/nameless-engine/*yourtargetname*/logs`.
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

You can also combine `std::format` with logger:

```Cpp
#include "misc/Logger.h"
#include <format>

void bar(){
    int iAnswer = 42;
    Logger::get().info(std::format("the value of the variable is {}", iAnswer));
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

You will see this message in the console and in the log files as the last message if there were any warnings and/or errors logged. So pay attention to the logs/console after closing your game.

## Project paths

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

## Memory leaks and garbage collection

### Memory leak checks

By default in Debug builds memory leak checks are enabled, look for the output/debugger output tab of your IDE after running your project. If any leaks occurred it should print about them. You can test whether the memory leak checks are enabled or not by doing something like this:
```Cpp
new int(42);
// don't `delete`
```
run your program that runs this code and after your program is finished you should see a message about the memory leak in the output/debugger output tab of your IDE.

### About raw pointers

Some engine functions return raw pointers. Generally, when the engine returns a raw pointer to you this means that you should not free/delete it and it is guaranteed to be valid for the (most) time of its usage. For more information read the documentation for the functions you are using.

### Garbage collector overview

The engine uses the following garbage collector: https://github.com/Flone-dnb/sgc. The main reason why we need a garbage collector is to resolve cyclic references. The game has a node hierarchy that can change dynamically, nodes can reference other nodes that exist in the world and cyclic references could occur easily.

The garbage collector library provides a smart pointer `sgc::GcPtr<T>` that acts as a `std::shared_ptr<T>`, the library also has `sgc::GarbageCollector::get().collectGarbage()` function that is used to resolve cyclic references and free objects that are stuck in cyclic references. By default the engine runs garbage collection regularly so you don't have to care about it (and you don't need to call `sgc::GarbageCollector::get().collectGarbage()` from your game code). Here is an example on how to use these `sgc::GcPtr` objects:

```Cpp
sgc::GcPtr<MyNode> pNode = sgc::makeGc<MyNode>(); // it's like `std::make_shared<MyNode>()`
```

And here is a cyclic reference example that will be resolved by the garbage collector:

```Cpp
#include "GcPtr.h" // include for `GC` pointers

class Foo{
public:
    sgc::GcPtr<Foo> pFoo;
};

{
    auto pFoo = sgc::makeGc<Foo>();
    pFoo->pFoo = pFoo; // cyclic reference
}

// `Foo` still exists and was not freed, waiting for GC...
sgc::GarbageCollector::get().collectGarbage(); // this will be run regularly somewhere in the engine code
                                               // so you don't have to care about it
// `Foo` was freed
```

Because the engine runs garbage collection regularly we want to minimize the amount of `gc` pointers to minimize the amount of work that the garbage collector will do. If we would have used only `gc` pointers and used them a lot, the garbage collection would probably cause stutters or freezes that players would not appreciate.

"When should I use `gc` pointers" you might ask? The answer is simple: start by thinking with `std::unique_ptr`s, if you need more than just one owner think of `std::shared_ptr`s for the part of the code you are planning/developing, if you know that cyclic reference can occur - use `gc` pointers, otherwise - don't, because there would be no need for that. For example, we use `gc` pointers in the node system because we know that nodes can reference other nodes and can dynamically change references so cyclic references could occur easily.

Of course, not everything will work magically. There is one or two cases where garbage collector can fail (leak memory), but don't worry, if this will happen the engine will let you know by logging an error message with possible reasons on why that happend so pay attention to the logs and always read the documentation for the functions you are using.

### Storing GC pointers in containers

Probably the most common mistake that could cause the garbage collection to fail is related to containers, because it's the most common mistake that developers could make, let's see how to avoid it. Imagine you want to store an array of `gc` pointers or store them in another container like `std::unordered_map`, you could write something like this:

```Cpp
std::vector<sgc::GcPtr<MyNode>> vMyNodes; // DON'T DO THIS, this may leak
```

but due to garbage collector limitations this might cause memory leaks. Don't worry, when these leaks will happen the engine will log an error message with possible reasons on why that happend and possible solutions, so you will instantly know what to do. The right way to store `gc` pointers in a container will be the following:

```Cpp
#include "gccontainers/GcVector.hpp"

sgc::GcVector<sgc::GcPtr<MyNode>> vMyNodes; // works just like `std::vector<T>`
```

Not all STL containers have `gc` alternative, see the garbage collector's repository for more information.

### GC limitations and thread safety with examples

It's really important that you know how to properly use the GC objects.

Use the link to the garbage collector's repository (see above) and make sure to read the `README` file (especially the limitations and thread safety sections).

Here's what you don't need to care about in your game's code:

- No need to set GC info callbacks because the engine does that for you.
- No need to directly interact with `sgc::GarbageCollector` because the engine runs garbage collector regularly and in some special situations.
- No need to add GC to your game's cmake file because it's already included in the engine's cmake target.

### Interacting with garbage collector

`GameInstance` class has function to interact with the garbage collector, such as:

- `getGarbageCollectorRunIntervalInSec` to get the time interval after which the GC runs again.
- `setGarbageCollectorRunInterval` to set the time interval after which the GC runs again.
- and probably more, see the `GameInstance` class for more details...

Just note that you don't need to directly use `sgc::GarbageCollector` (if you maybe saw this somewhere in examples), the engine will use it, you on the other hand should use `GameInstance` functions for garbage collection.

## Deferred tasks and thread pool

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

## World creation

### World axes and world units

The engine uses a left handed coordinate system. +X is world "forward" direction, +Y is world "right" direction and +Z is world "up" direction. These directions are stored in `Globals::WorldDirection` (`misc/Globals.h`).

Rotations are applied in the following order: ZYX, so "yaw" is applied first, then "pitch" and then "roll". If you need to do manual math with rotations you can use `MathHelpers::buildRotationMatrix` that builds a rotation matrix with correct rotation order.

1 world unit is expected to be equal to 1 meter in your game.

### Creating a world using C++

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
#include "game/node/MeshNode.h"
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
        const auto pMeshNode = sgc::makeGc<MeshNode>();
        pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
        getWorldRootNode()->addChildNode(pMeshNode);

        // Set mesh node location.
        pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));
    });
}
```

The code from above creates a new world with just 2 nodes: a root node and a mesh node. As you can see you specify a callback function that will be called once the world is created.

Unfortunatelly you would also need a camera to see your world but we will discuss this in the next section, for now let's talk about world creation.

If you instead want to load some level/map as your new world you need to use `loadNodeTreeAsWorld` instead of `createWorld`, see an example:

```Cpp
#include "MyGameInstance.h"

// Custom.
#include "game/node/MeshNode.h"
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

## Lighting

Light source nodes (such as point/spot/directional) are stored in `game/node/light`. Just spawn one of these nodes and configure their setting using `setLight...` functions and `setWorldLocation`/`setWorldRotation`. Here is an example:

```Cpp
#include "game/node/light/PointLightNode.h"

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
        const auto pMeshNode = sgc::makeGc<MeshNode>();
        pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
        getWorldRootNode()->addChildNode(pMeshNode);

        // Set mesh node location.
        pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

        // Spawn point light.
        const auto pPointLightNode = sgc::makeGc<PointLightNode>();
        getWorldRootNode()->addChildNode(pPointLightNode);

        // Configure point light.
        pPointLightNode->setLightColor(glm::vec3(0.0F, 1.0F, 0.0F));      // emit green light
        pPointLightNode->setLightHalfDistance(20.0F);                     // change light radius
        pPointLightNode->setWorldLocation(glm::vec3(-1.0F, 5.0F, 5.0F));  // move the light
    });
}
```

## Environment

Using `EnvironmentNode` you can configure environment settings such as ambient light, skybox, etc. Here is an example:

```Cpp
#include "game/node/EnvironmentNode.h"

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
        const auto pMeshNode = sgc::makeGc<MeshNode>();
        pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
        getWorldRootNode()->addChildNode(pMeshNode);

        // Set mesh node location.
        pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

        // Spawn environment node.
        const auto pEnvironmentNode = sgc::makeGc<EnvironmentNode>();
        getWorldRootNode()->addChildNode(pEnvironmentNode);

        // Configure environment settings.
        pEnvironmentNode->setAmbientLight(glm::vec3(0.1F, 0.1F, 0.1F)); // add small ambient lighting
    });
}
```

Note that your world can only have 1 active environment node. If you will spawn another environment node you will get a warning in the logs that will tell you that settings of your newly spawned environment node will be ignored.

## Creating a character and working with input

### Creating a simple flying character node

In this section we will start with the most simplest character node: a node that has a camera and can fly around using WASD and mouse movement.

Let's create new files `FlyingCharacter.h` and `FlyingCharacter.cpp` inside of our new directory named `private/node` to our library target (we pick our library target since generally library targets will contain all functionality (so that it can be used in multiple executable targets such as `tests`) while executable targets generally will only contain `main.cpp`). Open `CMakeLists.txt` of your library target and add new files to `PROJECT_SOURCES`:

```
## `CMakeLists.txt`
## ... some code here ...

## Specify project sources.
set(PROJECT_SOURCES
    ## ... some files here ...
    private/node/FlyingCharacter.h
    private/node/FlyingCharacter.cpp
    ## add your .h/.cpp files here
)

## Define target.
add_library(${PROJECT_NAME} STATIC ${PROJECT_SOURCES})

## ... some code here ...
```

Now make sure CMake configuration was run, for example, Qt Creator runs CMake configuration after your make modifications to a `CMakeLists.txt` file and save it.

Now let's create our character node. We have two choises: we can derive our flying character node from `CameraNode` (it derives from `SpatialNode` so it has a location/rotation/scale in 3D world) or derive from `SpatialNode` and attach a `CameraNode` to it. Since more complicated characters will require multiple nodes such as: collision node, mesh node, camera node that will be somehow attached we will derive from `SpatialNode` and demonstrate how you can attach a `CameraNode`.

We will start with the most basic node header file:

```Cpp
// FlyingCharacter.h

#pragma once

// Custom.
#include "game/node/SpatialNode.h"

// Using engine namespace.
using namespace ne;

class FlyingCharacterNode : public SpatialNode {
public:
    FlyingCharacterNode();
    FlyingCharacterNode(const std::string& sNodeName);

    virtual ~FlyingCharacterNode() override = default;
};
```

As you can see we created 2 constructors, engine's node classes have 2 constructors (1 without a name and 1 with a name) so we will also define 2 constructors just to be consistent with the engine (although you are not required to do this).

Now the .cpp file:

```Cpp
// FlyingCharacter.cpp

#include "FlyingCharacter.h"

FlyingCharacterNode::FlyingCharacterNode() : FlyingCharacterNode("Flying Character Node") {}

FlyingCharacterNode::FlyingCharacterNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
    // constructor logic
}

```

Engine's nodes do the same thing as we did here: their constructor without parameters is delegating constructor that calls constructor with a name and provides a default node name (and does nothing). The actual construction logic happens in the constructor that accepts a node name.

Now let's add reflection to our node so that the editor will be able to recognize our node and the engine will be able to save/load our node when we are using it as part of some node tree.

```Cpp
// FlyingCharacter.h

#pragma once

// Custom.
#include "game/node/SpatialNode.h"

#include "FlyingCharacter.generated.h"

using namespace ne;

class RCLASS(Guid("0f84de3a-b5bb-42c7-aeea-aef89d31ed63")) FlyingCharacterNode : public SpatialNode {
public:
    FlyingCharacterNode();
    FlyingCharacterNode(const std::string& sNodeName);

    virtual ~FlyingCharacterNode() override = default;

    FlyingCharacterNode_GENERATED
};

File_FlyingCharacter_GENERATED
```

Note
> `FlyingCharacterNode_GENERATED` and `File_FlyingCharacter_GENERATED` are different, one defines a `Node` class and the other defines a file name.

We followed instructions from one of the previous sections in the manual where we discussed reflection. Also don't forget to update our `.cpp` file:

```Cpp
// FlyingCharacter.cpp

#include "FlyingCharacter.h"

#include "FlyingCharacter.generated_impl.h" // <- new include, should be last

FlyingCharacterNode::FlyingCharacterNode() : FlyingCharacterNode("Flying Character Node") {}

FlyingCharacterNode::FlyingCharacterNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
    // constructor logic
}
```

As we said earlier your IDE might highlight new code as errors since the reflection headers were not generated yet. It's compile our project so that reflection headers will be generated.

Node
> If your compilation fails at `Cannot open include file: 'FlyingCharacter.generated.h': No such file or directory` this might indicate that either the reflection generator was not run or your CMake target does not have reflection generator enabled - see one of the previous sections about reflection for possible solutions.


Now before moving to handling user input let's add a camera node. There are several ways on how we can do this and we will discuss various ways in one of the following sections, for now let's just create a camera node in our character's constructor. Let's create a new field in our class to store a `gc` pointer to the camera node:

```Cpp
// FlyingCharacter.h

#pragma once

// Custom.
#include "game/node/SpatialNode.h"

#include "FlyingCharacter.generated.h"

using namespace ne;

namespace ne {
    class CameraNode;
}

class RCLASS(Guid("0f84de3a-b5bb-42c7-aeea-aef89d31ed63")) FlyingCharacterNode : public SpatialNode {
public:
    FlyingCharacterNode();

    FlyingCharacterNode(const std::string& sNodeName);

    virtual ~FlyingCharacterNode() override = default;

private:
    sgc::GcPtr<CameraNode> pCameraNode;

    FlyingCharacterNode_GENERATED
};

File_FlyingCharacter_GENERATED
```

Note
> We also added forward declaration of the camera node type `class CameraNode;`.

Since nodes only use `gc` pointers we use it instead of some other smart pointer types.

Now let's create a camera node in our constructor:

```Cpp
#include "FlyingCharacter.h"

// Custom.
#include "game/node/CameraNode.h"

#include "FlyingCharacter.generated_impl.h"

FlyingCharacterNode::FlyingCharacterNode() : FlyingCharacterNode("Flying Character Node") {}

FlyingCharacterNode::FlyingCharacterNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
    // Create our camera node.
    pCameraNode = sgc::makeGc<CameraNode>("Player Camera");

    // Attach the camera to the character.
    addChildNode(pCameraNode, AttachmentRule::KEEP_RELATIVE, AttachmentRule::KEEP_RELATIVE);
}
```

Now let's compile our program and make sure we have a world, in our `GameInstance::onGameStarted` create an empty world:

```Cpp
// MyGameInstance.cpp
#include "MyGameInstance.h"

#include "game/node/MeshNode.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "nodes/FlyingCharacter.h"

void MyGameInstance::onGameStarted() {
    // Create world.
    createWorld([this](const std::optional<Error>& optionalWorldError) {
        // Check if there was an error.
        if (optionalWorldError.has_value()) [[unlikely]] {
            auto error = optionalWorldError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Prepare a sample mesh.
        const auto pMeshNode = sgc::makeGc<MeshNode>();
        pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));

        // Spawn mesh node.
        getWorldRootNode()->addChildNode(pMeshNode);
        pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

        // Spawn character node.
        const auto pCharacter = sgc::makeGc<FlyingCharacterNode>();
        getWorldRootNode()->addChildNode(pCharacter);
        pCharacter->setWorldLocation(glm::vec3(-1.0F, 0.0F, 0.0F));
    });
}
```

If you would compile and run your project now you still wouldn't be able to see anything. This is because we need to explicitly make character's camera active.

The documentation for `CameraNode::makeActive` says that "Only spawned camera nodes can be primary (active), otherwise an error will be shown." let's then override `Node::onSpawning` in our character. Add the following code to our character's header file:

```Cpp
protected:
    virtual void onSpawning() override;
```

The documentation for `Node::onSpawning` says "This node will be marked as spawned before this function is called." this means that when `Node::onSpawning` is called our node is already considered as spawned. Also the documentation says "If overriding you must call the parent's version of this function first (before executing your login) to execute parent's logic." so let's implement this function. In the `.cpp` file of our character add the following code:

```Cpp
void FlyingCharacterNode::onSpawning() {
    SpatialNode::onSpawning(); // call super class

    pCameraNode->makeActive(); // make camera active
}
```

If you would compile and run your project now you will get an error saying "camera node "Player Camera" needs to be spawned in order to make it the active camera". If we would look at the documentation for `Node::onSpawning` again, we will see that it also says "This function is called before any of the child nodes are spawned. If you need to do some logic after child nodes are spawned use @ref onChildNodesSpawned." and since the camera node is a child node of our character when character node is spawning its child nodes are still waiting to be spawned. Thus let's use `onChildNodesSpawned` instead.

Replace `onSpawning` function with `onChildNodesSpawned`:

```Cpp
void FlyingCharacterNode::onChildNodesSpawned() {
    SpatialNode::onChildNodesSpawned(); // also change super call

    pCameraNode->makeActive();
}
```

Now compile and run your project. There should be no errors but still black screen. This is because we forgot about the lighting. Let's create the Sun for our world. In our game instance add code to spawn a directional light:

```Cpp
// MyGameInstance.cpp
#include "MyGameInstance.h"

#include "game/node/MeshNode.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "nodes/FlyingCharacter.h"
#include "game/node/light/DirectionalLightNode.h" // <- new include
#include "math/MathHelpers.hpp"                    // <- new include

void MyGameInstance::onGameStarted() {
    // Create world.
    createWorld([this](const std::optional<Error>& optionalWorldError) {
        // ... some code here ...

        // Spawn character node.
        const auto pCharacter = sgc::makeGc<FlyingCharacterNode>();
        getWorldRootNode()->addChildNode(pCharacter);
        pCharacter->setWorldLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

        // Spawn directional light.
        const auto pDirectionalLightNode = sgc::makeGc<DirectionalLightNode>();
        pDirectionalLightNode->setWorldRotation(
            MathHelpers::convertDirectionToRollPitchYaw(glm::normalize(glm::vec3(1.0F, -1.0F, -1.0F))));
        getWorldRootNode()->addChildNode(pDirectionalLightNode);
    });
}
```

You can also add other types of light sources or configure ambient lighting using `EnvironmentNode` but we will stick with the minimum for now.

If you run your project now you should see a cube. Now close your project, look at your console/logs and make sure there are no warnings/errors reported in the end.

We can now continue and work on handling user input to allow our character to fly.

### Working with user input

#### Input events

The usual way to handle user input is by binding to action/axis events and doing your processing once these events are triggered.

Each input event (action/axis event) is a pair:
- action event contains:
  - action ID (`unsigned int`, for example 0, 1, 2, ...)
  - array of keys (for example key `W` and `ArrowUp`) that trigger the event
- axis event contains:
  - axis ID (`unsigned int`, for example 0, 1, 2, ...)
  - array of key pairs (for example `W` and `S`, `ArrowUp` and `ArrowDown`) that trigger that event and define +1.0 and -1.0 states of the axis event

Action events are used for input that can only have 2 states: pressed and not pressed (for example a jump action), and axis events are used for input that can have a "smooth"/floating state (from -1.0 to +1.0, think about gamepad thumbsticks or `W`/`S` button combination for moving forward/backward).

Action events are a perfect case for, say, Jump or Fire/Shoot event since those can only have 2 states: on and off. So when you use an action event your input callback will receive input as a `bool` value (pressed/not pressed).

Axis events can be used for gamepad input and to replace 2 keyboard action events. You can have 2 action events: "moveForward" on `W` and "moveBackward" on `S` or just 1 axis event "moveForward" with `W` as +1.0 input and `S` as -1.0 input. So when you use an axis event your input callback will receive input as a `float` value (from +1.0 to -1.0). 

Let's consider 2 examples for axis events:
    - If we have gamepad thumbstick we can use 2 axis events: one for X movement and one for Y movement. As you might expect the X movement axis event will trigger +1.0 value when you move your thumbstick fully to the right and your axis event will trigger -1.0 when your thumbstick is moved fully to the left. You will receive 0.0 input when the thumbstick remains in its default (centered) state.
    - If you use a keyboard for moving your character you can use 2 axis events: one for right/left movement and one for forward/backward movement. When the user presses the button for moving forward you will receive +1.0 value and when the user presses the button for moving backward you will receive -1.0 value, same goes for right/left movement. When no button is pressed you will receive 0.0 value. For movement, since it has 2 directions on each axis it's better to use one axis event for each direction instead of 2 action events for each direction to simplify your code. In addition if in the future you will decide to add gamepad support if you have axis events for movement it will be very easy to bind gamepad input to movement while if you use action events you would have to rewrite them to use axis events.

Note
> A so-called "repeat" input is disabled in the engine. "Repeat" input happens when use hold some button, while you hold it the window keeps receiving "repeat" input events with this button but the engine will ignore them and so if you will hold a button only pressed/released events will be received by your code.

Note
> We don't have gamepad support yet.

Mouse movement is handled using `GameInstance::onMouseMove` function or `Node::onMouseMove` function. There are other mouse related functions like `onMouseScrollMove` that you might find useful.

#### Handling input event IDs

In the next sections you will learn that you can bind to input events in game instance and in nodes. As it was said earlier input events use IDs to be distinguished. This means that you need to have an application-global collection of unique IDs for input events.

Prefer to describe input event IDs of your application in a separate file using enums, for example:

```Cpp
// GameInputEventIds.hpp

#pragma once

/** Stores unique IDs of input events. */
struct GameInputEventIds {
    /** Groups action events. */
    enum class Action : unsigned int {
        CAPTURE_MOUSE_CURSOR = 0, //< Capture mouse cursor.
        INCREASE_CAMERA_SPEED,    //< Increase camera's speed.
        DECREASE_CAMERA_SPEED,    //< Decrease camera's speed.
    };

    /** Groups axis events. */
     enum class Axis : unsigned int {
        MOVE_CAMERA_FORWARD = 0, //< Move camera forward/back.
        MOVE_CAMERA_RIGHT,       //< Move camera right/left.
        MOVE_CAMERA_UP,          //< Move camera up/down.
    };
};
```

This file will store all input IDs that your game needs, even if you have switchable controls like "walking" or "in vehice" all input event IDs should be generally stored like that to make sure they all have unique IDs because all your input events will be registered in the same input manager.

Note
> We will talk about switchable controls like "walking" or "in vehice" and how to handle them in one of the next sections.

#### Binding to input events in game instance

Since input events are identified using unique IDs we should create a special struct for our input IDs:

```Cpp
// GameInputEventIds.hpp

#pragma once

/** Stores unique IDs of input events. */
struct GameInputEventIds {
    /** Groups action events. */
    enum class Action : unsigned int {
        ACTION_EVENT1 = 0,
    };

    /** Groups axis events. */
     enum class Axis : unsigned int {
        AXIS_EVENT1 = 0,
    };
};
```

Let's see how we can bind to input events in our `GameInstance` class.

```Cpp
// MyGameInstance.cpp

#include "GameInputEventIds.hpp"

void MyGameInstance::MyGameInstance(
    Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
    : GameInstance(pWindow, pGameManager, pInputManager) {
    // Register action event.
    auto optionalError = pInputManager->addActionEvent(
        static_cast<unsigned int>(GameInputEventIds::Action::ACTION_EVENT1),
        {KeyboardKey::KEY_F});
    if (optionalError.has_value()) [[unlikely]] {
        // ... handle error ...
    }
    
    // Register axis event.
    optionalError = pInputManager->addAxisEvent(
        static_cast<unsigned int>(GameInputEventIds::Action::AXIS_EVENT1),
        {{KeyboardKey::KEY_A, KeyboardKey::KEY_D}});
    if (optionalError.has_value()) [[unlikely]] {
        // ... handle error ...
    }

    // Now let's bind callback functions to our events.

    // Bind action events.
    {
        const auto pActionEvents = getActionEventBindings();
        std::scoped_lock guard(pActionEvents->first);

        pActionEvents->second[static_cast<unsigned int>(GameInputEventIds::Action::ACTION_EVENT1)]
            = [](KeyboardModifiers modifiers, bool bIsPressedDown) {
            Logger::get().info(std::format("action event triggered, state: {}", bIsPressedDown));
        };
    }

    // Bind axis events.
    {
        const auto pAxisEvents = getAxisEventBindings();
        std::scoped_lock guard(pAxisEvents->first);

        pAxisEvents->second[static_cast<unsigned int>(GameInputEventIds::Action::AXIS_EVENT1)]
            = [](KeyboardModifiers modifiers, float input) {
            Logger::get().info(std::format("axis event triggered, value: {}", input));
        };
    }
}
```

As you can see we are using game instance constructor and we don't create a separate function to "add" our input events. This is done intentionally because once input events were added you should not add the same input events again, thus no function - we don't want anybody to accidentally add input events again.

At this point you will be able to trigger registered action/axis events by pressing the specified keyboard buttons.

Note
> Although you can process input events in `GameInstance` it's not really recommended because input events should generally be processed in nodes such as in your character node so that your nodes will be self-contained and modular.

Note
> You can register/bind input events even when world does not exist or not created yet.

Note
> You can bind to input events before registering input events - this is perfectly fine.

#### Binding to input events in nodes

This is the most common use case for input events. The usual workflow goes like this:
1. Register action/axis events with some default keys in your `GameInstance` constructor.
2. Bind to input processing callbacks in your nodes.

Let's define 2 axis events in our game instance: one for moving right/left and one for moving forward/backward:

```Cpp
// GameInputEventIds.hpp

#pragma once

/** Stores unique IDs of input events. */
struct GameInputEventIds {
    /** Groups action events. */
    enum class Action : unsigned int {
        CLOSE_APP = 0,
    };

    /** Groups axis events. */
     enum class Axis : unsigned int {
        MOVE_RIGHT = 0,
        MOVE_FORWARD,
    };
};
```

Now let's register and bind to those events:

```Cpp
#include "GameInputEventIds.hpp"

void MyGameInstance::MyGameInstance(
    Window* pWindow, GameManager* pGameManager, InputManager* pInputManager)
    : GameInstance(pWindow, pGameManager, pInputManager) {
    // Register "moveRight" axis event.
    auto optionalError = pInputManager->addAxisEvent(
        static_cast<unsigned int>(GameInputEventIds::Axis::MOVE_RIGHT),
        {{KeyboardKey::KEY_D, KeyboardKey::KEY_A}});
    if (optionalError.has_value()) [[unlikely]] {
        // ... handle error ...
    }

    // Register "moveForward" axis event.
    optionalError = pInputManager->addAxisEvent(
        static_cast<unsigned int>(GameInputEventIds::Axis::MOVE_FORWARD),
        {{KeyboardKey::KEY_W, KeyboardKey::KEY_S}});
    if (optionalError.has_value()) [[unlikely]] {
        // ... handle error ...
    }

    // Register "closeApp" action event.
    optionalError = pInputManager->addActionEvent(
        static_cast<unsigned int>(GameInputEventIds::Action::CLOSE_APP),
        {KeyboardKey::KEY_ESCAPE});
    if (optionalError.has_value()) [[unlikely]] {
        // ... handle error ...
    }
}
```

In our node create 2 new private fields and one new function that we will use:

```Cpp
protected:
    virtual void onBeforeNewFrame(float timeSincePrevFrameInSec) override;

private:
    glm::vec2 lastInputDirection = glm::vec2(0.0F, 0.0F);

    static constexpr float movementSpeed = 5.0F;
```

Now in our node's constructor let's bind to input events. We don't need to care about register/bind order because you can bind to input events before registering input events - this is perfectly fine.

```Cpp
// FlyingCharacter.cpp

#include "FlyingCharacter.h"

// Custom.
#include "game/node/CameraNode.h"
#include "GameInputEventIds.hpp"

#include "FlyingCharacter.generated_impl.h"

FlyingCharacterNode::FlyingCharacterNode() : FlyingCharacterNode("Flying Character Node") {}

FlyingCharacterNode::FlyingCharacterNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
    // Create our camera node.
    pCameraNode = sgc::makeGc<CameraNode>("Player Camera");

    // Attach the camera to the character.
    addChildNode(pCameraNode, AttachmentRule::KEEP_RELATIVE, AttachmentRule::KEEP_RELATIVE);

    // Make our node to receive user input. Input will be received only when the node is spawned.
    setIsReceivingInput(true);

    // Make our node to be called every frame so that we can apply input to movement.
    // Node will be called every frame only while it's spawned.
    setIsCalledEveryFrame(true);

    // Bind to axis events.
    {
        const auto pAxisEvents = getAxisEventBindings();
        std::scoped_lock guard(pAxisEvents->first);

        pAxisEvents->second[static_cast<unsigned int>(GameInputEventIds::Axis::MOVE_FORWARD)]
            = [this](KeyboardModifiers modifiers, float input) {
            lastInputDirection.x = input;
        };

        pAxisEvents->second[static_cast<unsigned int>(GameInputEventIds::Axis::MOVE_RIGHT)]
            = [this](KeyboardModifiers modifiers, float input) {
            lastInputDirection.y = input;
        };
    }
}

void FlyingCharacterNode::onChildNodesSpawned() {
    SpatialNode::onChildNodesSpawned();

    pCameraNode->makeActive();
}

void FlyingCharacterNode::onBeforeNewFrame(float timeSincePrevFrameInSec) {
    // Check for early exit.
    // Also make sure input direction is not zero to avoid NaNs during `normalize` (see below).
    if (glm::all(glm::epsilonEqual(lastInputDirection, glm::vec2(0.0F, 0.0F), 0.0001F))) {
        return;
    }

    // Normalize direction to avoid speed up on diagonal movement and apply speed.
    const auto movementDirection =
        glm::normalize(lastInputDirection) * timeSincePrevFrameInSec * movementSpeed;

    // Get node's world location.
    auto newWorldLocation = getWorldLocation();

    // Calculate new world location.
    newWorldLocation += getWorldForwardDirection() * movementDirection.x;
    newWorldLocation += getWorldRightDirection() * movementDirection.y;

    // Apply movement.
    setWorldLocation(newWorldLocation);
}
```

As you can see we have decided to apply input movement not instantly but once a frame. The motivation for this is that we now have `timeSincePrevFrameInSec` (also known as deltatime - time in seconds that has passed since the last frame was rendered) to eliminate a speed up that occurs on diagonal movement (length of the vector becomes ~1.41 on diagonal movement while we expect the length of the vector to be in the range [0.0; 1.0]).

In addition to this it may happen that a button is pressed/released multiple times during one frame (this is even more likely when use have a gamepad and use thumbsticks for movement) which will cause our input callbacks to be triggered multiple times during one frame, so if we process the movement instantly this might affect the performance. **Note that movement input is special because we can process it like that but it does not mean that all other input should be handled like this, for example an action to Fire/Shoot should be processed instantly or an action Open/Interact should also be processed instantly, in addition "look" or "rotation" mouse events should also be processed instantly.**

Compile and run your project now, you can try modifying `movementSpeed` variable to fit your needs or even make it non-constant if you want.

Let's now make our character to look/rotate using mouse movement. Add a new protected function and a private field in your node:

```Cpp
protected:
    virtual void onMouseMove(double xOffset, double yOffset) override;

private:
    static constexpr double rotationSpeed = 0.1;
```

And implementation:

```Cpp
void FlyingCharacterNode::onMouseMove(double xOffset, double yOffset) {
    auto currentRotation = getRelativeRotation();

    currentRotation.z += static_cast<float>(xOffset * rotationSpeed); // modify "yaw"
    currentRotation.y -= static_cast<float>(yOffset * rotationSpeed); // modify "pitch"

    setRelativeRotation(currentRotation);
}
```

Note
> Make sure to use relative rotation and not world rotation.

And let's hide the cursor.

```Cpp
void FlyingCharacterNode::onChildNodesSpawned() {
    SpatialNode::onChildNodesSpawned();

    // Hide (capture) mouse cursor.
    getGameInstance()->getWindow()->setCursorVisibility(false);

    // Enable camera.
    pCameraNode->makeActive();
}
```

Compile and run. You should be able to rotate our character using mouse input.

As the final step let's finish by implementing "closeApp" action event, this one is very simple:

```Cpp
FlyingCharacterNode::FlyingCharacterNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
    // ... some code here ...

    // Bind to axis events.
    // ... some code here ...

    // Bind to action events.
    {
        const auto pActionEvents = getActionEventBindings();
        std::scoped_lock guard(pActionEvents->first);

        pActionEvents->second[static_cast<unsigned int>(GameInputEventIds::Action::CLOSE_APP)]
            = [](KeyboardModifiers modifiers, bool bIsPressed) {
            getGameInstance()->getWindow()->close();
        };
    }
}
```

If you read the documentation for `Node::setIsReceivingInput` you would know that we can change whether we receive input or not while spawned. This can be handy if your game has (for example) a car that your character can drive, once your character enters a car you can use `PlayerNode::setIsReceivingInput(false)` and `CarNode::setIsReceivingInput(true)` and when the character leaves a car do the opposite `CarNode::setIsReceivingInput(false)` and `PlayerNode::setIsReceivingInput(true)`. Although if your character can still use some of his controls while driving a car you might want to implement some states like `bool bIsDrivingCar` and in some input callbacks in `PlayerNode` check this variable to decide whether an action should be executed or not.

This will be enough for now. Later we will talk about using `InputManager` in a slightly better way and also talk about saving/loading inputs.

## Working with timers

### Timers and GameInstance

Let's consider an example where you need to trigger some logic after some time in your `GameInstance`:

```Cpp
void MyGameInstance::onGameStarted() override {
    // Create a timer.
    pTimer = createTimer("custom");

    // (optional) Let's set timer to wait 1 second (1000 ms) before calling our callback.
    pTimer->setCallbackForTimeout(1000, []() { 
        Logger::get().info("Hello from timer callback!");

        // Timer was not set as `looping` so there is no need to explicitly stop it.
    });

    // Start the timer.
    pTimer->start();

    // ... some code here ...

    // Let's see how much time has passed.
    const auto iTimePassedInMs = pTimer->getElapsedTimeInMs();
}
```

As the documentation for `Timer::setCallbackForTimeout` says your callback will be called on the main thread so can safely use all engine functions from it. Moreover, the engine does some additional work for you and guarantees that this callback will never be called on deleted `GameInstance` (see function documentation for more details).

We store a `Timer* pTimer = nullptr` in the header file of our game instance to reuse that timer. **There's currently no `removeTimer` function (and may even never be) so it might be a good idea to reuse your timers instead of creating new timers again and again.** Returned raw pointer `Timer*` is guaranteed to be valid while the game instance is valid.

Using timer callbacks in game instance is pretty safe and you generally shouldn't worry about anything. And as always if you have any questions you might look at the documentation of the timer class and at the timer tests at `src/engine_tests/game/GameInstance.cpp`.

### Timers and Node

Same timer functions are used in nodes:

```Cpp
MyDerivedNode() {
    // Create a timer in constructor to reuse it later.
    pTimer = createTimer("custom");
};

void MyDerivedNode::onSpawning() override{
    Node::onSpawning(); // don't forget to call parent's version

    // (optional) Let's set timer to wait 1 second (1000 ms) before calling our callback.
    pTimer->setCallbackForTimeout(1000, []() { 
        Logger::get().info("Hello from timer callback!");

        // Timer was not set as `looping` so there is no need to explicitly stop it.
    });

    // Start the timer.
    pTimer->start();

    // ... some code here ...

    // Let's see how much time has passed.
    const auto iTimePassedInMs = pTimer->getElapsedTimeInMs();
}
```

Some things about timers in nodes:
    - Timers in nodes can only be `Timer::start`ed while the node is spawned any attempt to start a timer while the node is despawned (or not spawned yet) will result in an error being logged.
    - Created timers will be automatically stopped before `Node::onDespawning`.
    - Timer's callback will only be called while the timer is running and your node is spawned. This means that when your timer's callback is started you know that the node is still spawned.
    - Timer's callback will be called on the main thread.

Again, we store a `Timer* pTimer = nullptr` in the header file of our node to reuse that timer. **There's currently no `removeTimer` function (and may even never be) so it might be a good idea to reuse your timers instead of creating new timers again and again.** Returned raw pointer `Timer*` is guaranteed to be valid while the node is valid.

As with game instance using timer callbacks in nodes is pretty safe and you generally shouldn't worry about anything. And as always if you have any questions you might look at the documentation of the timer class and at the timer tests at `src/engine_tests/misc/Timer.cpp`.

## Node callbacks (including asynchronous tasks)

In your game you would most likelly want to use callbacks to trigger custom logic in nodes when some event happens, for example when your character's collision hits/overlaps with something you might want the collision node to notify the character node to do some custom logic.

When you think of callbacks you might think of `std::function` but the problem is that `std::function` has no guarantees if the node is spawned or not, or if it was deleted (freed). In order to avoid issues like that the engine provides `NodeFunction` - it's an `std::function` wrapper used for `Node` functions/lambdas with an additional check (compared to the usual `std::function`): once the callback is called `NodeFunction` will first safely check if the node, the callback function points to, is still spawned or not, and if not spawned then the callback function will not be called to avoid running functions on despawned nodes or hitting deleted memory.

That "safely check" means that the engine does not use your node for the check because the node might be deleted and calling any functions on it might lead to undefined behaviour.

So when you need callbacks for triggering custom logic in `Node`s it's highly recommended to use the `NodeFunction` instead of the `std::function`.

Additionally, when you use asynchronous requests (for example when you make asynchronous requests to remote backend server) for triggering some `Node` callbacks to process asynchronous results you should also use `NodeFunction`. This would protect you from running into despawned/deleted nodes if the asynchronous result was received unexpectedly late (for example) so you don't need to check if the node is still valid or not.
Your code then will look like this:

```Cpp
// Inside of some `Node`:

// note that `Node::getNodeId()` below will return empty if the node is not spawned and
// we use `.value()` directly just to simplify our example
auto onFinished = NodeFunction<void(bool, int)>(getNodeId().value(), [this](bool bSomeParameter, int iSomeParameter){
    // Asynchronous call was finished successfully and `this` node is still spawned.
    // Do some logic here.
});

SomeSystem->DoAsynchronousQuery(
    someData,
    [onFinished](bool bSomeParameter, int iSomeParameter){
        onFinished(bSomeParameter, iSomeParameter); // won't call the callback if the node is despawned or deleted
    });
```

## Safely using the publisher-subscriber pattern with nodes

Every game has custom game events, in this engine some nodes can subscribe to a specific publisher object, this way subscriber nodes can be notified when a specific event happens. For example, if your character node wants to know when its collision hits something we can use the publisher-subscriber pattern. When the publisher object decides that the event has happend it notifies all subscriber nodes and calls `NodeFunction` callbacks of all subscribed nodes.

In the engine the publisher-subscriber pattern is implemented by the `NodeNotificationBroadcaster` class. `Node` class already has a built-in functionality to use these broadcasters. You can use `Node::createNotificationBroadcaster` to create such broadcasters, allow other nodes to subscribe by specifying their `NodeFunction` callbacks and notify subscribers by calling the `broadcast` method. Here is a simplified example:

```Cpp
class MyNode : public Node{
public:
    MyNode(){
        // Create broadcaster.
        pEventBroadcaster = createNotificationBroadcaster<void(bool, int)>();
    }
    virtual ~MyNode() override = default;

    // Allow other nodes to subscribe to the event.
    // Return a unique binding ID if other node wants to unsubscribe later.
    size_t subscribeToEvent(const NodeFunction<void(bool, int)>& callback){
        return pEventBroadcaster->subscribe(callback);
    }

    // Allow other nodes to unsubscribe from the event.
    void unsubscribeFromEvent(size_t iBindingId){
        pEventBroadcaster->unsubscribe(iBindingId);
    }

private:
    void onAfterSomeGameEvent(bool bSomeParameter, int iSomeParameter){
        // Notify subscribers (queues a deferred task in which calls all subscribed callbacks).
        pEventBroadcaster->broadcast(bSomeParameter, iSomeParameter);
    }

    NodeNotificationBroadcaster<void(bool, int)>* pEventBroadcaster = nullptr;
}

// inside of some other spawned (!) node (otherwise `getNodeId()` returns empty):
const auto iBindingId = pPublisherNode->subscribeToEvent(
    NodeFunction<void(bool, int)>(getNodeId().value(), [this](bool bSomeParameter, int iSomeParameter){
    // event occurred
}));

// ... later (if we need to) ...

pPublisherNode->unsubscribeFromEvent(iBindingId);
```

Note that you don't need to unsubscribe when your subscribed node is being despawned/destroyed as this is done automatically. Each `broadcast` call removes callbacks of despawned nodes.

Note
> Because `broadcast` call simply loops over all registered callbacks extra care should be taken when you subscribe to pair events such as "on started overlapping" / "on finished overlapping" because the order of these events may be incorrect in some special situations. When subscribing to such "pair" events it's recommended to add checks in the beginning of your callbacks that make sure the order is correct (as expected), otherwise show an error message so that you will instantly notice that and fix it.

## Saving and loading user's input settings

`InputManager` allows creating input events and axis events, i.e. allows binding names with multiple input keys. When working with input the workflow for creating, modifying, saving and loading inputs goes like this:

```Cpp
using namespace ne;

void MyGameInstance::onGameStarted(){
    // On each game start, create action/axis events with default keys.
    const std::vector<std::pair<KeyboardKey, KeyboardKey>> vMoveForwardKeys = {
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_W, KeyboardKey::KEY_S),
        std::make_pair<KeyboardKey, KeyboardKey>(KeyboardKey::KEY_UP, KeyboardKey::KEY_DOWN)
    };

    auto optionalError = getInputManager()->addAxisEvent(
        static_cast<unsigned int>(GameInputEventIds::Axis::MOVE_FORWARD),
        vMoveForwardKeys);
    if (optionalError.has_value()){
        // ... handle error ...
    }

    // ... add more default events here ...

    // After we defined all input events with default keys:
    // Load modifications that the user previously saved (if exists).
    constexpr auto pInputFilename = "input"; // filename "input" is just an example, you can use other filename
    const auto pathToFile
        = ConfigManager::getCategoryDirectory(ConfigCategory::SETTINGS) / pInputFilename
            + ConfigManager::getConfigFormatExtension();
    if (std::filesystem::exists(pathToFile)){
        optionalError = getInputManager()->loadFromFile(pInputFilename);
        if (optionalError.has_value()){
            // ... handle error ...
        }
    }

    // Finished.
}

// Later, the user modifies some keys:
optionalError = getInputManager()->modifyAxisEventKey(
    static_cast<unsigned int>(GameInputEventIds::Axis::MOVE_FORWARD),
    oldKey,
    newKey);
if (optionalError.has_value()){
    // ... handle error ...
}

// The key is now modified in the memory (in the input manager) but if we restart our game the modified key will
// be the same (non-modified), in order to fix this:

// After a key is modified we save the modified inputs to the disk to load modified inputs on the next game start.
optionalError = getInputManager()->saveToFile(pInputFilename);
if (optionalError.has_value()){
    // ... handle error ...
}
```

As it was shown `InputManager` can be acquired using `GameInstance::getInputManager()`, so both game instance and nodes (using `getGameInstance()->getInputManager()`) can work with the input manager.

## Configuring graphics settings

Graphics settings are configured using the `RenderSettings` class. In order to get render settings from a game instance you need to use the following approach:

```Cpp
void MyGameInstance::foo(){
    // Get renderer.
    const auto pRenderer = getWindow()->getRenderer();

    // Get render settings.
    const auto mtxRenderSettings = pRenderer->getRenderSettings();
    std::scoped_lock guard(*mtxRenderSettings.first);

    // Get supported render resolutions.
    auto result = pRenderer->getSupportedRenderResolutions();
    if (std::holds_alternative<Error>(result)){
        // ... handle error ...
    }
    const auto supportedRenderResolutions = std::get<...>(std::move(result));

    // ... display `supportedRenderResolutions` on screen ...

    // Later, when user wants to change render resolution:
    mtxRenderSettings.second->setRenderResolution(someRenderResolution);
}
```

When use change something in `RenderSettings` (for example render resolution) that change is instantly saved on the disk in the renderer config so you don't need to save them manually, on the next startup last applied settings will be restored.

You can find renderer's config file at:
    - (on Windows) `%%localappdata%/nameless-engine/*yourtarget*/engine/render.toml`
    - (on Linux) `~/.config/nameless-engine/*yourtarget*/engine/render.toml`

Note
> You can change values in the specified config files to quicky change settings for testing purposes. Note that changes made in the config files will only be applied locally (only for your computer).

Note that some render settings might not be supported depending on the OS/renderer/hardware. Let's consider another example, this one uses anti-aliasing:

```Cpp
void MyGameInstance::foo(){
    // Get renderer.
    const auto pRenderer = getWindow()->getRenderer();

    // Get render settings.
    const auto mtxRenderSettings = pRenderer->getRenderSettings();
    std::scoped_lock guard(*mtxRenderSettings.first);

    // Get maximum supported AA quality.
    auto result = mtxRenderSettings.second->getMaxSupportedAntialiasingQuality();
    if (std::holds_alternative<Error>(result)){
        // ... handle error ...
    }
    const auto maxQuality = std::get<std::optional<AntialiasingQuality>>(result); // empty if AA is not supported

    // ... display all `AntialiasingQuality` values on the screen but don't exceed `maxQuality` ...

    // Later, when user wants to change AA:
    mtxRenderSettings.second->setAntialiasingQuality(selectedQuality);
}
```

As always if you forget something or pass unsupported value the engine will let you know by logging an error so pay attention to the logs/console.

## Saving and loading user's progress data

### Overview

There are 2 ways to save/load custom data:
    - using `ConfigManager` (`io/ConfigManager.h`) - this approach is generally used when you have just 1-2 simple variables that you want to save or when you don't want to / can't create a type and use reflection
    - using reflection - this approach is generally used more often than `ConfigManager` and it provides great flexibility

Let's start with the most commonly used approach.

### Saving and loading data using reflection

#### Overview

One of the previous sections of the manual taught you how to use reflection for your types so at this point you should know how to add reflection to your type. Now we will talk about additional steps that you need to do in order to use serialization/deserialization. As you might guess we will use some C++ types (that store some user data) and serialize them to file.

Reflection serialization uses `TOML` file format so if you don't know how this format looks like you can search it right now on the Internet. `TOML` format generally looks like `INI` format but with more features.

Let's consider an example that shows various serialization features and then explain some details:

```Cpp
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

    /// Not required for serialization function to add a specific item instance to the inventory data
    /// so that it will be saved to file.
    void addOneItem(unsigned long long iItemId) {
        const auto it = items.find(iItemId);

        if (it == items.end()) {
            items[iItemId] = 1;
            return;
        }

        it->second += 1;
    }

    /// Not required for serialization function to remove a specific item instance from the inventory.
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

    /// Not required for serialization function that returns the amount of specific items in the inventory.
    unsigned long long getItemAmount(unsigned long long iItemId) {
        const auto it = items.find(iItemId);
        if (it == items.end())
            return 0;

        return it->second;
    }

private:
    /// Contains item ID as a key and item amount (in the inventory) as a value.
    /// This data might not be your actual in-game inventory instead it may store some minimal data
    /// that you can use to create an actual inventory when loading user's progress.
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

    // ... maybe some other code/fields here ...

    Ability_GENERATED
};

/// Type that we will serialize and deserialize.
/// Contains objects of both `Ability` and `InventorySaveData` types plus some additional data.
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

    /// We can serialize fields of custom user types if they also derive from `Serializable`.
    RPROPERTY(Serialize)
    InventorySaveData inventory;

    /// Can also store objects of type that derive from `Ability` without any serialization/deserialization issues
    /// (just as you would expect).
    RPROPERTY(Serialize)
    std::vector<std::unique_ptr<Ability>> vAbilities;

    PlayerSaveData_GENERATED
};

File_PlayerSaveData_GENERATED

// ---------------------------------------

{
    // Somewhere in the game code.
    std::unique_ptr<PlayerSaveData> pPlayerSaveData = nullptr;

    // ... if the user creates a new player profile ...
    pPlayerSaveData = std::make_unique<PlayerSaveData>();

    // Fill save data with some information.
    pPlayerSaveData->sCharacterName = "Player 1";
    pPlayerSaveData->iCharacterLevel = 42;
    pPlayerSaveData->iExperiencePoints = 200;
    pPlayerSaveData->vAbilities = {std::make_unique<Ability>("Fire"), std::make_unique<Ability>("Wind")};
    pPlayerSaveData->inventory.addOneItem(42);
    pPlayerSaveData->inventory.addOneItem(42); // now have two items with ID "42"
    pPlayerSaveData->inventory.addOneItem(102);

    // Prepare a new unique file name for the new save file.
    const std::string sNewProfileName = ConfigManager::getFreeProgressProfileName();

    // Serialize.
    const auto pathToFile = ConfigManager::getCategoryDirectory(ConfigCategory::PROGRESS) / sNewProfileName;
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
    auto result = Serializable::deserialize<std::unique_ptr<PlayerSaveData>>(pathToFile);
    if (std::holds_alternative<Error>(result)) [[unlikely]] {
        // ... handle error ...
    }

    const auto pPlayerSaveData = std::get<std::unique_ptr<PlayerSaveData>>(std::move(result));

    // Everything is deserialized:
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

Let's now describe what needs to be do in order to use serialization and deserialization in your reflected type:
1. Derive from `Serializable` (`io/Serializable.h`) in order to have `serialize`/`deserialize` and similar functions. Note that base `Node` class already derives from `Serializable` so you might already derive from this class.
2. Make sure your type has a constructor without parameters (make sure that it's not deleted) as it's required for deserialization. Use can override `Serializable::onAfterDeserialized` to do post-deserialization logic (just make sure to read this function's docs first).
3. Don't forget to override destructor, for example: `virtual ~T() override = default;`.
3. Add `Guid` property to your type's `RCLASS`/`RSTRUCT` macro with a random GUID (just search something like "generate GUID" on the Internet), this GUID will be saved in the file so that when the engine reads the file it will know what type to use. In debug builds when the engine starts it checks uniqueness of all GUIDs so you shouldn't care about GUID uniqueness.
3. Mark fields that you want to be serialized/deserialized with `Serialize` property on `RPROPERTY` macro.
4. Make sure that all fields marked with `RPROPERTY(Serialize)` that have primitive types have initialization values, for example: `long long iCharacterLevel = 0;` otherwise you might serialize a "garbage value" (since primitive types don't have initialization unlike `std::string` for example) and then after deserialization be suprised to see a character of level `-123215315115` or something like that.

And here are some notes about serialization/deserialization:
- If you released your game and in some update you added a new `Serialize` field then everything (saving/loading) will work fine, the new field just needs to have initialization value for old save files that don't have this value (again, if it's a primitive type just initialize it otherwise you are fine).
- If you released your game and in some update you removed/renamed some `Serialize` field then everything (saving/loading) will work fine and if some old save file will have that old removed/renamed field it will be ignored.
- If you released your game it's highly recommended to not change your type's GUID otherwise the engine will not be able to deserialize the data since it will no longer know what type to use.

If you look in the file `src/engine_lib/public/io/properties/SerializeProperty.h` you might find that `Serialize` has a constructor that accepts `FieldSerializationType` which is `FST_WITH_OWNER` by default and as the documentation says it means that "Field is serialized in the same file as the owner object". There is also an option to use `FST_AS_EXTERNAL_FILE` or `FST_AS_EXTERNAL_BINARY_FILE` like so:

```Cpp
RPROPERTY(Serialize(FST_AS_EXTERNAL_BINARY_FILE)) // allow VCSs to treat this file in a special way
MeshData meshData;
```

And again, as the documentation says it means that "Field is serialized in a separate file located next to the file of owner object. The only difference between these two is that `FST_AS_EXTERNAL_FILE` serializes into a `TOML` file and `FST_AS_EXTERNAL_BINARY_FILE` serializes into a binary file using special binary field serilaizers. Some fields from engine types are stored as external binary files in order to provide smaller file size and faster deserialization (although sacrificing readability of the file). Only fields of types that derive from `Serializable` can be marked with this type". Generally we use the default `FST_WITH_OWNER` but there might be situations where you might want to use additional options.

There are various types that you can mark as `Serialize` as you can see you can mark some primitive types, `std::string`, `std::vector`, `std::unordered_map` and more, but note that when we talk about containers (such as `std::vector`) their serialization depends on the contained type. Here is a small description of **some types** that you can use for serialization:
- `bool`
- `int`
- `unsigned int`
- `long long`
- `unsigned long long`
- `float`
- `double`
- `std::string`
- `T` (where `T` is any type that derives from Serializable)
- and more!

See the directory `src/engine_lib/public/io/serializers` for available field serializers (you don't need to use them directly, they will be automatically picked when you use `serialize` function).

If you can't find some desired type in available field serializers don't worry, you can write your own field serializer (it's not that hard) to support new field types to be serialized/deserialized but we will talk about this later in one of the next sections.

As it was previously said our reflection serialization system uses `TOML` file format so if you need even more flexibility you can serialize `toml::value` structures directly. Under the hood we use https://github.com/ToruNiina/toml11 for working with `TOML` files so if you want to serialize something very complicated in a very special way you can read the documentation for this library and serialize `toml::value`s yourself or use a combination of raw `toml::value`s and our serialization system by using various overloads of `Serializable::serialize` that use `toml::value`.

In the end let's consider one more (this time very simplified) example where we serialize multiple objects into one file:

```Cpp
// Serialize 2 objects in 1 file.
Node node1("My Cool Node 1");
Node node2("My Cool Node 2");
SerializableObjectInformation node1Info(&node1, "0", {{sNode1CustomAttributeName, "1"}}); // will have ID 0
SerializableObjectInformation node2Info(&node2, "1", {{sNode2CustomAttributeName, "2"}}); // will have ID 1
const auto optionalError = Serializable::serializeMultiple(pathToFile, {node1Info, node2Info}, false);
if (optionalError.has_value()) {
    // ... handle error ...
}

// Deserialize.
const auto result = Serializable::deserializeMultiple<sgc::GcPtr<Serializable>>(pathToFile);
if (std::holds_alternative<Error>(result)) {
    // ... handle error ...
}
auto vDeserializedObjects
    = std::get<std::vector<DeserializedObjectInformation<sgc::GcPtr<Serializable>>>>(std::move(result));
```

#### Reflection limitations

Currently used version of the reflection library has some issues with multiple inheritance. If you are deriving from two classes one of which is `Serializable` (or a class/struct that derives from it) and the other is not an interface class (contains fields) then you might want to derive from `Serializable` first and then from that non-interface class. Example:

```Cpp
class IMyInterface{
public:
    virtual void foo() = 0;
    virtual void bar() = 0;
protected:
    virtual void foo2() = 0;
}

class MyParentClass{
public:
    void myFoo();

private:
    int iAnswer = 42;
}

// might fail when attempting to get reflected fields' values (will just read garbage)
class RCLASS(Guid("...")) MyClass : public MyParentClass, public Serializable{ 
  ...
}

// should work fine
class RCLASS(Guid("...")) MyClass : public Serializable, public IMyInterface{
  ...
}

// should work fine
class RCLASS(Guid("...")) MyClass : public Serializable, public MyParentClass{
  ...
}
```

Just keep this in mind when using reflection for serialization/deserialization.

### Saving and loading data using ConfigManager

`ConfigManager` (just like reflection serialization) uses `TOML` file format so if you don't know how this format looks like you can search it right now on the Internet. `TOML` format generally looks like `INI` format but with more features.

Here is an example of how you can save and load data using `ConfigManager`:

```Cpp
#include "io/ConfigManager.h"

using namespace ne;

// Write some data.
ConfigManager manager;
manager.setValue<std::string>("section name", "key", "value", "optional comment");
manager.setValue<bool>("section name", "my bool", true, "this should be true");
manager.setValue<double>("section name", "my double", 3.14159, "this is a pi value");
manager.setValue<int>("section name", "my long", 42); // notice no comment here

// Save to file.
auto optionalError = manager.saveFile(ConfigCategory::SETTINGS, "my file");
if (optionalError.has_value()) {
    // ... handle error ...
}

// -----------------------------------------
// Let's say that somewhere in other place of your game you want to read these values:

ConfigManager manager;
auto optionalError = manager.loadFile(ConfigCategory::SETTINGS, "my file");
if (optionalError.has_value()) {
    // ... handle error ...
}

// Read string.
const auto realString = manager.getValue<std::string>("section name", "key", "default value");
assert(realString == "value");

// Read bool.
const auto realBool = manager.getValue<bool>("section name", "my bool", false);
assert(realBool == true);

const auto realDouble = manager.getValue<double>("section name", "my double", 0.0);
assert(realDouble >= 3.13);

const auto realLong = manager.getValue<int>("section name", "my long", 0);
assert(realLong == 42);
```

As you can see `ConfigManager` is a very simple system for very simple tasks. Generally only primitive types and some STL types are supported, you can of course write a serializer for some STL type by using documentation from https://github.com/ToruNiina/toml11 as `ConfigManager` uses this library under the hood.

`ConfigManager` also has support for backup files and some other interesting features (see documentation for `ConfigManager`).

## Adding external dependencies

In CMake we modify our `CMakeLists.txt` to add external dependencies. This generally comes down to something like this:

```
message(STATUS "${PROJECT_NAME}: adding external dependency \"FMT\"...")
add_subdirectory(${RELATIVE_EXT_PATH}/fmt ${DEPENDENCY_BUILD_DIR_NAME}/fmt SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt)
set_target_properties(fmt PROPERTIES FOLDER ${EXTERNAL_FOLDER})
```

Note
> This manual expects that you know what the code from above does as it's a usual CMake usage.

Note
> As you can see we mark external dependencies with `SYSTEM` so that clang-tidy (enabled in release builds) will not analyze source code of external dependencies.

As it was said earlier you might need to do one additional step: if your target uses reflection (has `add_refureku` command) then you also need to tell the reflection generator about included headers of your external dependency:

```
## Write project include directories for Refureku.
set(REFUREKU_INCLUDE_DIRECTORIES
    ${ABSOLUTE_EXT_PATH}
    ${ABSOLUTE_EXT_PATH}/fmt/include     ## <- new dependency
    ${CMAKE_CURRENT_SOURCE_DIR}/public
    ${CMAKE_CURRENT_SOURCE_DIR}/private
)
add_refureku(
    ## some code here
```

If you compile your project after adding a new external dependency and the compilation fails with something like this:

```
[Error] While processing the following file: .../MyHeaderThatUsesExternalDependency.h: 'fmt/core.h' file not found (.../MyHeaderThatUsesExternalDependency.h, line 10, column 10), : 0:0
```

this is because you forgot to tell the reflection generator about some directory of your external dependency.

## Importing custom meshes

Generally you would import meshes using the editor but we will show how to do it in C++.

Note
> We only support import from GLTF/GLB format.

In order to import your file you need to use `MeshImporter` like so:

```Cpp
#include "io/MeshImporter.h"

using namespace ne;

auto optionalError = MeshImporter::importMesh(
    "C:\\models\\DamagedHelmet.glb",       // importing GLB as an example, you can import GLTF in the same way
    "game/models",                         // path to the output directory relative `res` (should exist)
    "helmet",                              // name of the new directory that will be created (should not exist yet)
    [](std::string_view sState) {
        Logger::get().info(sState);
    });
if (optionalError.has_value()) [[unlikely]] {
    // ... process error ...
}
```

If the import process went without errors you can then find your imported model in form of a node tree inside of the resulting directory. You can then deserialize that node tree and use it in your game using the following code:

```Cpp
// Deserialize node tree.
auto result = Node::deserializeNodeTree(
    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "game/models/helmet/helmet.toml");
if (std::holds_alternative<Error>(result)) {
    // ... process errorr ...
}
auto pImportedRootNode = std::get<sgc::GcPtr<Node>>(std::move(result));

// Spawn node tree.
getWorldRootNode()->addChildNode(pImportedRootNode);
```

### Configuring export settings for your mesh in Blender

Usually the only thing that you need to do is to untick the "+Y up" checkbox in "Transform" section (since we use +Z as our UP axis).

## Procedurally generating meshes

The most simple example of procedurally generated geometry is the following:

```Cpp
#include "misc/PrimitiveMeshGenerator.h"

using namespace ne;

// Spawn procedural cube mesh.
const auto pMeshNode = sgc::makeGc<MeshNode>();
pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
getWorldRootNode()->addChildNode(pMeshNode);
```

If you would look into how `PrimitiveMeshGenerator::createCube` is implemented you would see that it just constructs a `MeshData` by filling all positions, normals, UVs, etc. In the same way you can create a procedural mesh by constructing a `MeshData` object. After we assigned a new mesh data to our mesh node we can set materials and shaders to it.

## Working with materials

### Working with materials in C++

Each `MeshNode` uses a default engine material (if other material is not specified) which means that if we have a `MeshNode` it already has a material.

```Cpp
#include "material/Material.h"
#include "shader/general/EngineShaderNames.h"

// Set mesh's diffuse color to red.
pMeshNode->getMaterial()->setDiffuseColor(glm::vec(1.0F, 0.0F, 0.0F));
```

You can also assign a new material to your `MeshNode`:

```Cpp
#include "material/Material.h"
#include "shader/general/EngineShaderNames.h"

auto result = Material::create(
  EngineShaderNames::MeshNode::sVertexShaderName, // since we change `MeshNode`'s material we use `MeshNode` shaders
  EngineShaderNames::MeshNode::sPixelShaderName,
  false); // create with transparency disabled
if (std::holds_alternative<Error>(result)) {
  // ... handle error ...
}
auto pMaterial = std::get<std::unique_ptr<Material>>(std::move(result));

// Assign this material to your mesh node.
pMeshNode->setMaterial(std::move(pMaterial));

// Set mesh's diffuse color to red.
pMeshNode->getMaterial()->setDiffuseColor(glm::vec(1.0F, 0.0F, 0.0F));
```

As you can see you can specify custom shaders when creating a new material (use will talk about custom shaders in another section).

Please note that each "Material" here should not be considered as some "big thing" that you need to reuse on multiple meshes - no, each material here is just a collection of small parameters such as diffuse color. You can think of the material here as "material properties" or a "material instance" if you want. If you have multiple materials that use the same shaders it's perfectly fine because under the hood the engine will not duplicate any loaded shaders or similar resources so these materials will just reference 1 set of shaders.

Note
> This also means that you cannot just simply clone/duplicate/share a material between multiple meshes. Unfortunatelly, at the time of writing this, we can't just use `std::shared_ptr` for materials because it might create a false assumption when you have 2 meshes that reference a single material, serialize and deserialize them - would result in both meshes having a separate (unique) material and they will no longer reference a single material. Thus we use `std::unique_ptr` to avoid false assumptions. But generally there wouldn't be such a need for this. When you import some mesh in the engine (this information is covered in another section) it's imported as a node tree (an asset file) where you generally only have a mesh node, then when this asset (node tree) is used in some level it's added to your level as external node tree and if you have multiple assets that were taken from the same node tree it's enough to modify the material in the asset's node tree to then see changes in all assets on the level.

In order to enable transparency and use `Material::setOpacity` function you need to either create a material with transparency enabled (see example from above) or enable transparency using `Material::setEnableTransparency`:

```Cpp
#include "material/Material.h"
#include "shader/general/EngineShaderNames.h"

// Enable transparency.
pMeshNode->getMaterial()->setEnableTransparency(true);

// Set opacity.
pMeshNode->getMaterial()->setOpacity(0.5F);

// Set mesh's diffuse color to red.
pMeshNode->getMaterial()->setDiffuseColor(glm::vec(1.0F, 0.0F, 0.0F));
```

Note
> Transparent materials have very serious impact on the performance so you might want to avoid using them.

In order to use textures in your material you need to first import the textures you want to use. Most of the time you will import new textures through the editor using its GUI but we will show how to do it in C++:

```Cpp
#include "io/TextureImporter.h"

using namespace ne;

// Import some texture to be used as a diffuse texture in our materials.
auto optionalError = TextureImporter::importTexture(
    "C:\\somedirectory\\textures\\mytexture.png", // file to import
    TextureImportFormat::RGBA,                    // import format
    "game/player/textures",                       // will be imported to "res/game/player/textures/"
    "diffuse");                                   // "res/game/player/textures/diffuse/" will be created during import
if (optionalError.has_value()) {
    // ... process error ...
}
// texture is imported
```

In the example above after the image is imported the directory `res/game/player/textures/diffuse` will have multiple files with `DDS` and `KTX` extensions. Both formats are special GPU image formats with compression and mipmaps (if you heard about them). The `DDS` files are used by the DirectX renderer and the `KTX` files are used by the Vulkan renderer.

Let's now see how we can use this texture in our material:

```Cpp
pMeshNode->getMaterial()->setDiffuseTexture("game/player/textures/diffuse");
```

As you can see we specify a path to the directory with `DDS` and `KTX` files relative to our `res` directory and we don't need to point to a specific file because the engine will automatically use the appropriate file according to the currently used renderer.

Note if a texture is requested it will be loaded from disk, then if some other part of the game needs this texture it won't be loaded from disk again, it will just be used from the memory and finally when all parts of your game finish using a specific texture so that it's no longer used the texture will be automatically released from the memory.

Mesh nodes can have multiple materials assigned to different parts of the mesh. Both `MeshNode::getMaterial` and `MeshNode::setMaterial` have a default argument `iMaterialSlot = 0`. Each parts of the mesh that needs to have a separate material defines its own material slot, default cube only uses 1 material so it only has 1 material slot.

In order to query available material slots use `MeshNode::getAvailableMaterialSlotCount`. In order to create more material slots you need to define mesh that has multiple "parts". Information about these "parts" is stored in `MeshData`, here is an example:

```Cpp
// Create mesh node.
const auto pMeshNode = sgc::makeGc<MeshNode>();

// Generate cube mesh data.
auto meshData = PrimitiveMeshGenerator::createCube(1.0F);

// Most importantly `MeshData` stores:
// - `std::vector<MeshVertex>` vVertices - vertices of the mesh
// - `std::vector<std::vector<meshindex_t>>` vIndices - array that stores indices of the mesh per material slot

// Generated cube only has 1 material slot...
assert(meshData.getIndices()->size() == 1);

// ... and it has some indices in this slot (which is what we expect).
assert(meshData.getIndices()->at(0)->size() > 0);

// Set mesh data to our mesh node.
pMeshNode->setMeshData(std::move(meshData));

// Mesh data that we set has only 1 material slot so our mesh node now has only 1 material slot available
// (all new slots use engine default material).
assert(pMeshNode->getAvailableMaterialSlotCount() == 1);

// Spawn mesh node.
getWorldRootNode()->addChildNode(pMeshNode);
```

Now let's split the cube in 2 material slots so that 1 special face of the cube will use one material and other faces will use other material:

```Cpp
auto meshData = PrimitiveMeshGenerator::createCube(1.0F);
meshData.getIndices()->at(0) =  {
    0,  1,  2,  3,  2,  1,  // +X face.
    8,  9,  10, 11, 10, 9,  // +Y face.
    12, 13, 14, 15, 14, 13, // -Y face.
    16, 17, 18, 19, 18, 17, // +Z face.
    20, 21, 22, 23, 22, 21  // -Z face.
};
meshData.getIndices()->push_back({4,  5,  6,  7,  6,  5}); // -X face.

// Set mesh data to our mesh node.
pMeshNode->setMeshData(std::move(meshData));

// We now have 2 material slots.
assert(pMeshNode->getAvailableMaterialSlotCount() == 2);

// Modify first material slot.
pMeshNode->getMaterial(0)->setDiffuseColor(glm::vec3(1.0F, 0.0F, 0.0F));

// Modify second material slot.
pMeshNode->getMaterial(1)->setDiffuseColor(glm::vec3(0.0F, 1.0F, 0.0F));
pMeshNode->getMaterial(1)->setEnableTransparency(true);

// Spawn mesh node.
getWorldRootNode()->addChildNode(pMeshNode);
```

Generally you won't need to directly modify mesh data or material slots as this will happen automatically when you import some mesh from a (for example) GLTF/GLB file but it's good if you know what they are and where there are created/stored.

## Render statistics

If you want to know your game's FPS or other similar statistics you can use `Renderer::getRenderStatistics`. For example, you can display the FPS on your game's UI for debugging purposes:

```Cpp
void MyUiNode::onLoopingTimerTimeout() {
#if defined(DEBUG)
    const auto iFps = getWindow()->getRenderer()->getRenderStatistics()->getFramesPerSecond();
    // ... display on UI ...
#endif
}
```

## Using profiler

The engine has https://github.com/Celtoys/Remotery integrated and you can use this profiler in order to detect slow parts of your game.

By default profiler is disabled in order to enable it you need to create a file at `*project_root*/src/engine_settings.cmake` and add the following variable to it:

```
set(ENABLE_PROFILER ON)
```

Then you need to re-run cmake configuration and if everything is correct during the configuration you might see a message like `adding external dependency "Remotery"`. Note that when `ENABLE_PROFILER` is set profiler will be enabled only in debug builds.

Compile and run your project, during initialization you should see a message `profiler enabled` in the log.

Here are a few examples on how to use profiler:

```Cpp
#include "misc/Profiler.hpp"

using namespace ne;

void MyNode::onBeforeNewFrame(float timeSincePrevFrameInSec){
    Node::onBeforeNewFrame(timeSincePrevFrameInSec);

    PROFILE_FUNC;

    // ... some code here ...
}

void MyGameInstance::onMouseMove(double xOffset, double yOffset) {
    PROFILE_FUNC;

    // ... some code here ...

    {
        PROFILE_SCOPE(MyScope);

        // ... some code here ...
    }

    PROFILE_SCOPE_START(MyScope); // same as `PROFILE_SCOPE(MyScope)` but needs to be finished with `PROFILE_SCOPE_END`

    // ... some code here ...

    PROFILE_SCOPE_END;
}
```

You can use these macros interchangeably.

After you add profiling macros you need to run your app and open `*project_root*/ext/Remotery/vis/index.html` in your browser. When your app is running with profiler enabled you will see time measurements for profiled functions.

Note
> If you don't see any time measurements you might need to refresh the page, then wait 5-10 seconds and try again if nothing shows up.

In the browser page near the text "Main Thread" (in "Sample Timeline" panel) you can click on buttons "+" and "-" to show/hide hierarchy (inner time measurements). You can also click on "Pause" button in the top-right corner to pause receiving of the new data. You can also expand a panel named "Main Thread" (usually in the bottom-right corner) to view hierarchy of calls you selected in "Sample Timeline" and their time measurements.

Note
> It's recommended to use profiler for a short amount of time to identify slow parts of your code because the profiler has proved to cause freezes at startup and sometimes memory leaks.

## Simulating input for automated tests

Your game has a `..._tests` target for automated testing (which relies on https://github.com/catchorg/Catch2) and generally it will be very useful to simulate user input. Here is an example on how to do that:

```Cpp
// Simulate input.
getWindow()->onKeyboardInput(KeyboardKey::KEY_A, KeyboardModifiers(0), true); // simulate keyboard "A" pressed
getWindow()->onKeyboardInput(KeyboardKey::KEY_A, KeyboardModifiers(0), false); // simulate keyboard "A" released
```

Once such function is called it will trigger register input bindings in your game instance and nodes.

There are also other `on...` function in `Window` that you might find handy in simulating user input.

For more examples see `src/engine_tests/game/node/Node.cpp`, there are some tests that simulate user input.

## Exporting your game

If you want to distribute a version of your game you need to switch to the `release` build mode in your IDE to switch CMake to `release` build configuration (make sure CMake is now using a `release` configuration). Then build your project as usual, it will take much longer since we have `clang-tidy` enabled for `release` builds. `clang-tidy` can fail the build if it finds some issues in your code. It's expected that you build your game in `release` mode regularly to fix `clang-tidy` warnings/errors (if there are any).

Once your project is built in `release` mode go to the directory where the root `CMakeLists.txt` file is located and directories like `ext` and `res`. Depending on your setup you need to open the directory where built binaries are located, this is typically `build` directory (or it may be called differently, for example `out`). Inside of this directory you will see a directory named `OUTPUT` - this is where all built CMake targets are located. Inside of the `OUTPUT` directory you should find a directory named after your game's target. Inside of that directory will be located the final executable (or it will be located in the directory named `Release`). If you would try to run that executable you will get an error saying that the `res` directory is not found. This is expected as there are some additional steps that you need to do.

After you have your built executable you might notice a file named something like `COPY_UPDATED_RES_DIRECTORY_HERE` next to your game's executable. This is a "reminder" file that you need to manually copy the `res` directory from the root of your project directory next to the executable. Note that you need to make an actual copy, don't make symlinks and don't cut-paste it, you need to make a copy. This copy is a snapshot of the game's resources for this specific game version. After you've copied the `res` directory you can remove the "reminder" file named `COPY_UPDATED_RES_DIRECTORY_HERE`. At this point if you run the executable your game should start. The only thing left to do is to remove non-game related files from this directory.

In order to remove non-game related files next to the built executable there will be a directory named `delete_nongame_files`, open it and you will find a Go script in it. Read the `README.md` file next to the script file and run the script. It will interactively ask you if you want to delete non-game related files and etc. Once the script is finished it will tell you that you can delete the directory with this script as it's no longer needed. You should now try starting your game using the built executable to see if everything works after all non-game related files were deleted.

As you might have noticed next to the built executable of your game there is an `ext` directory. This directory contains license files of all external dependencies that you have in your project's root `ext` directory, plus there is a copy of engine's license file. You are required to distribute this directory as part of your build - do not delete this directory. You don't need to list these licenses in your EULA or somewhere else, you just need to distribute them as part of your build - nothing more.

That's it! Your game is ready to be distributed. You can archive the directory with the built executable and send it to a friend or upload it on the Internet.

At the time of writing this there is no compression/encryption of the game's resources. All game's resources are distributed as-is.

# Regular reading

This part of the manual groups sections that you might want to re-read regularly in the future.

## Tips to note when working with nodes

Prefer to start your custom nodes like this:

```Cpp
MyDerivedNode() : MyDerivedNode("My Derived Node") {};
MyDerivedNode(const std::string& sNodeName) : ParentNodeClass(sNodeName) {
  // constructor logic
}
virtual ~MyDerivedNode() override = default;
```

If you override some `virtual` function in node it's very likely (read the documentation for the functions you are overriding) that **you need to call the parent's version**:

```Cpp
void FlyingCharacterNode::onChildNodesSpawned() {
    SpatialNode::onChildNodesSpawned(); // <- calling parent's version

    // ... your code ...
}
```

Don't forget about `Node::getChildNodeOfType` and `Node::getParentNodeOfType` as they might be useful.

**Remember that `World` is inaccessable when the node is not spawned and thus `Node::getWorldRootNode` is `nullptr`.**

If you have a character node with some child nodes and you want the player to explore the world by walking on his feet or by riding a car you can use `pCarNode->addChildNode(pCharacterNode)` to attach your already spawned player (which is attached to the world's root node) to the car node when the player gets in the car or detach it from the car by using something like `getWorldRootNode()->addChildNode(pCharacterNode)` to make it attached to the world's root node again. By using `addChildNode` you can not only add child nodes but also attach and detach existing ones even if they already have a parent.

The order in which `Node::onBeforeNewFrame` are called on nodes is kind of random. If you need a specific node's `onBeforeNewFrame` to be called before `onBeforeNewFrame` of some other node consider using `Node::setTickGroup`. For example if your game have a functionality to focus the camera on some world entity you might want to put the "focusing" logic in the later tick group to make sure that all world entities processed their movement before you rotate (focus) the camera.

## Important things to keep in mind

This section contains a list of important things that you need to regularly check while developing a game to minimize the amount of bugs/crashes in your game. All information listed below is documented in the manual and in the engine code (just duplicating the most important things here, see more details in other sections of the manual or in the engine code documentation).

### General

- always read the documentation for the functions you are using (documentation comments in the code), this generally saves you from all issues listed here
- always check the logs, if something goes wrong the engine will let you know in the logs, after your game is closed if there were any warnings/errors the last message in the log (before the application is closed) will be the total number of warnings/errors produced (if there were any) so you don't have to scroll through the logs or use search every time
- from time to time check your console output's beginning when you start your game for special warnings like `[Refureku] WARNING: Double registration detected` which are not captured by our logging system, these might occur in very special cases, report these if found

### Garbage collection and GC pointers

- never capture `gc` pointers in `std::function`
- never store `gc` pointers in STL containers, store `gc` pointers only in "gc containers"

### Multiple inheritance

- using `gc` pointers on types that use multiple inheritance is not supported and will cause exceptions, leaks and crashes
- if you use multiple inheritance with serialization (not using `gc` pointers), make sure to derive from the `Serializable` class (or derived, for ex. `Node`) first and only then from other non `Serializable` classes (order matters, otherwise garbage data will be serialized instead of the actual data)

### Serialization/deserialization

- initialize almost all `RPROPERTY(Serialize)` fields, for example:
```Cpp
// MyHeader.h
UPROPERTY(Serialize)
long long iCharacterLevel = 0; // initialize because it can get garbage value

UPROPERTY(Serialize)
std::string sMyText; // `std::string` can't get garbage value since it's initialized in std::string's constructor
```
otherwise you might serialize a garbage value and then deserialize it as a garbage value which might cause unexpected reaction

### Node

- always check `getWorldRootNode` for `nullptr` before using it, `nullptr` here typically means that the node is not spawned or the world is being destroyed
- always remember to call parent's virtual function in the beginning of your override (note that this is not always needed but is required for some `Node` functions)
- use `NodeFunction` instead of `std::function` when you need callbacks in nodes
- use `NodeNotificationBroadcaster` when you need publisher-subscriber pattern
- it's highly recommended to not do any logic if the node is not spawned to avoid running into `nullptr`s (or deleted memory when on a non-main thread, use `getSpawnDespawnMutex()` mutex for non-main thread functions)

### Multithreading

- when using thread pool tasks remember that all thread pool tasks will be stopped during the game destruction before GameInstance is destroyed, there are no other checks for thread pool tasks to be stopped so in other cases you need to make sure your async task will not hit deleted memory:
  - if you're using `Node` member functions in async task make sure the task is finished in your Node::onDespawning,
  - if you're using `GameInstance` member functions in async task you only might care about world being changed, for this use promise/future or something else to guarantee that the callback won't be called on a deleted object
- use `NodeFunction` instead of `std::function` when you need to process asynchronous results in nodes
- submitting a deferred task from a **non-main thread** where in deferred task you operate on a `gc` controlled object such as Node can be dangerous as you may operate on a deleted (freed) memory, in this case use additional checks in the beginning of your deferred task to check if the node you want to use is still valid:
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

# Advanced topics

## Writing custom shaders

### Introduction

This section expects that you have knowledge in writing programs in HLSL and/or GLSL.

Note: currently we are looking for a solution that will make writing custom shaders easier but right now writing custom shaders is not that simple:
> Right now if you want to go beyond what Material provides to you and achieve some special look of your meshes you would have to write shaders in both HLSL and GLSL if you want your game to support both DirectX and Vulkan renderers that we have because each graphics API (like DirectX or Vulkan) has its own shading language. If you know that you don't want Vulkan support and don't care about Linux and other non-Windows platforms then you might just write a shader in HLSL and ignore GLSL, this would mean that any attempt to run your game using Vulkan renderer will fail with an error.

### Shader formatter

Similar to `clang-format` we use a special formatter for shaders: https://github.com/Flone-dnb/vscode-shader-formatter

Make sure you use it when writing shaders.

### Writing custom vertex/pixel/fragment shaders

We will talk about creating a custom pixel/fragment shader but the same idea applies to creating custom vertex shaders. Here are the steps to create a new custom shader:

1. Create a new shader file somewhere in the `res` directory, for example: `res/game/shaders/hlsl/CustomMeshNode.frag.hlsl` or in `glsl` directory with `.frag.glsl` extension if you want to create a GLSL shader.
2. `#include` an engine shader file that your shader "derives" from. For example if you want to create a custom shader for `MeshNode` you need to include `MeshNode.frag.hlsl`. For example: `#include "../../../engine/shaders/hlsl/include/MeshNode.frag.hlsl"`.
    2. 1. For GLSL you need to include `#include "../../../engine/shaders/glsl/include/MeshNode.frag.glsl"`.
3. Define pixel shader function, you can copy-paste their signature from the included engine shader file, it may be named as `psMeshNode` or `fsMeshNode`.
4. As the first line of your shader function, call a function from the included engine shader, again it may be named as `psMeshNode` or `fsMeshNode`, and pass any input parameters if your function has them.
5. Modify resulting data as you want.

In order to compile your shader you need to use the `ShaderManager` object, here is an example on how to do it using HLSL shaders:

```Cpp
#include "shader/ShaderManager.h"
#include "shader/general/EngineShaders.hpp"

using namespace ne;

void MyGameInstance::onGameStarted(){
    const auto pixelShader = ShaderDescription(
        "game.meshnode.fs",                               // global unique shader name
        "res/game/shaders/hlsl/CustomMeshNode.frag.hlsl", // path to shader file, using custom pixel shader
        ShaderType::FRAGMENT_SHADER,                      // shader type
        VertexFormat::MESH_NODE,                          // vertex structure layout
        "fsCustomMeshNode",                               // shader entry function name
        EngineShaders::MeshNode::getFragmentShader(false) // macros: since we are "deriving" from MeshNode shader we use
            .definedShaderMacros);                        // engine shader's macros (but we can also add our macros)
        // (you don't need to define all macros that engine shader files use (like PS_USE_DIFFUSE_TEXTURE) because they
        // will be automatically defined by the engine when needed)

    std::vector vShaders = {pixelShader};

    const auto onProgress = [](size_t iCompiledShaderCount, size_t iTotalShadersToCompile) {
        // show progress here
    };
    const auto onError = [](ShaderDescription shaderDescription, std::variant<std::string, Error> errorInfo) {
        if (std::holds_alternative<std::string>(errorInfo)){
            // shader compilation error
        }else{
            // internal error
        }
    };
    const auto onCompleted = []() {
        // do final logic here
    };

    // shader compilation is done asynchronously by using engine's thread pool
    getWindow()->getRenderer()->getShaderManager()->compileShaders(
        vShaders,
        onProgress,
        onError,
        onCompleted
    );
}
```

For HLSL you would do the same thing (`ShaderType::FRAGMENT_SHADER` is considered as "pixel shader" when compiling HLSL shaders).

You should not remove the code to compile your shaders (`ShaderManager::compileShaders`) from your game. This code not only compiles the shaders but also adds them to the global "shader registry". If some shader was previously compiled then this means that the results of that compilation were cached and the next time you will call `compileShaders` instead of compiling it again the results will be retrived from the cache. If you change your shader code or something else the cache might be automatically invalidated (inside `ShaderManager::compileShaders`) and your shader will be automatically recompiled so if you do any changes in the shader file (or in any files that your shader includes) you just need to restart the game to see your changes.

Please note:
> If you got an idea of displaying a splash screen using a separate `GameInstance` (before starting your game's `GameInstance`) in order to compile your shaders inside of that splash screen game instance it would be a bad idea because `compileShaders` will be called twice (inside of your splash screen game instance and inside of your game's game instance) which means that even if no shader was changed the shader cache will be checked twice which might take some time if you have lots of shaders.

As you might have noticed in the `res/engine/shaders/include` directory there are `.glsl` shaders outside of the `glsl`/`hlsl` directory. These shaders contain code that can be used in both HLSL and GLSL. Before passing shader code to a shader compiler we parse the code from disk using a special but simple parser (see https://github.com/Flone-dnb/combined-shader-language-parser). It allows mixing HLSL and GLSL code. You can also use such functionality and have just one shader file instead of separate HLSL and GLSL files if you want.

### Writing custom compute shaders

Shader compilation for compute shaders is the same as from the previous section that described custom vertex/pixel/fragment shaders. What's different is how we interact with compute shaders.

After you have compiled your compute shader you need to create a special "interface" object to interact with your compute shader (specify input/output resources, dispatch it and etc.). Let's consider an example where you want to calculate some data (stored in a resource) that will be used during the rendering:

```Cpp
#include "shader/ComputeShaderInterface.h"

void MyGameInstance::onGameStarted(){
    // ... compile compute shader ...
    // on compilation finished:

    // Create shader interface.
    auto computeInterfaceResult = ComputeShaderInterface::createUsingGraphicsQueue(
        getWindow()->getRenderer(),
        "my.compute.shader.name", 
        ComputeExecutionStage::AFTER_DEPTH_PREPASS);
    if (std::holds_alternative<Error>(computeInterfaceResult)) {
        // ... handle error ...
    }
    pComputeShaderInterface = std::get<std::unique_ptr<ComputeShaderInterface>>(std::move(computeInterfaceResult));

    // Create a resource that we will use in compute shader.
    auto resourceCreationResult = getWindow()->getRenderer()->getResourceManager()->createResourceWithData(...);
    if (std::holds_alternative<Error>(resourceCreationResult)) {
        // ... handle error ...
    }
    auto pComputeResource = std::get<std::unique_ptr<GpuResource>>(std::move(resourceCreationResult));

    // Bind resource to be available in compute shader.
    optionalError = pComputeShaderInterface->bindResource(
        pComputeResource.get(), "result", ComputeResourceUsage::READ_WRITE_ARRAY_BUFFER);
    if (optionalError.has_value()) {
        // ... handle error ...
    }

    // Submit our shader.
    pComputeShaderInterface->submitForExecution(1, 2, 3);
}
```

Here our shader will be run only once before the first frame is rendered, if you want to regularly run your shader you just need to re`submitForExecution` it in your `onBeforeNewFrame` (for example). For more information, see function documentation.

### Shader packs and shader variants

If you looked in the engine shader files you might have noticed that some parts of the shader code are used only when a specific macro is defined (for example `#ifdef PS_USE_DIFFUSE_TEXTURE`). This is how engine shaders do branching (mostly), so instead of doing an actual runtime `if` the engine shader rely on predefined macros because runtime branching on GPUs can cause performance issues.

When you or the engine submits a shader to be compiled the engine creates a special object `ShaderPack`. Then depending on the shader type (vertex/pixel/fragment/compute/etc) the engine retrieves a special collection of compatible macro combinations, for pixel/fragment shader these combinations may be:

- [] (no macros),
- [PS_USE_DIFFUSE_TEXTURE] (opaque material with diffuse texture set),
- [PS_USE_DIFFUSE_TEXTURE, PS_USE_MATERIAL_TRANSPARENCY] (transparent material with diffuse texture set),
- [PS_USE_MATERIAL_TRANSPARENCY] (transparent material without diffuse texture),
- etc.

For every combination of macros the engine compiles one shader variant with only specific macros defined and then stores all shader variants in the shader pack object. Shader pack is then saved on the disk (cached) to be used on the next startup (so that the engine will just read shader bytecode from disk instead of compiling the shaders again). You can see information about all compiled shaders and their variants if you look in the following directory:

- [Windows] %localappdata%/nameless-engine/*targetname*/shader_cache
- [Linux] ~/.config/nameless-engine/*targetname*/shader_cache

In the shader cache directory you will find one directory per shader. Inside of the shader specific directory you will find multiple files but you should focus on the files with the `.toml` extension. Each TOML file describes one shader variant and if you open that TOML file in your text editor you might learn some information about a shader (like which macros were defined and etc).

At runtime when, for example, some material is created it requests a pair of vertex and pixel/fragment shaders (it actually requests a graphics pipeline but we will omit this for simplicity). The engine then asks the renderer on which macros should be defined right now (depending on the current render settings) and plus the material also tells which macros it defines (for example `PS_USE_MATERIAL_TRANSPARENCY` when transparency is enabled), then `ShaderManager` looks for a shader pack for the specified shaders and returns a specific shader from the pack that corresponds with the requested macros. This is how a material receives its shader. If material changes its settings (like transparency) or something global (like render settings) is changed, if there is a shader macro that should be added/removed due to these changes, materials' shaders are being changed by getting another shader variant from the shader pack.

This is why you should not define some shader macros that are used in the engine shader files as they will be "defined" automatically when needed.

### Debugging custom shaders

You can use your usual shader debugging software (`PIX`, `RenderDoc`, `NVIDIA Nsight`, etc.) to debug your custom shaders. Just make sure your game is built in the `Debug` mode.

### Using custom shader resources

#### Introduction to using custom resources

Let's consider a simple example of passing a buffer from C++ into your custom shader which looks like this:

```HLSL
#include "../../../engine/shaders/hlsl/include/MeshNode.frag.hlsl"

cbuffer customData : register(b1) // register index/space can be different (as long as no other resource is using it)
{
    float4x4 someMatrix;
};

// your HLSL code
```

```GLSL
#include "../../../engine/shaders/glsl/include/MeshNode.frag.glsl"

struct CustomData {
    mat4 someMatrix; 
};
layout(std140, binding = 9) readonly buffer CustomDataBuffer{ // binding index can be different (same as in HLSL)
    CustomData array[];  // stores data for all objects (meshes) that use this shader
} customData;

#additional_shader_constants
{
	uint customData; // named as your `readonly buffer`, case sensitive,
                     // used to access element specific to the current object (mesh)
}

void main() {
    fsMeshNode(); // call "parent" function

    // Define a short macro for simplicity.
    #define MY_DATA customData.array[arrayIndices.customData]

    // Access our matrix.
    glm::mat4x4 myMatrix = MY_DATA.someMatrix;
}
```

Note
> If you want to use the same shader resource in both vertex and pixel/fragment shaders make sure that resource is using the same binding register/index in both shaders, when the engine finds a shader resource that was specified in both vertex and pixel/fragment shaders with the same name and the same binding register/index it will understand that it's the same resource and will avoid double-registration of the resource. It's recommended to move that resource's definition into a separate file and then include that file in your vertex and pixel/fragment shaders to guarantee that resource's binding register/index is the same. Engine shaders are using the same approach.

Then in C++:

```Cpp
#include "shader/VulkanAlignmentConstants.hpp"
#include "math/GLMath.hpp"

using namespace ne;

struct CustomMeshShaderConstants {
    alignas(iVkMat4Alignment) glm::mat4x4 someMatrix = glm::identity<glm::mat4x4>();
};

// using mutex for example purposes, you are not required to use a mutex here
std::pair<std::recursive_mutex, CustomMeshShaderConstants> mtxShaderData;
```

As you can see we use `alignas` to satisfy Vulkan aligning requirements and at the same time keep track of HLSL packing rules. If you only want to stick with some specific shading language (only GLSL or only HLSL) then you just need to keep track of your language specific packing rules.

Note that if you don't find a `iVk...Alignment` variable matching your type's name this means that you should avoid using this type, this includes types such as `vec3` and `mat3`, instead use `vec4` and `mat4` so you will avoid a bunch of alignment/packing issues.

Generally if you specify `alignas` to all fields (of a type that will be directly copied to the GPU) you should be pretty safe in terms of both Vulkan alignment requirements and HLSL packing rules.

In most cases there are only 2 things that you need to keep track of:

1. Order matters, that is the padding that `alignas` might introduce, for example:

```Cpp
struct CustomMeshShaderConstants {
    alignas(iVkScalarAlignment) float test = 0.0F;    // has 12 bytes of padding to satisfy next `iVkMat4Alignment`

    alignas(iVkMat4Alignment) glm::mat4x4 somematrix = glm::identity<glm::mat4x4>();
};
```

Note
> If you are using Qt Creator IDE you can see field alignment (plus padding if there is one) by hovering your mouse cursor over a field's name, which is very useful for such cases.

In order to avoid this you might want to prefer to put "big types" (types with bigger alignment such as `mat`s and `vec`s) first and only then "small types" (such as `float`s and etc). Otherwise you might have lots of unused padding bytes that might bloat your data.

2. Extra padding on the last field might cause alignment problems in HLSL `StructuredBuffer`s:

```Cpp
struct ShaderConstants {
    alignas(iVkVec4Alignment) glm::vec4 color = glm::vec4(1.0F, 1.0F, 1.0F, 1.0F);

    alignas(iVkScalarAlignment) float somevar = 0.0F;

    // 12 bytes of padding here
};
```

HLSL structured buffer will look like this:

```HLSL
struct MyData {
    vec4 color;
    
    float somevar;
};

StructuredBuffer<MyData> myData : register(t0);
```

If you store more than 1 element in this structured buffer your second element will be aligned incorrectly because in C++ you have that 12 bytes of padding at the end but HLSL `StructuredBuffer`s are tightly-packed and there's no 12 bytes of padding in the end so your second element in the structured buffer will reference padding bytes in its `color` field.

In order to avoid such issues just add explicit padding like `float pad[3]` and `vec3 pad` in C++ and HLSL.

Now let's tell the engine how to pass your buffer to shaders:

```Cpp
void CustomMeshNode::onSpawning(){
    SpatialNode::onSpawning();

    setShaderCpuWriteResourceBinding(       // call this function only in `onSpawning`, see function docs
        "customData",                       // name of the resource written in your shader file (HLSL/GLSL)
        sizeof(CustomMeshShaderConstants),  // size of your buffer
        [this]() -> void* { return onStartedUpdatingShaderConstants(); },
        [this]() { onFinishedUpdatingShaderConstants(); }
    );
}
```

Then implement "updating" functions that will be automatically called by the engine when it needs to copy our data from RAM to VRAM:

```Cpp
void* CustomMeshNode::onStartedUpdatingShaderConstants() {
    mtxShaderData.first.lock();  // don't unlock yet
    return &mtxShaderData.second;
}

void CustomMeshNode::onFinishedUpdatingShaderConstants() {
    mtxShaderData.first.unlock(); // copying is finished
}
```

Then in any place of your custom mesh node (even when it's not spawned yet) when you need to copy your data to shaders:

```Cpp
void CustomMeshNode::onSomeEvent() {
    // Update our data.
    std::scoped_lock guard(mtxShaderData.first);
    mtxShaderData.second.somematrix = getSomeMatrix();

    // Mark data to be copied to the GPU.
    markShaderCpuWriteResourceToBeCopiedToGpu("customData");
}
```

`markShaderCpuWriteResourceToBeCopiedToGpu` will notify the engine if your node is spawned, otherwise it won't do anything so that your "update" functions will only be called while your node is spawned. After the engine was notified it will mark that resource as "needs update" and call your "update" functions before the next frame is submitted to be rendered (when the engine will be ready to update GPU resources).

If you assigned your custom shaders to the material of your `CustomMeshNode` then we don't need to do anything else.

#### Using custom textures

Similar approach is used for custom textures. First, define a shader:

```GLSL
// ... some code here ...

#hlsl Texture2D customTexture : register(t5, space5);
#glsl layout(binding = 8) uniform sampler2D customTexture[];

#additional_shader_constants{
    uint customTexture; // access it using `constants.customTexture`
}

void main(){
... some code here ...

#hlsl float4 color = customTexture.Sample(textureSampler, pin.uv);
#glsl vec4 color   = texture(customTexture[constants.customTexture], fragmentUv);

... some code here ...
}
```

Then assign a material that uses this pixel/fragment shader to your custom mesh node (use default vertex shader). Now, import a texture, use can use the editor or pure C++ for this step:

```Cpp
#include "material/TextureManager.h"

auto optionalError = TextureManager::importTexture(
    "C:\\images\\stone.png", TextureType::DIFFUSE, "game", "stone", importTextureProgress);
if (optionalError.has_value()) [[unlikely]] {
    // ... handle error ...
}

// `importTextureProgress` signature and definition is skipped to simplify the example
```

Once you have a texture in your `res` directory you need to bind the file to the shader:

```Cpp
void MyMeshNode::onSpawning(){
    MeshNode::onSpawning();

    setShaderTextureResourceBinding(    // call this function only in `onSpawning`, see function docs
        "customTexture",                // name of the resource
        "game/stone"                    // path relative `res`
    );
}
```

Now your mesh's pixel/fragment shader will have a custom texture binded.

## Adding support for new types for serialization/deserialization

When you use `Serializable::serialize` function all fields marked with `RPROPERTY(Serialize)` will be checked for serialization. Serialization for fields is achieved by implementing the `IFieldSerializer` interface. Thanks to field serializers you can, for example, serialize fields that have primitive types (bool, int, long long, etc.) because there is a `PrimitiveFieldSerializer` that implements `IFieldSerializer`. Field serializers are located in the `io/serializers` directory, you can look how they are implemented. Back to the beginning, when you use `serialize` function the engine goes through each reflected field marked with `Serialize` property and basically does this:

```Cpp
for (const auto& pSerializer : vFieldSerializers) {
    if (pSerializer->isFieldTypeSupported(&field)) {
        pSerializer->serializeField(&field);
        break;
    }
}
```

So if you want to add support for a new field type for serialization, you just need to implement `IFieldSerializer` interface and register your serializer like so:

```Cpp
#include "io/FieldSerializerManager.h"

FieldSerializerManager::addFieldSerializer(std::make_unique<MyFieldSerializer>());
```

After this, when you use `serialize` functions your serializer will be used.

In addition to the usual `IFieldSerializer` serializers we also have `IBinaryFieldSerializer` interface that works the same way but used for fields marked as `FST_AS_EXTERNAL_BINARY_FILE` and serializes into a binary file for smaller file size and faster deserialization.

## Creating new reflection properties

You can create custom reflection properties like `Guid` or `Serialize` that we currently have. All engine reflection properties are located at `io/properties` so you can look at them to see an example.

You can find instructions for creating custom properties here: https://github.com/jsoysouvanh/Refureku/wiki/Create-custom-properties

## Serialization internals

Note:
> Information described in this section might not be up to date, please notify the developers if something is not right / outdated.

### General overview

Most of the game assets are stored in the human-readable `TOML` format. This format is similar to `INI` format but has more features. This means that you can use any text editor to view or edit your asset files if you need to.

When you serialize a serializable object (an object that derives from `Serializable`) the general TOML structure will look like this (comments start with #):

```INI
## <unique_id> is an integer, used to globally differentiate objects in the file
## (in case they have the same type (same GUID)), if you are serializing only 1 object the ID is 0 by default
["<unique_id>.<type_guid>"]       ## section that describes an object with GUID
<field_name> = <field_value>      ## not all fields will have their values stored like that
<field_name> = <field_value>


["<unique_id>.<type_guid>"]       ## some other object
<field_name> = <field_value>      ## some other field
"..parent_node_id" = <unique_id>  ## keys that start with two dots are "custom attributes" (user-specified)
                                  ## that you pass into `serialize`, they are used to store additional info
<field_name> = "reflected type, see other sub-section"  ## this field derives from `Serializable` and so we store
                                                        ## its data in a separate section


["<unique_parent_object_id>.<unique_subobject_id>.<type_guid>"] ## section of an object
                                                                ## <unique_subobject_id> is unique only within
                                                                ## the parent object
".field_name" = <field_name>      ## keys that start with one dot are "internal attributes" they are used for storing 
                                  ## internal info, in this case this key describes the name of the field this section 
                                  ## represents in `<unique_parent_object_id>` object
```

Here is a more specific example:

```INI
["0.2a721c37-3c22-450c-8dad-7b6985cbbd61"]
sName = "Node"

["1.550ea9f9-dd8a-4089-a717-0fe4e351a687"]
bBoolValue2 = true
"..parent_node_id" = "0"                              ## means that this section describes a node that has
                                                      ## a parent node
entity = "reflected type, see other sub-section"
".path_relative_to_res" = "test/custom_node.toml"     ## means that this section contains only changed fields compared
                                                      ## to the specified file (original entity)

["1.0.550ea9f9-dd8a-4089-a717-0fe4e351a686"]
iIntValue2 = 42
".field_name" = "entity"
vVectorValue2 = [
"Hello",
"World",
]
```

### Storing only changed fields

Some `Serializable::serialize` overloads allow you to specify `Serializable* pOriginalObject`, when such object is specified each field of the object that is being serialized will be compared to this original object and only fields that were changed compared to the original object will be serialized. To keep the information about all other fields in this case we add an internal attribute like so:

```INI
["1.550ea9f9-dd8a-4089-a717-0fe4e351a687"]
bBoolValue2 = true
".path_relative_to_res" = "test/custom_node.toml"
```

This attribute points to a file located in the `res` directory, specifically at `res/test/custom_node.toml`. This all will work only if the original object was previously deserialized from a file located in the `res` directory (see `Serializable::getPathDeserializedFromRelativeToRes`).

### Referencing external node tree

If your node tree uses another (external) node tree that is located in the `res` directory, this external node tree is saved in a special way, that is, only the root node of the external node tree is saved and information about all child nodes is stored as a path to the file that contains this node tree.

This means that when we reference an external node tree, only changes to external node tree's root node will be saved.