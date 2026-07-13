#pragma once

// Tracy 計測マクロ + インゲーム ProfilerHud 用インプロセス集約の集約ヘッダ。
// 計測する TU はこのヘッダを include するだけでよい。
//
// 2 系統の計測先を持つが、いずれも呼び出し側は同じマクロで書ける:
//   1. Tracy: WITCH_PROFILING=ON のビルドでのみ TRACY_ENABLE が定義される。
//      外部 Tracy GUI へストリーム送信する（インプロセスで読み返せない）。
//   2. インプロセス集約（ProfileCollector）: WITCH_PROFILE_COLLECT が定義された
//      ビルドでのみ動く（CMakeLists.txt が WITCH_DEBUG_UI に連動して定義）。
//      Tracy が読み返せない数値をインゲーム HUD に出すための自前計測。
//
// 両者は独立。Tracy OFF / DebugUI ON なら集約のみ、その逆なら Tracy のみ、
// 両方 OFF なら全マクロが no-op に潰れて依存ゼロになる。

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#ifdef WITCH_PROFILE_COLLECT
#include "Core/ProfileCollector.h"
#endif

// ── 内部ヘルパ: 一意な変数名を作る（同一関数内で複数スコープを開けるように） ──
#define WITCH_PROFILE_CONCAT_(a, b) a##b
#define WITCH_PROFILE_CONCAT(a, b)  WITCH_PROFILE_CONCAT_(a, b)

// ── インプロセス集約の 1 スコープ分。Tracy とは別に RAII を 1 個積む ──
#ifdef WITCH_PROFILE_COLLECT
#define WITCH_PROFILE_COLLECT_SCOPE_(name) \
    ::witch::profile::ProfileScope WITCH_PROFILE_CONCAT(witchProfileScope_, __LINE__){name}
#define WITCH_PROFILE_COLLECT_FRAME_() \
    ::witch::profile::Collector::Instance().BeginFrame()
#else
#define WITCH_PROFILE_COLLECT_SCOPE_(name) ((void)0)
#define WITCH_PROFILE_COLLECT_FRAME_()     ((void)0)
#endif

// ── Tracy の 1 スコープ分 ──
#ifdef TRACY_ENABLE
#define WITCH_PROFILE_TRACY_SCOPE_()      ZoneScoped
#define WITCH_PROFILE_TRACY_SCOPE_N_(n)   ZoneScopedN(n)
#define WITCH_PROFILE_TRACY_FRAME_()      FrameMark
#else
#define WITCH_PROFILE_TRACY_SCOPE_()      ((void)0)
#define WITCH_PROFILE_TRACY_SCOPE_N_(n)   ((void)0)
#define WITCH_PROFILE_TRACY_FRAME_()      ((void)0)
#endif

// ── 公開マクロ ────────────────────────────────────────────────────────────────
// 無名スコープ: Tracy のみ（インプロセス集約は表示に名前が要るので名前付き専用）。
#define WITCH_PROFILE_SCOPE()       WITCH_PROFILE_TRACY_SCOPE_()

// 名前付きスコープ: Tracy とインプロセス集約の両方へ流す。
// name は文字列リテラルを渡すこと（集約側がコピーせずポインタ保持するため）。
#if defined(TRACY_ENABLE) || defined(WITCH_PROFILE_COLLECT)
#define WITCH_PROFILE_SCOPE_N(name)              \
    WITCH_PROFILE_TRACY_SCOPE_N_(name);          \
    WITCH_PROFILE_COLLECT_SCOPE_(name)
#else
#define WITCH_PROFILE_SCOPE_N(name)   ((void)0)
#endif

// フレーム境界: Tracy の FrameMark と、集約器の BeginFrame（前フレーム確定）。
#if defined(TRACY_ENABLE) || defined(WITCH_PROFILE_COLLECT)
#define WITCH_PROFILE_FRAME()               \
    do {                                    \
        WITCH_PROFILE_COLLECT_FRAME_();     \
        WITCH_PROFILE_TRACY_FRAME_();       \
    } while (0)
#else
#define WITCH_PROFILE_FRAME()         ((void)0)
#endif
