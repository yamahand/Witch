#include "WitchEngine/Core/Log/DetailedTextFormatter.h"
#include "WitchEngine/Core/Log/LogView.h"
#include "Core/Log/TimestampFormat.h"
#include <format>
#include <string_view>

namespace witch::log {

namespace {

/// フルパスからファイル名だけを取り出す。
std::string_view BaseName(const char* path) {
    std::string_view sv(path);
    if (auto pos = sv.find_last_of("/\\"); pos != std::string_view::npos) {
        sv.remove_prefix(pos + 1);
    }
    return sv;
}

/// MSVC の function_name() は戻り値型・呼出規約込みの完全なシグネチャを返すため、
/// 引数リスト以降を落とし、最後の空白より後ろ（修飾名）だけを残す。
/// 例: "void __cdecl witch::EmptyScene::FrameUpdate(float)" → "witch::EmptyScene::FrameUpdate"
std::string_view ShortFunction(const char* function) {
    std::string_view sv(function);
    if (auto paren = sv.find('('); paren != std::string_view::npos) {
        sv = sv.substr(0, paren);
    }
    if (auto space = sv.find_last_of(' '); space != std::string_view::npos) {
        sv.remove_prefix(space + 1);
    }
    return sv;
}

} // namespace

std::string DetailedTextFormatter::Format(const LogView& view) const {
    if (view.category.empty()) {
        return std::format("[{}][{:<5}][F{}][T{:08x}] {} ({}:{} {})",
                           FormatTimestamp(view.timestamp), ToString(view.level),
                           view.frameNumber, view.threadId,
                           view.message, BaseName(view.file), view.line,
                           ShortFunction(view.function));
    }
    return std::format("[{}][{:<5}][F{}][T{:08x}][{}] {} ({}:{} {})",
                       FormatTimestamp(view.timestamp), ToString(view.level),
                       view.frameNumber, view.threadId, view.category,
                       view.message, BaseName(view.file), view.line,
                       ShortFunction(view.function));
}

} // namespace witch::log
