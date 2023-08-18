# File that should be included by all engine-related `CMakeLists` files.

# Set default build mode to be `Release`.
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Using build mode '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
endif()
message(STATUS "${PROJECT_NAME}: build type: ${CMAKE_BUILD_TYPE}.")

# Prepare variable for build type.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(IS_RELEASE_BUILD 0)
else()
    set(IS_RELEASE_BUILD 1)
endif()

# Add `DEBUG` macro in debug builds.
if(NOT IS_RELEASE_BUILD)
    message(STATUS "${PROJECT_NAME}: adding DEBUG macro for this build type.")
    add_compile_definitions(DEBUG)
endif()

# Add `WIN32` and `_WIN32` macros on Windows (some setups don't define them).
if(WIN32)
    add_compile_definitions(WIN32)
    add_compile_definitions(_WIN32)
endif()

# Set directory name for dependencies.
set(DEPENDENCY_BUILD_DIR_NAME dep)

# Enable cmake folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Define some folder names.
set(ENGINE_FOLDER "NAMELESS-ENGINE")
set(EXTERNAL_FOLDER "EXTERNAL")

# Define some names.
set(POST_BUILD_SCRIPT_NAME "post_build")
set(DELETE_NONGAME_FILES_SCRIPT_NAME "delete_nongame_files")

# Refureku related stuff: ------------------------------------------------------------------------------------
set(GENERATED_DIR_NAME ".generated")
set(REFUREKU_SETTINGS_FILE_NAME "RefurekuSettings.toml")

