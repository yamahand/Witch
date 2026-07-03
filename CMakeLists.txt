cmake_minimum_required(VERSION 3.28)
project(Witch
    VERSION 0.1.0
    DESCRIPTION "2D Game Engine and Game"
    # mimalloc は C ライブラリのため、find_package 時に C コンパイラが要求される
    LANGUAGES CXX C
)

# ─── WitchEngine version ─────────────────────────────────────────────────────
set(WITCH_ENGINE_VERSION_MAJOR 0)
set(WITCH_ENGINE_VERSION_MINOR 1)
set(WITCH_ENGINE_VERSION_PATCH 0)
set(WITCH_ENGINE_VERSION "${WITCH_ENGINE_VERSION_MAJOR}.${WITCH_ENGINE_VERSION_MINOR}.${WITCH_ENGINE_VERSION_PATCH}")

set(WITCH_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${WITCH_GENERATED_INCLUDE_DIR}/WitchEngine/Core")
file(MAKE_DIRECTORY "${WITCH_GENERATED_INCLUDE_DIR}/WitchGame")

configure_file(
    Engine/Include/WitchEngine/Core/Version.h.in
    ${WITCH_GENERATED_INCLUDE_DIR}/WitchEngine/Core/Version.h
    @ONLY
)

# ─── Profiling option ────────────────────────────────────────────────────────
# Tracy プロファイラの有効化。明示的に ON にしたビルドだけで計測する
# （RelWithDebInfo のパフォーマンス計測に Tracy のオーバーヘッドが紛れ込むのを避ける）。
option(WITCH_PROFILING "Enable Tracy profiling" OFF)

# Dear ImGui デバッグ UI の有効化。OFF（例: release プリセット）では ImGui を
# find_package / link / define せず、ビルドから完全に外す。vcpkg 側も debug-ui feature を
# 付けたプリセットでのみ imgui を取得する（CMakePresets.json / vcpkg.json 参照）。
option(WITCH_DEBUG_UI "Enable Dear ImGui debug UI" OFF)

if(WITCH_DEBUG_UI)
    find_package(imgui CONFIG REQUIRED)
endif()

# ─── WitchEngine (STATIC) ────────────────────────────────────────────────────
# ヘッダも target_sources に加えているのは機能上必須ではなく、Visual Studio の
# ソリューションエクスプローラー（フィルター）にヘッダを表示させ、単独 IDE 編集を
# 快適にするため（source_group(TREE ...) と対にして使う）。
add_library(WitchEngine STATIC
    Engine/Source/Core/Engine.cpp
    Engine/Source/Core/Application.cpp
    Engine/Source/Core/GameLoop.cpp
    Engine/Source/Core/Logger.cpp
    Engine/Source/Core/ObjectRegistry.cpp
    Engine/Source/Core/ResourceManager.cpp
    Engine/Source/Core/Time.cpp
    Engine/Source/Scene/GameObject.cpp
    Engine/Source/Scene/Scene.cpp
    Engine/Source/Graphics2D/CameraManager.cpp
    Engine/Source/Graphics2D/SpriteComponent.cpp

    Engine/Source/Core/Profiling.h
    Engine/Source/Platform/Memory.h
    Engine/Source/Platform/PlatformFactory.h
    Engine/Source/Platform/PlatformWindow.h

    Engine/Include/WitchEngine/Core/Engine.h
    Engine/Include/WitchEngine/Core/Application.h
    Engine/Include/WitchEngine/Core/FixedString.h
    Engine/Include/WitchEngine/Core/GameLoop.h
    Engine/Include/WitchEngine/Core/Logger.h
    Engine/Include/WitchEngine/Core/ObjectRegistry.h
    Engine/Include/WitchEngine/Core/ResourceManager.h
    Engine/Include/WitchEngine/Core/Services.h
    Engine/Include/WitchEngine/Core/TextureInfo.h
    Engine/Include/WitchEngine/Core/Time.h
    Engine/Include/WitchEngine/Graphics2D/Anchor.h
    Engine/Include/WitchEngine/Graphics2D/Camera2D.h
    Engine/Include/WitchEngine/Graphics2D/CameraManager.h
    Engine/Include/WitchEngine/Graphics2D/SpriteComponent.h
    Engine/Include/WitchEngine/Input/IInput.h
    Engine/Include/WitchEngine/Input/Key.h
    Engine/Include/WitchEngine/Rhi/IRenderer.h
    Engine/Include/WitchEngine/Rhi/RhiTypes.h
    Engine/Include/WitchEngine/Scene/Component.h
    Engine/Include/WitchEngine/Scene/DebugUI.h
    Engine/Include/WitchEngine/Scene/GameObject.h
    Engine/Include/WitchEngine/Scene/Scene.h
    Engine/Include/WitchEngine/Scene/Transform.h
)

if(WIN32)
    find_package(mimalloc CONFIG REQUIRED)

    target_sources(WitchEngine PRIVATE
        Engine/Source/Platform/Windows/PlatformWindow.cpp
        Engine/Source/Platform/Windows/Win32Input.cpp
        Engine/Source/Platform/Windows/Win32InputFactory.cpp
        Engine/Source/Platform/Windows/Memory.cpp
        Engine/Source/Rhi/D3D12/D3D12Renderer.cpp
        Engine/Source/Rhi/D3D12/D3D12Factory.cpp

        Engine/Source/Platform/Windows/Win32Input.h
        Engine/Source/Rhi/D3D12/D3D12CommandList.h
        Engine/Source/Rhi/D3D12/D3D12Renderer.h
    )
    target_compile_definitions(WitchEngine PUBLIC UNICODE _UNICODE)
    target_link_libraries(WitchEngine PUBLIC user32 d3d12 dxgi d3dcompiler)
    # new/delete 差し替えは Memory.cpp 内で完結し API を外部公開しないため PRIVATE。
    target_link_libraries(WitchEngine PRIVATE mimalloc)
endif()

# Visual Studio のソリューションエクスプローラーで実ディレクトリ階層通りの
# フィルターを作る（ヘッダも target_sources 済みなので Header Files 相当も分類される）。
get_target_property(_WitchEngineSources WitchEngine SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${_WitchEngineSources})

if(WITCH_DEBUG_UI)
    # ImGui はゲーム側が GameObject::DrawDebugUI() 内で直接使う設計のため、あえて PUBLIC で
    # Game へ伝播させる（デバッグ UI 記述のイテレーション速度を優先）。
    # WITCH_DEBUG_UI マクロも PUBLIC 定義し、Game 側 TU が自分の ImGui コードを
    # #ifdef WITCH_DEBUG_UI で守れるようにする。
    target_link_libraries(WitchEngine PUBLIC imgui::imgui)
    target_compile_definitions(WitchEngine PUBLIC WITCH_DEBUG_UI)
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
    PUBLIC
        Engine/Include
        ${WITCH_GENERATED_INCLUDE_DIR}
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

    Game/Source/Scenes/EmptyScene.h
)

# ─── WitchGame version ─────────────────────────────────────────────────────
set(WITCH_GAME_VERSION_MAJOR 0)
set(WITCH_GAME_VERSION_MINOR 1)
set(WITCH_GAME_VERSION_PATCH 0)
set(WITCH_GAME_VERSION "${WITCH_GAME_VERSION_MAJOR}.${WITCH_GAME_VERSION_MINOR}.${WITCH_GAME_VERSION_PATCH}")

configure_file(
    Game/Source/Version.h.in
    ${WITCH_GENERATED_INCLUDE_DIR}/WitchGame/Version.h
    @ONLY
)

target_include_directories(WitchGame
    PRIVATE
        Game/Source
        ${WITCH_GENERATED_INCLUDE_DIR}
)

target_link_libraries(WitchGame PRIVATE WitchEngine)

get_target_property(_WitchGameSources WitchGame SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${_WitchGameSources})

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
