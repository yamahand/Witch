#include "Platform/PlatformFactory.h"
#include "Platform/Windows/Win32Input.h"

// CreatePlatformInput() の実装。Win32Input 具象への依存をこの Windows TU に閉じ込める。
// （レンダラ生成は RHI バックエンドのヘッダ隔離のため Rhi/<backend>/ 側の専用 TU に分離。）

namespace witch::platform {

std::unique_ptr<IInput> CreatePlatformInput() {
    return std::make_unique<Win32Input>();
}

} // namespace witch::platform
