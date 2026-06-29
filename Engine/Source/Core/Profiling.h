#pragma once

// Tracy 計測マクロの集約ヘッダ。計測する TU はこのヘッダを include するだけでよい。
//
// WITCH_PROFILING=ON のビルドでのみ TRACY_ENABLE が定義される（CMakeLists.txt 参照）。
// TRACY_ENABLE 未定義時は Tracy ヘッダを含めず、計測マクロを no-op に潰す。
// これにより OFF ビルドは Tracy に非依存になり、各 TU で no-op フォールバックを
// 重複させずに済む。
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#if defined(TRACY_ENABLE)
    #define WITCH_PROFILE_SCOPE()       ZoneScoped
    #define WITCH_PROFILE_SCOPE_N(name) ZoneScopedN(name)
    #define WITCH_PROFILE_FRAME()       FrameMark
#else
    #define WITCH_PROFILE_SCOPE()         ((void)0)
    #define WITCH_PROFILE_SCOPE_N(name)   ((void)0)
    #define WITCH_PROFILE_FRAME()         ((void)0)
#endif