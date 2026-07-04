#include "WitchEngine/Vfs/VfsPathUtil.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace witch::vfs::detail {

std::optional<std::string> NormalizePath(std::string_view path) {
    // バックスラッシュをスラッシュに統一し、先頭スラッシュ・"." を除去する。
    // ".." を含むパスはトラバーサル攻撃の可能性があるため nullopt を返す。
    std::string result;
    result.reserve(path.size());

    for (char c : path) {
        result.push_back(c == '\\' ? '/' : c);
    }

    while (!result.empty() && result.front() == '/') {
        result.erase(result.begin());
    }

    std::vector<std::string> segments;
    std::istringstream stream(result);
    std::string segment;
    while (std::getline(stream, segment, '/')) {
        if (segment.empty() || segment == ".") {
            continue;
        }
        if (segment == "..") {
            return std::nullopt;
        }
        segments.push_back(std::move(segment));
    }

    result.clear();
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) result.push_back('/');
        result.append(segments[i]);
    }

    return result;
}

std::optional<std::string> NormalizePathForLock(std::string_view path) {
    // ロックキーは大文字小文字を区別しない（Windows ファイルシステムに合わせる）。
    auto normalized = NormalizePath(path);
    if (!normalized) {
        return std::nullopt;
    }
    std::string& s = *normalized;
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

std::string GetExtension(std::string_view path) {
    auto pos = path.rfind('.');
    if (pos == std::string_view::npos || pos == path.size() - 1) {
        return {};
    }
    auto slash = path.rfind('/');
    auto backslash = path.rfind('\\');
    size_t lastSep = std::string_view::npos;
    if (slash != std::string_view::npos) lastSep = slash;
    if (backslash != std::string_view::npos && (lastSep == std::string_view::npos || backslash > lastSep)) {
        lastSep = backslash;
    }
    if (lastSep != std::string_view::npos && pos < lastSep) {
        return {};
    }

    std::string ext(path.substr(pos));
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

}  // namespace witch::vfs::detail
