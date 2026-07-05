#include "WitchEngine/Vfs/DiskSource.h"

#include "Vfs/VfsPathUtil.h"
#include "WitchEngine/Core/Logger.h"

#include <format>
#include <fstream>

namespace witch::vfs {

DiskSource::DiskSource(std::filesystem::path rootPath) {
    std::error_code ec;
    root_ = std::filesystem::weakly_canonical(rootPath, ec);
    if (ec) root_ = std::move(rootPath);
}

std::optional<std::filesystem::path> DiskSource::SafeResolve(
    std::string_view normalizedPath) const {
    if (normalizedPath.empty()) {
        return std::nullopt;
    }

    auto resolved = (root_ / std::filesystem::path(std::string(normalizedPath)))
                        .lexically_normal();

    // ".." によるディレクトリトラバーサルを字句レベルで弾く。
    auto rel = resolved.lexically_relative(root_);
    if (rel.empty() || *rel.begin() == "..") {
        return std::nullopt;
    }

    // シンボリックリンクで root_ 外に飛ばせないよう、canonical パスでも確認する。
    std::error_code ec;
    if (std::filesystem::exists(resolved, ec)) {
        auto realResolved = std::filesystem::canonical(resolved, ec);
        if (ec) return std::nullopt;
        auto realRoot = std::filesystem::canonical(root_, ec);
        if (ec) return std::nullopt;

        auto realRel = realResolved.lexically_relative(realRoot);
        if (realRel.empty() || *realRel.begin() == "..") {
            return std::nullopt;
        }
        return realResolved;
    }

    return resolved;
}

bool DiskSource::Exists(std::string_view normalizedPath) const {
    auto resolved = SafeResolve(normalizedPath);
    if (!resolved) return false;
    std::error_code ec;
    return std::filesystem::exists(*resolved, ec);
}

bool DiskSource::ReadFile(std::string_view normalizedPath,
                          std::vector<uint8_t>& out) const {
    auto resolved = SafeResolve(normalizedPath);
    if (!resolved) return false;

    std::ifstream ifs(*resolved, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;

    auto size = ifs.tellg();
    if (size < 0) return false;

    constexpr auto kMaxFileSize = static_cast<std::streamoff>(256 * 1024 * 1024);
    if (size > kMaxFileSize) {
        log::Error("VFS ReadFile: file exceeds max size ({} bytes): {}",
                   static_cast<int64_t>(size), normalizedPath);
        return false;
    }

    ifs.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(size));
    if (size > 0) {
        ifs.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
        if (!ifs.good()) {
            out.clear();
            return false;
        }
    }
    return true;
}

std::vector<std::string> DiskSource::ListFiles(std::string_view normalizedDir) const {
    auto dirPath = normalizedDir.empty()
                       ? root_
                       : SafeResolve(normalizedDir).value_or(std::filesystem::path{});
    std::error_code dirEc;
    if (dirPath.empty() || !std::filesystem::is_directory(dirPath, dirEc)) {
        return {};
    }

    std::vector<std::string> result;
    std::error_code ec;
    auto it = std::filesystem::directory_iterator(dirPath, ec);
    if (ec) return {};
    const std::filesystem::directory_iterator end;
    // range-for は内部で非 ec 版の operator++ を呼び、走査中の OS エラー（同時削除・権限変更等）
    // で例外を送出し得る。明示的に increment(ec) を使い、例外を ListFiles の外へ漏らさない。
    for (; it != end; it.increment(ec)) {
        if (ec) break;  // 走査中エラーは打ち切り、それまでの結果を返す
        const auto& entry = *it;
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc)) continue;

        // WriteFile が生成する中間ファイル（.tmp / .old）は列挙に含めない。
        // クラッシュや別スレッド Write との競合で残り得るため、呼び出し元へは見せない。
        auto ext = entry.path().extension();
        if (ext == ".tmp" || ext == ".old") continue;

        std::error_code relEc;
        auto rel = std::filesystem::relative(entry.path(), root_, relEc);
        if (!relEc) {
            auto normalized = detail::NormalizePath(rel.generic_string());
            if (normalized) {
                result.push_back(std::move(*normalized));
            }
        }
    }
    return result;
}