# Adds Refureku (reflection generator) as a PRE_BUILD dependency to your `${PROJECT_NAME}` target.
#
# Arguments:
# - TARGET_CMAKE_DIR                - value of ${CMAKE_CURRENT_SOURCE_DIR}. This directory is assumed to also be the root directory
# for your source code, all files (from all subdirectories) will be recursively analyzed by the reflection generator.
# - EXCLUDED_FROM_ANALYZING         - a list of files that are located in the directory with the source code but should be excluded
# from analyzing, this is typically used for platform specific source files, for example if you are building on Linux
# you want to exclude Win32 sources.
# - REFUREKU_EXT_PATH               - absolute path to the directory `ext/Refureku`.
# - ENGINE_DEPENDENCY_TARGET        - optional (can be empty), if your target depends on some other engine-related target
# (for example if your `game_lib` target depends on the `engine_lib` target), specify name of the engine-related target
# you depend on. This will result in all specified Refureku included directories, of the target you depend on, to be
# appended to your specified included directories so that Refureku can find all includes.
# - INCLUDED_DIRECTORIES            - a list of all included directories (including `include`s of all other
# non-engine related dependencies (if you, for example, depend on `engine_lib` then just specify path to its ".generated"
# directory as other argument of this function instead of writing all includes of `engine_lib`)),
# this list typically contains a list of paths to "include" directories of external dependencies plus path to
# included directories of your target.
function(add_refureku TARGET_CMAKE_DIR EXCLUDED_FROM_ANALYZING REFUREKU_EXT_PATH ENGINE_DEPENDENCY_TARGET INCLUDED_DIRECTORIES)
    message(STATUS "${PROJECT_NAME}: adding external dependency \"Refureku\" (reflection generator)...")

    # Make sure the path to the Refureku directory exists.
    if (NOT EXISTS ${REFUREKU_EXT_PATH})
         message(FATAL_ERROR "${PROJECT_NAME}: path to Refureku directory ${REFUREKU_EXT_PATH} does not exist.")
    endif()

    # Make sure the script path exists.
    set(SCRIPT_PATH ${REFUREKU_EXT_PATH}/download_and_setup_refureku.go)
    if (NOT EXISTS ${SCRIPT_PATH})
        message(FATAL_ERROR "${PROJECT_NAME}: path to the script file ${SCRIPT_PATH} does not exist.")
    endif()

    # Construct path to the ".generated" directory of the target we depend on.
    set(ENGINE_DEPENDENCY_GENERATED_DIR "")
    if (NOT "${ENGINE_DEPENDENCY_TARGET}" STREQUAL "")
        set(ENGINE_DEPENDENCY_GENERATED_DIR ${TARGET_CMAKE_DIR}/../${ENGINE_DEPENDENCY_TARGET}/${GENERATED_DIR_NAME})
        if (NOT EXISTS ${ENGINE_DEPENDENCY_GENERATED_DIR})
            message(FATAL_ERROR "${PROJECT_NAME}: expected the path ${ENGINE_DEPENDENCY_GENERATED_DIR} to the \".generated\" directory of the target we depend on to exist")
        endif()
    endif()

    # Append ".generated" directory to the include paths.
    set(INCLUDED_DIRECTORIES
        ${INCLUDED_DIRECTORIES}
        ${TARGET_CMAKE_DIR}/${GENERATED_DIR_NAME}
    )

    # Define some paths.
    set(GENERATED_SOURCE_DIR_PATH ${TARGET_CMAKE_DIR}/${GENERATED_DIR_NAME})
    set(REFUREKU_SETTINGS_FILE_PATH ${GENERATED_SOURCE_DIR_PATH}/${REFUREKU_SETTINGS_FILE_NAME})

    # Check that path to target's cmake file does not have pipe characters in it because we will use pipes to
    # separate include paths later.
    string(FIND ${TARGET_CMAKE_DIR} "|" PIPE_POS)
    if (NOT ${PIPE_POS} STREQUAL "-1")
        message(FATAL_ERROR "${PROJECT_NAME}: path ${TARGET_CMAKE_DIR} contains the character `|` which is used for internal purposes.")
    endif()

    # Merge included directories into one string while separating each filename with the pipe character "|".
    set(REFUREKU_INCLUDE_DIRECTORIES "")
    foreach(DIR_PATH IN LISTS INCLUDED_DIRECTORIES)
        string(FIND ${DIR_PATH} "|" PIPE_POS)
        if (NOT ${PIPE_POS} STREQUAL "-1")
            message(FATAL_ERROR "${PROJECT_NAME}: included directory path ${DIR_PATH} contains the character `|` which is used for internal purposes.")
        endif()
        set(REFUREKU_INCLUDE_DIRECTORIES ${DIR_PATH}|${REFUREKU_INCLUDE_DIRECTORIES})
    endforeach()

    # Merge excluded files into one string while separating each filename with the pipe character "|".
    set(REFUREKU_EXCLUDE_FILES "")
    foreach(SRC_PATH IN LISTS EXCLUDED_FROM_ANALYZING)
        string(FIND ${SRC_PATH} "|" PIPE_POS)
        if (NOT ${PIPE_POS} STREQUAL "-1")
            message(FATAL_ERROR "${PROJECT_NAME}: excluded file path ${SRC_PATH} contains the character `|` which is used for internal purposes.")
        endif()
        set(REFUREKU_EXCLUDE_FILES ${SRC_PATH}|${REFUREKU_EXCLUDE_FILES})
    endforeach()

    # Create the generated directory if not exists.
    if (NOT EXISTS ${GENERATED_SOURCE_DIR_PATH})
        file(MAKE_DIRECTORY ${GENERATED_SOURCE_DIR_PATH})
    endif()

    # Delete existing Refureku config file.
    message(STATUS "${PROJECT_NAME}: removing Refureku (reflection generator) config file to generate a fresh one...")
    file(REMOVE ${REFUREKU_SETTINGS_FILE_PATH})

    # Download Refureku and generate new config file.
    message(STATUS "${PROJECT_NAME}: downloading Refureku (reflection generator) and creating a new reflection generator config...")
    execute_process(
        WORKING_DIRECTORY ${REFUREKU_EXT_PATH}
        COMMAND go run ${SCRIPT_PATH}
        ${REFUREKU_EXT_PATH} # directory where the script is located
        ${TARGET_CMAKE_DIR}/ # directory where the source code is located
        "${ENGINE_DEPENDENCY_GENERATED_DIR}" # depends ".generated" dir
        "${REFUREKU_INCLUDE_DIRECTORIES}" # project include directories
        "${REFUREKU_EXCLUDE_FILES}" # files to exclude from analyzing
        ${CMAKE_CXX_COMPILER_ID} # used compiler
        OUTPUT_VARIABLE refureku_output
        ERROR_VARIABLE refureku_error
    )
    message(STATUS "${PROJECT_NAME}: ${refureku_output}")
    if (refureku_error)
        message(FATAL_ERROR "${refureku_error}")
    endif()

    # Include Refureku header files.
    target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${REFUREKU_EXT_PATH}/build/Include/)

    # Link Refureku library.
    if (MSVC)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4006")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4006")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4006")
    endif()
    if (WIN32)
        target_link_directories(${PROJECT_NAME} PUBLIC ${REFUREKU_EXT_PATH}/build/Lib)
    elseif(APPLE) # test macos before unix
        message(FATAL_ERROR "${PROJECT_NAME}: TODO: put Refureku dynamic library directory path here.")
    elseif(UNIX)
        target_link_directories(${PROJECT_NAME} PUBLIC ${REFUREKU_EXT_PATH}/build/Bin)
    endif()
    target_link_libraries(${PROJECT_NAME} PUBLIC Refureku)

    # Add a pre build step to run reflection code generator.
    set(REFUREKU_GENERATOR_TARGET ${PROJECT_NAME}_run_reflection_generator)
    add_custom_target(${REFUREKU_GENERATOR_TARGET}
        WORKING_DIRECTORY ${REFUREKU_EXT_PATH}/build/Bin
        COMMAND ${REFUREKU_EXT_PATH}/build/Bin/RefurekuGenerator ${REFUREKU_SETTINGS_FILE_PATH}
        ENVIRONMENT LD_LIBRARY_PATH ${REFUREKU_EXT_PATH}/build/Bin
        COMMENT "${PROJECT_NAME}: running reflection generator..."
    )
    add_dependencies(${PROJECT_NAME} ${REFUREKU_GENERATOR_TARGET})
    set_target_properties(${REFUREKU_GENERATOR_TARGET} PROPERTIES FOLDER ${EXTERNAL_FOLDER})

    if (NOT "${ENGINE_DEPENDENCY_TARGET}" STREQUAL "")
        # Make sure our reflection generator will only run after the target we depend on.
        add_dependencies(${REFUREKU_GENERATOR_TARGET} ${ENGINE_DEPENDENCY_TARGET})
    endif()

    # Add generated headers to include directories.
    target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GENERATED_SOURCE_DIR_PATH})
endfunction()

# ------------------------------------------------------------------------------------------------------------------