cmake_minimum_required(VERSION 3.25)
project(YELLOWTEAMGAME LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "YELLOWTEAMGAME currently supports the Windows development target only.")
endif()
if(NOT DEFINED ENV{VULKAN_SDK} OR "$ENV{VULKAN_SDK}" STREQUAL "")
    message(FATAL_ERROR "VULKAN_SDK is not set. Install the Vulkan SDK and reopen PowerShell.")
endif()

include(cmake/FastGltf.cmake)

set(PROJECT_SOURCES
    src/main.cpp
    src/CCL.cpp
    src/DRAW/Level.cpp
    src/DRAW/GltfEnvironment.cpp
    src/DRAW/VulkanBuffers.cpp
    src/DRAW/VulkanRenderer.cpp
    src/DRAW/VulkanTextures.cpp
    src/GAME/Player.cpp
    src/GAME/Collision.cpp
    src/GAME/Enemy.cpp
    src/GAME/KnightBoss.cpp
    src/GAME/Boss.cpp
    src/GAME/Combat.cpp
    src/GAME/PickUp&Apply.cpp
    src/GAME/DamageFeedback.cpp
    src/GAME/HordeSpawner.cpp
    src/GAME/DungeonKeys.cpp
    src/GAME/GameManager.cpp
    src/UTIL/GameConfig.cpp
    src/UTIL/Utilities.cpp
    include/imgui/imgui.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/backends/imgui_impl_vulkan.cpp
    include/imgui/backends/imgui_impl_win32.cpp
    )
set(PROJECT_HEADERS
    precompiled.h
    src/APP/Window.hpp
    src/CCL.h
    src/DRAW/DrawComponents.h
    src/DRAW/VulkanTextures.h
    src/DRAW/Utility/FileIntoString.h
    src/DRAW/Utility/h2bParser.h
    src/DRAW/Utility/load_data_oriented.h
    src/GAME/GameComponents.h
    src/GAME/GameplaySystems.h
    src/GAME/GameManager.h
    src/UTIL/GameConfig.h
    src/UTIL/Utilities.h
    include/imgui/imgui.h
    include/imgui/backends/imgui_impl_vulkan.h
    include/imgui/backends/imgui_impl_win32.h
    )
set(PROJECT_SHADERS shaders/LevelVertex.hlsl shaders/LevelPixel.hlsl)

# Keep object filenames containing & out of the Windows linker shell command.
set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS} ${PROJECT_SHADERS})
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf::fastgltf)
add_custom_target(
    copy_runtime_content ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying runtime configuration, shaders, and assets")
add_dependencies(${PROJECT_NAME} copy_runtime_content)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME "Carcosa")
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE $<$<CONFIG:Debug>:DEBUG>)
target_compile_options(${PROJECT_NAME} PRIVATE /MD /EHsc)
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/gateware-main"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/entt-3.13.1/single_include"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/json-develop/single_include"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/inifile-cpp-master/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends"
            "$ENV{VULKAN_SDK}/Include")
target_link_directories(${PROJECT_NAME} PRIVATE "$ENV{VULKAN_SDK}/Lib")
target_precompile_headers(${PROJECT_NAME} PRIVATE precompiled.h)

set_source_files_properties(${PROJECT_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE VS_TOOL_OVERRIDE
                                                                                "None")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PROJECT_SOURCES} ${PROJECT_HEADERS}
                                                      ${PROJECT_SHADERS})
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