std::expected<void, std::string> DiskSource::WriteFile(std::string_view normalizedPath, const void* data,
                           size_t size) {
    auto resolved = SafeResolve(normalizedPath);
    if (!resolved) {
        return std::unexpected(std::format("invalid path: {}", normalizedPath));
    }

    const auto& targetPath = *resolved;

    std::error_code ec;
    if (targetPath.has_parent_path()) {
        // create_directories より前に、既存の祖先ディレクトリが root_ 配下であることを確認する
        // （symlink で root_ 外を指す中間ディレクトリを辿って外部にディレクトリを作られるのを防ぐ）。
        std::filesystem::path existingAncestor = targetPath.parent_path();
        std::error_code existsEc;
        while (!existingAncestor.empty() && !std::filesystem::exists(existingAncestor, existsEc)) {
            existingAncestor = existingAncestor.parent_path();
        }
        if (!existingAncestor.empty()) {
            auto realAncestor = std::filesystem::canonical(existingAncestor, ec);
            if (ec) return std::unexpected(std::format("canonical failed: {}", ec.message()));
            auto realRootForAncestor = std::filesystem::canonical(root_, ec);
            if (ec) return std::unexpected(std::format("canonical failed: {}", ec.message()));
            auto ancestorRel = realAncestor.lexically_relative(realRootForAncestor);
            if (ancestorRel.empty() || *ancestorRel.begin() == "..") {
                return std::unexpected(std::string("parent directory escapes root via symlink"));
            }
        }

        std::filesystem::create_directories(targetPath.parent_path(), ec);
        if (ec) return std::unexpected(std::format("create_directories failed: {}", ec.message()));

        // シンボリックリンク対策: 作成後の親ディレクトリが依然として root_ 配下にあることを確認する。
        auto realParent = std::filesystem::canonical(targetPath.parent_path(), ec);
        if (ec) return std::unexpected(std::format("canonical failed: {}", ec.message()));
        auto realRoot = std::filesystem::canonical(root_, ec);
        if (ec) return std::unexpected(std::format("canonical failed: {}", ec.message()));
        auto parentRel = realParent.lexically_relative(realRoot);
        if (parentRel.empty() || *parentRel.begin() == "..") {
            return std::unexpected(std::string("parent directory escapes root via symlink"));
        }
    }

    // .tmp に書いてからアトミックに rename することでファイル破損を防ぐ。
    auto tmpPath = targetPath; tmpPath += ".tmp";

    {
        std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            return std::unexpected(std::format("failed to open tmp file: {}", tmpPath.string()));
        }
        if (size > 0) {
            ofs.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        }
        if (!ofs.good()) {
            std::filesystem::remove(tmpPath, ec);
            return std::unexpected(std::format("failed to write tmp file: {}", tmpPath.string()));
        }
    }

    if (std::filesystem::exists(targetPath, ec)) {
        auto oldPath = targetPath; oldPath += ".old";

        if (std::filesystem::exists(oldPath, ec)) {
            std::filesystem::remove(oldPath, ec);
        }

        std::filesystem::rename(targetPath, oldPath, ec);
        if (ec) {
            auto message = std::format("failed to rename target to .old: {}", ec.message());
            std::error_code cleanupEc;
            std::filesystem::remove(tmpPath, cleanupEc);
            return std::unexpected(std::move(message));
        }

        std::filesystem::rename(tmpPath, targetPath, ec);
        if (ec) {
            log::Error("VFS WriteFile: failed to rename tmp to target, attempting rollback");
            std::error_code restoreEc;
            std::filesystem::rename(oldPath, targetPath, restoreEc);
            if (restoreEc) {
                log::Error("VFS WriteFile: rollback failed, .old and .tmp files preserved");
            } else {
                std::filesystem::remove(tmpPath, restoreEc);
            }
            return std::unexpected(std::format("failed to rename tmp to target: {}", ec.message()));
        }

        std::filesystem::remove(oldPath, ec);
    } else {
        std::filesystem::rename(tmpPath, targetPath, ec);
        if (ec) {
            auto message = std::format("failed to rename tmp to target: {}", ec.message());
            std::error_code cleanupEc;
            std::filesystem::remove(tmpPath, cleanupEc);
            return std::unexpected(std::move(message));
        }
    }

    return {};
}

}  // namespace witch::vfs
