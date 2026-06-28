#include "Platform/Memory.h"

#include "WitchEngine/Core/Logger.h"

#include <mimalloc.h>
#include <mimalloc-new-delete.h>

#include <new>

namespace witch::platform {

void EnsureAllocatorActive() {
    // この関数が他 TU（Engine::Init）から参照されることで、MSVC リンカが
    // Memory.obj（new/delete 差し替えを含む）を静的ライブラリから取りこぼす
    // silent failure を防ぐ。mi_version() 参照も同じ保証に寄与する。
    log::Info("Verifying mimalloc allocator (v{})...", mi_version());

    // mimalloc は static リンク（redirect DLL 不使用）のため mi_is_redirected()
    // は常に false。代わりにグローバル new が mimalloc ヒープから確保されるかを
    // 実測し、operator new/delete の差し替えが実際に効いているか検証する。
    // 例外をエンジン内部へ伝播させない方針のため nothrow 版を使う。
    void* p = ::operator new(64, std::nothrow);
    if (!p) {
        log::Warn("mimalloc probe allocation failed.");
        return;
    }
    const bool overridden = mi_is_in_heap_region(p);
    ::operator delete(p, std::nothrow);

    if (overridden) {
        log::Info("mimalloc override verified.");
    } else {
        log::Warn("mimalloc override is NOT active; using the standard allocator.");
    }
}

}  // namespace witch::platform
