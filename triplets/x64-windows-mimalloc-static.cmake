set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# mimalloc だけ static にして、dynamic override（mimalloc-redirect.dll）が
# ucrtbase より後に初期化されて差し替えが無効化する Win11 のレースを回避する。
# static override は redirect DLL を使わず、operator new/delete の差し替えが
# リンク時にバイナリへ直接入るため DLL ロード順に依存しない。
if(PORT STREQUAL "mimalloc")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
