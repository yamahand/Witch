# WitchEngine のユニットテスト。描画（RHI / D3D12）非依存のロジックのみを対象にする。
# Catch2 の catch_discover_tests が各 TEST_CASE を ctest に個別登録するため、
# CI（ctest --preset test-ci-windows-*）はテスト追加のたびに配線不要でそのまま拾う。
find_package(Catch2 3 CONFIG REQUIRED)

add_executable(WitchEngineTests
    TestHelpers.h
    SceneTests.cpp
    ComponentTypeIdTests.cpp
    ComponentSchedulerTests.cpp
    FixedStepAccumulatorTests.cpp
    VfsPathUtilTests.cpp
)

target_link_libraries(WitchEngineTests PRIVATE WitchEngine Catch2::Catch2WithMain)

# 非公開ヘッダ（Engine/Source 下の VfsPathUtil.h 等）をホワイトボックステストするため。
target_include_directories(WitchEngineTests PRIVATE ${CMAKE_SOURCE_DIR}/Engine/Source)

if(MSVC)
    target_compile_options(WitchEngineTests PRIVATE /W4 /WX /utf-8)
endif()

get_target_property(_WitchEngineTestsSources WitchEngineTests SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${_WitchEngineTestsSources})

include(Catch)
catch_discover_tests(WitchEngineTests)
