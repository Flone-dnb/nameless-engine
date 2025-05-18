/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Nameless Engine", "index.html", [
    [ "Manual", "index.html", [
      [ "Introduction to the engine", "index.html#autotoc_md1", [
        [ "Prerequisites", "index.html#autotoc_md2", null ],
        [ "Creating a new project", "index.html#autotoc_md3", [
          [ "Preparing project path (only for Windows users)", "index.html#autotoc_md4", null ],
          [ "Project generator", "index.html#autotoc_md5", null ],
          [ "Project structure", "index.html#autotoc_md6", null ]
        ] ],
        [ "Which header files to include and which not to include", "index.html#autotoc_md7", [
          [ "General", "index.html#autotoc_md8", null ],
          [ "Math headers", "index.html#autotoc_md9", null ]
        ] ],
        [ "Automatic code formatters and static analyzers", "index.html#autotoc_md10", null ],
        [ "Node system", "index.html#autotoc_md11", null ],
        [ "Game instance", "index.html#autotoc_md12", null ],
        [ "Reflection basics", "index.html#autotoc_md13", [
          [ "General", "index.html#autotoc_md14", null ],
          [ "Using reflection for your new type", "index.html#autotoc_md15", null ],
          [ "Common mistakes when using reflection", "index.html#autotoc_md16", null ]
        ] ],
        [ "Error handling", "index.html#autotoc_md17", null ],
        [ "Logging", "index.html#autotoc_md18", null ],
        [ "Project paths", "index.html#autotoc_md19", null ],
        [ "Memory leaks and garbage collection", "index.html#autotoc_md20", [
          [ "Memory leak checks", "index.html#autotoc_md21", null ],
          [ "About raw pointers", "index.html#autotoc_md22", null ],
          [ "Garbage collector overview", "index.html#autotoc_md23", null ],
          [ "Storing GC pointers in containers", "index.html#autotoc_md24", null ],
          [ "GC limitations and thread safety with examples", "index.html#autotoc_md25", null ],
          [ "Interacting with garbage collector", "index.html#autotoc_md26", null ]
        ] ],
        [ "Deferred tasks and thread pool", "index.html#autotoc_md27", null ],
        [ "World creation", "index.html#autotoc_md28", [
          [ "World axes and world units", "index.html#autotoc_md29", null ],
          [ "Creating a world using C++", "index.html#autotoc_md30", null ]
        ] ],
        [ "Lighting", "index.html#autotoc_md31", null ],
        [ "Environment", "index.html#autotoc_md32", null ],
        [ "Creating a character and working with input", "index.html#autotoc_md33", [
          [ "Creating a simple flying character node", "index.html#autotoc_md34", null ],
          [ "Working with user input", "index.html#autotoc_md35", [
            [ "Input events", "index.html#autotoc_md36", null ],
            [ "Handling input event IDs", "index.html#autotoc_md37", null ],
            [ "Binding to input events in game instance", "index.html#autotoc_md38", null ],
            [ "Binding to input events in nodes", "index.html#autotoc_md39", null ]
          ] ]
        ] ],
        [ "Working with timers", "index.html#autotoc_md40", [
          [ "Timers and GameInstance", "index.html#autotoc_md41", null ],
          [ "Timers and Node", "index.html#autotoc_md42", null ]
        ] ],
        [ "Node callbacks (including asynchronous tasks)", "index.html#autotoc_md43", null ],
        [ "Safely using the publisher-subscriber pattern with nodes", "index.html#autotoc_md44", null ],
        [ "Saving and loading user's input settings", "index.html#autotoc_md45", null ],
        [ "Configuring graphics settings", "index.html#autotoc_md46", null ],
        [ "Saving and loading user's progress data", "index.html#autotoc_md47", [
          [ "Overview", "index.html#autotoc_md48", null ],
          [ "Saving and loading data using reflection", "index.html#autotoc_md49", [
            [ "Overview", "index.html#autotoc_md50", null ],
            [ "Reflection limitations", "index.html#autotoc_md51", null ]
          ] ],
          [ "Saving and loading data using ConfigManager", "index.html#autotoc_md52", null ]
        ] ],
        [ "Adding external dependencies", "index.html#autotoc_md53", null ],
        [ "Importing custom meshes", "index.html#autotoc_md54", [
          [ "Configuring export settings for your mesh in Blender", "index.html#autotoc_md55", null ]
        ] ],
        [ "Procedurally generating meshes", "index.html#autotoc_md56", null ],
        [ "Working with materials", "index.html#autotoc_md57", [
          [ "Working with materials in C++", "index.html#autotoc_md58", null ]
        ] ],
        [ "Render statistics", "index.html#autotoc_md59", null ],
        [ "Using profiler", "index.html#autotoc_md60", null ],
        [ "Simulating input for automated tests", "index.html#autotoc_md61", null ],
        [ "Exporting your game", "index.html#autotoc_md62", null ]
      ] ],
      [ "Regular reading", "index.html#autotoc_md63", [
        [ "Tips to note when working with nodes", "index.html#autotoc_md64", null ],
        [ "Important things to keep in mind", "index.html#autotoc_md65", [
          [ "General", "index.html#autotoc_md66", null ],
          [ "Garbage collection and GC pointers", "index.html#autotoc_md67", null ],
          [ "Multiple inheritance", "index.html#autotoc_md68", null ],
          [ "Serialization/deserialization", "index.html#autotoc_md69", null ],
          [ "Node", "index.html#autotoc_md70", null ],
          [ "Multithreading", "index.html#autotoc_md71", null ]
        ] ]
      ] ],
      [ "Advanced topics", "index.html#autotoc_md72", [
        [ "Writing custom shaders", "index.html#autotoc_md73", [
          [ "Introduction", "index.html#autotoc_md74", null ],
          [ "Shader formatter", "index.html#autotoc_md75", null ],
          [ "Writing custom vertex/pixel/fragment shaders", "index.html#autotoc_md76", null ],
          [ "Writing custom compute shaders", "index.html#autotoc_md77", null ],
          [ "Shader packs and shader variants", "index.html#autotoc_md78", null ],
          [ "Debugging custom shaders", "index.html#autotoc_md79", null ],
          [ "Using custom shader resources", "index.html#autotoc_md80", [
            [ "Introduction to using custom resources", "index.html#autotoc_md81", null ],
            [ "Using custom textures", "index.html#autotoc_md82", null ]
          ] ]
        ] ],
        [ "Adding support for new types for serialization/deserialization", "index.html#autotoc_md83", null ],
        [ "Creating new reflection properties", "index.html#autotoc_md84", null ],
        [ "Serialization internals", "index.html#autotoc_md85", [
          [ "General overview", "index.html#autotoc_md86", null ],
          [ "Storing only changed fields", "index.html#autotoc_md87", null ],
          [ "Referencing external node tree", "index.html#autotoc_md88", null ]
        ] ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Related Functions", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"AABB_8h_source.html",
"classne_1_1ConfigManager.html#a23d1dee3702c1138f50e19ec07dea40a",
"classne_1_1DirectionalLightNode.html#a5371921a835741ed94bc0c823e8f5424",
"classne_1_1GlobalShaderResourceBinding.html#ac41bfc2a986ce605df3dc1f72a7e4c14",
"classne_1_1LightingShaderResourceManager.html#a5ef0adb2877a35e64a6a8b6e9a69370a",
"classne_1_1Node.html#acc4ff3174738477037de125dff1a0707",
"classne_1_1Renderer.html#a19bc0b11a97b3e92b8510f348d0fa28f",
"classne_1_1ShaderLightArray.html#aae70b6d70f55db9c6a6c7e6a4e79813c",
"classne_1_1Timer.html#a1e4b01f28b5d3436354dd5b382f273c3",
"classne_1_1VulkanShadowMapArrayIndexManager.html#a41a7fa9b722b6108ff567975febb1a24",
"index.html#autotoc_md14",
"structne_1_1Frustum.html#abd4917883366761005bf6538fa775279",
"structne_1_1ShaderCacheInvalidationReasonDescription.html#a300c8dbf4348ff61b07d83da04191055"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';