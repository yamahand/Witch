cmake_minimum_required(VERSION 3.28)
project(Witch
    VERSION 0.1.0
    DESCRIPTION "2D Game Engine and Game"
    # mimalloc は C ライブラリのため、find_package 時に C コンパイラが要求される
    LANGUAGES CXX C
)

# ─── Profiling option ────────────────────────────────────────────────────────
# Tracy プロファイラの有効化。明示的に ON にしたビルドだけで計測する
# （RelWithDebInfo のパフォーマンス計測に Tracy のオーバーヘッドが紛れ込むのを避ける）。
option(WITCH_PROFILING "Enable Tracy profiling" OFF)

find_package(imgui CONFIG REQUIRED)

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
    find_package(mimalloc CONFIG REQUIRED)

    target_sources(WitchEngine PRIVATE
        Engine/Source/Platform/Windows/PlatformWindow.cpp
        Engine/Source/Platform/Windows/Memory.cpp
        Engine/Source/Rhi/D3D12/D3D12Renderer.cpp
    )
    target_compile_definitions(WitchEngine PUBLIC UNICODE _UNICODE)
    # ImGui はゲーム側が GameObject::DrawDebugUI() 内で直接使う設計のため、あえて PUBLIC で
    # Game へ伝播させる（デバッグ UI 記述のイテレーション速度を優先）。
    target_link_libraries(WitchEngine PUBLIC user32 d3d12 dxgi d3dcompiler imgui::imgui)
    # new/delete 差し替えは Memory.cpp 内で完結し API を外部公開しないため PRIVATE。
    target_link_libraries(WitchEngine PRIVATE mimalloc)
endif()

# ─── Tracy profiling ─────────────────────────────────────────────────────────
# WITCH_PROFILING=ON のときだけ Tracy をリンクし TRACY_ENABLE を定義する。
# OFF 時は find_package / link / define を一切行わないため、Engine 自体は Tracy に
# 非依存（計測マクロは Core/Profiling.h が no-op に潰す）。
# tracy 自体の取得も vcpkg の profiling feature に切り出してあり（vcpkg.json）、
# debug-profiling プリセット（VCPKG_MANIFEST_FEATURES=profiling）でのみ取得される。
# 通常の debug / release / ci プリセットは tracy を取得しないため依存ゼロ。
# 計測は Engine の .cpp 内で完結するため PRIVATE で十分（公開ヘッダには漏らさない）。
if(WITCH_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(WitchEngine PRIVATE Tracy::TracyClient)
    target_compile_definitions(WitchEngine PRIVATE TRACY_ENABLE)
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
