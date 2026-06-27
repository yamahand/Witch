#include <mimalloc.h>
#include <mimalloc-new-delete.h>

#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Memory.h"

namespace witch {

void EnsureAllocatorActive() {
    // この関数が他 TU（Engine::Init）から参照されることで、MSVC リンカが
    // Memory.obj（new/delete 差し替えを含む）を静的ライブラリから取りこぼす
    // silent failure を防ぐ。mi_version() 参照も同じ保証に寄与する。
    log::Info("mimalloc {} active.", mi_version());

    // mimalloc は static リンク（redirect DLL 不使用）のため mi_is_redirected()
    // は常に false。代わりにグローバル new が mimalloc ヒープから確保されるかを
    // 実測し、operator new/delete の差し替えが実際に効いているか検証する。
    void* p = ::operator new(64);
    const bool overridden = mi_is_in_heap_region(p);
    ::operator delete(p);

    if (!overridden) {
        log::Warn("mimalloc override is NOT active; using the standard allocator.");
    }
}

}  // namespace witch
