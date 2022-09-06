# nameless-engine

TODO: screenshot game/editor

### Supported platforms

* Windows
* Linux

# Features

- Automatic shader caching and cache invalidation.

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

- TODO.

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