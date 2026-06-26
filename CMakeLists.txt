cmake_minimum_required(VERSION 3.28)
project(Witch CXX)

# ─── WitchEngine (STATIC) ────────────────────────────────────────────────────
add_library(WitchEngine STATIC
    Engine/Source/Core/Engine.cpp
    Engine/Source/Core/Logger.cpp
    Engine/Source/Core/ObjectRegistry.cpp
    Engine/Source/Core/ResourceManager.cpp
    Engine/Source/Core/Time.cpp
    Engine/Source/Scene/GameObject.cpp
    Engine/Source/Scene/Scene.cpp
    Engine/Source/Graphics2D/SpriteComponent.cpp
)

if(WIN32)
    target_sources(WitchEngine PRIVATE
        Engine/Source/Platform/Windows/PlatformWindow.cpp
        Engine/Source/Rhi/D3D12/D3D12Renderer.cpp
    )
    target_compile_definitions(WitchEngine PUBLIC UNICODE _UNICODE)
    target_link_libraries(WitchEngine PUBLIC user32 d3d12 dxgi d3dcompiler)
endif()

target_include_directories(WitchEngine
    PUBLIC  Engine/Include
    PRIVATE Engine/Source
)

target_compile_features(WitchEngine PUBLIC cxx_std_23)

if(MSVC)
    target_compile_options(WitchEngine PRIVATE /W4 /WX /utf-8)
endif()

find_package(Stb REQUIRED)
target_include_directories(WitchEngine PRIVATE ${Stb_INCLUDE_DIR})

# ─── WitchGame (executable) ──────────────────────────────────────────────────
add_executable(WitchGame
    Game/Source/Main.cpp
    Game/Source/Scenes/EmptyScene.cpp
)

target_include_directories(WitchGame PRIVATE Game/Source)

target_link_libraries(WitchGame PRIVATE WitchEngine)

if(MSVC)
    target_compile_options(WitchGame PRIVATE /W4 /WX /utf-8)
endif()

# ─── Copy runtime assets to output directory ─────────────────────────────────
add_custom_command(TARGET WitchGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Engine/Shaders
        $<TARGET_FILE_DIR:WitchGame>/Shaders
    COMMENT "Copying shaders to output directory"
)

add_custom_command(TARGET WitchGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Game/Assets
        $<TARGET_FILE_DIR:WitchGame>/Assets
    COMMENT "Copying game assets to output directory"
)
