# Enable address sanitizer (gcc/clang only).
function(enable_address_sanitizer TARGET_NAME)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Address sanitizer is only supported for GCC/Clang.")
    endif()
    message(STATUS "Address sanitizer is enabled.")
    target_compile_options(${TARGET_NAME} PRIVATE -fno-omit-frame-pointer)
    target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=address)
    target_link_libraries(${TARGET_NAME} PRIVATE -fno-omit-frame-pointer)
    target_link_libraries(${TARGET_NAME} PRIVATE -fsanitize=address)
endfunction()

# Enable more warnings and warnings as errors.
function(enable_more_warnings TARGET_NAME)
    # More warnings.
    if(MSVC)
        target_compile_options(${TARGET_NAME} PUBLIC /W3 /WX)
    else()
        target_compile_options(${TARGET_NAME} PUBLIC -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable)
    endif()

    # Compiler specific flags.
    if(MSVC)
        target_compile_options(${TARGET_NAME} PUBLIC /utf-8)
    endif()
endfunction()
