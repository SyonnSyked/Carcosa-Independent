include_guard(GLOBAL)
include(FetchContent)

# Keep dependency settings local rather than changing the game's build options.
function(carcosa_add_fastgltf)
    set(BUILD_SHARED_LIBS OFF)
    # The game explicitly uses /MD in both Debug and Release.
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "Build fastgltf tests")
    set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "Build fastgltf examples")
    set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "Build fastgltf documentation")
    set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "Install fastgltf")
    set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "Build fastgltf C++ modules")

    FetchContent_Declare(fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        # v1.0.0; pin the commit so upstream changes cannot alter our build.
        GIT_TAG 0b800b4a584704f9e9add67c79c4c8732e552787
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(fastgltf)
    # fastgltf accessor templates can emit half-float conversions on clang-cl.
    # The MSVC runtime does not provide Clang's __extendhfsf2 helper.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND MSVC)
        execute_process(COMMAND "${CMAKE_CXX_COMPILER}" --print-resource-dir
            OUTPUT_VARIABLE clang_resource_dir OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$")
            set(clang_runtime_arch x86_64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
            set(clang_runtime_arch aarch64)
        else()
            set(clang_runtime_arch i386)
        endif()
        find_library(CARCOSA_CLANG_BUILTINS
            NAMES "clang_rt.builtins-${clang_runtime_arch}"
            PATHS "${clang_resource_dir}/lib/windows" NO_DEFAULT_PATH REQUIRED)
        target_link_libraries(fastgltf INTERFACE "${CARCOSA_CLANG_BUILTINS}")
    endif()
endfunction()

carcosa_add_fastgltf()