option(CARCOSA_AXE_TESTS "Build warrior axe gameplay checks" OFF)
option(CARCOSA_FASTGLTF_TESTS "Build fastgltf integration smoke check" OFF)
if(CARCOSA_FASTGLTF_TESTS)
    enable_testing()
    add_executable(FastGltfSmoke tests/FastGltfSmoke.cpp)
    target_link_libraries(FastGltfSmoke PRIVATE fastgltf::fastgltf)
    set_target_properties(FastGltfSmoke PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    add_test(NAME FastGltfIntegration COMMAND FastGltfSmoke)
    add_executable(GltfEnvironmentTests tests/GltfEnvironmentTests.cpp src/DRAW/GltfEnvironment.cpp)
    target_link_libraries(GltfEnvironmentTests PRIVATE fastgltf::fastgltf)
    target_compile_options(GltfEnvironmentTests PRIVATE /MD /EHsc)
    target_compile_definitions(GltfEnvironmentTests PRIVATE UNICODE _UNICODE)
    get_target_property(gltf_test_includes ${PROJECT_NAME} INCLUDE_DIRECTORIES)
    target_include_directories(GltfEnvironmentTests PRIVATE ${gltf_test_includes})
    target_link_directories(GltfEnvironmentTests PRIVATE "$ENV{VULKAN_SDK}/Lib")
    target_precompile_headers(GltfEnvironmentTests PRIVATE precompiled.h)
    add_test(NAME GltfEnvironment COMMAND GltfEnvironmentTests)
    set_tests_properties(GltfEnvironment PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

if(CARCOSA_AXE_TESTS)
    enable_testing()
    set(AXE_TEST_SOURCES ${PROJECT_SOURCES})
    list(REMOVE_ITEM AXE_TEST_SOURCES src/main.cpp)
    add_executable(AxeThrowTests tests/AxeThrowTests.cpp ${AXE_TEST_SOURCES})
    target_link_libraries(AxeThrowTests PRIVATE fastgltf::fastgltf)
    target_compile_features(AxeThrowTests PRIVATE cxx_std_20)
    target_compile_options(AxeThrowTests PRIVATE /MD /EHsc)
    target_compile_definitions(AxeThrowTests PRIVATE UNICODE _UNICODE $<$<CONFIG:Debug>:DEBUG>)
    get_target_property(axe_test_includes ${PROJECT_NAME} INCLUDE_DIRECTORIES)
    target_include_directories(AxeThrowTests PRIVATE ${axe_test_includes})
    target_link_directories(AxeThrowTests PRIVATE "$ENV{VULKAN_SDK}/Lib")
    target_precompile_headers(AxeThrowTests PRIVATE precompiled.h)
    add_test(NAME WarriorAxeThrow COMMAND AxeThrowTests)
    set_tests_properties(WarriorAxeThrow PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

option(CARCOSA_TRANSITION_TESTS "Build Vulkan level transition regression checks" OFF)
if(CARCOSA_TRANSITION_TESTS)
    enable_testing()
    set(TRANSITION_TEST_SOURCES ${PROJECT_SOURCES})
    list(REMOVE_ITEM TRANSITION_TEST_SOURCES src/main.cpp)
    add_executable(LevelTransitionTests tests/LevelTransitionTests.cpp ${TRANSITION_TEST_SOURCES})
    target_link_libraries(LevelTransitionTests PRIVATE fastgltf::fastgltf)
    target_compile_features(LevelTransitionTests PRIVATE cxx_std_20)
    target_compile_options(LevelTransitionTests PRIVATE /MD /EHsc)
    target_compile_definitions(LevelTransitionTests PRIVATE UNICODE _UNICODE $<$<CONFIG:Debug>:DEBUG>)
    get_target_property(transition_test_includes ${PROJECT_NAME} INCLUDE_DIRECTORIES)
    target_include_directories(LevelTransitionTests PRIVATE ${transition_test_includes})
    target_link_directories(LevelTransitionTests PRIVATE "$ENV{VULKAN_SDK}/Lib")
    target_precompile_headers(LevelTransitionTests PRIVATE precompiled.h)
    add_test(NAME LevelTransition COMMAND LevelTransitionTests)
    set_tests_properties(LevelTransition PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

option(CARCOSA_TEXTURE_TESTS "Build PNG material regression checks" OFF)
if(CARCOSA_TEXTURE_TESTS)
    enable_testing()
    add_executable(TextureTests tests/TextureTests.cpp src/DRAW/VulkanTextures.cpp)
    target_compile_features(TextureTests PRIVATE cxx_std_20)
    target_compile_options(TextureTests PRIVATE /MD)
    target_compile_definitions(TextureTests PRIVATE UNICODE _UNICODE)
    get_target_property(texture_test_includes ${PROJECT_NAME} INCLUDE_DIRECTORIES)
    target_include_directories(TextureTests PRIVATE ${texture_test_includes})
    target_link_directories(TextureTests PRIVATE "$ENV{VULKAN_SDK}/Lib")
    target_precompile_headers(TextureTests PRIVATE precompiled.h)
    add_test(NAME TextureMaterials COMMAND TextureTests)
    set_tests_properties(TextureMaterials PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

# Release Distribution
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES)
    find_program(GAME_GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
    find_file(VULKAN_LOADER vulkan-1.dll HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
    get_filename_component(VULKAN_LOADER_DIR "${VULKAN_LOADER}" DIRECTORY)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        message(FATAL_ERROR "Microsoft redistributable runtime DLLs were not found")
    endif()
    configure_file(scripts/package_game.cmake.in package_game.cmake @ONLY)
    # The Release preset uses single-config Ninja. Always refresh runtime data/ZIP,
    # while leaving Debug and multi-config default builds unchanged.
    set(package_game_default)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_CONFIGURATION_TYPES)
        set(package_game_default ALL)
    endif()
    add_custom_target(
        package_game ${package_game_default}
        COMMAND ${CMAKE_COMMAND} "-DGAME_EXECUTABLE=$<TARGET_FILE:${PROJECT_NAME}>"
                "-DGAME_CONFIGURATION=$<CONFIG>" -P "${CMAKE_BINARY_DIR}/package_game.cmake"
        DEPENDS ${PROJECT_NAME}
        COMMENT "Creating Release distribution and submission ZIP"
        VERBATIM)
else()
    # Packaging from a development build still uses the official Release preset.
    add_custom_target(
        package_game
        COMMAND ${CMAKE_COMMAND} --preset release
        COMMAND ${CMAKE_COMMAND} --build --preset release --target package_game
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        VERBATIM)
endif()
