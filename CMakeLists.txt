cmake_minimum_required(VERSION 3.25)
project(YELLOWTEAMGAME LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "YELLOWTEAMGAME currently supports the Windows development target only.")
endif()
if(NOT DEFINED ENV{VULKAN_SDK} OR "$ENV{VULKAN_SDK}" STREQUAL "")
    message(FATAL_ERROR "VULKAN_SDK is not set. Install the Vulkan SDK and reopen PowerShell.")
endif()

set(PROJECT_SOURCES
    src/main.cpp
    src/CCL.cpp
    src/DRAW/Level.cpp
    src/DRAW/VulkanBuffers.cpp
    src/DRAW/VulkanRenderer.cpp
    src/GAME/Player.cpp
    src/GAME/Enemy.cpp
    src/GAME/Combat.cpp
    src/GAME/GameManager.cpp
    src/UTIL/GameConfig.cpp
    src/UTIL/Utilities.cpp)
set(PROJECT_HEADERS
    precompiled.h
    src/APP/Window.hpp
    src/CCL.h
    src/DRAW/DrawComponents.h
    src/DRAW/Utility/FileIntoString.h
    src/DRAW/Utility/h2bParser.h
    src/DRAW/Utility/load_data_oriented.h
    src/GAME/GameComponents.h
    src/GAME/GameplaySystems.h
    src/GAME/GameManager.h
    src/UTIL/GameConfig.h
    src/UTIL/Utilities.h)
set(PROJECT_SHADERS shaders/LevelVertex.hlsl shaders/LevelPixel.hlsl)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS} ${PROJECT_SHADERS})
add_custom_target(copy_runtime_content ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying runtime configuration, shaders, and assets")
add_dependencies(${PROJECT_NAME} copy_runtime_content)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE $<$<CONFIG:Debug>:DEBUG>)
target_compile_options(${PROJECT_NAME} PRIVATE /MD)
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/gateware-main"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/entt-3.13.1/single_include"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/json-develop/single_include"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/inifile-cpp-master/include"
            "$ENV{VULKAN_SDK}/Include")
target_link_directories(${PROJECT_NAME} PRIVATE "$ENV{VULKAN_SDK}/Lib")
target_precompile_headers(${PROJECT_NAME} PRIVATE precompiled.h)

set_source_files_properties(${PROJECT_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE VS_TOOL_OVERRIDE
                                                                                "None")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PROJECT_SOURCES} ${PROJECT_HEADERS}
                                                      ${PROJECT_SHADERS})
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
