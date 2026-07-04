#include "WitchEngine/Vfs/DiskSource.h"

#include "WitchEngine/Vfs/VfsPathUtil.h"
#include "WitchEngine/Core/Logger.h"

#include <fstream>

namespace witch::vfs {

DiskSource::DiskSource(std::filesystem::path root_path) {
    std::error_code ec;
    root_ = std::filesystem::weakly_canonical(std::move(root_path), ec);
    if (ec) root_ = std::move(root_path);
}

std::optional<std::filesystem::path> DiskSource::SafeResolve(
    std::string_view normalized_path) const {
    if (normalized_path.empty()) {
        return std::nullopt;
    }

    auto resolved = (root_ / std::filesystem::path(std::string(normalized_path)))
                        .lexically_normal();

    // ".." によるディレクトリトラバーサルを字句レベルで弾く。
    auto rel = resolved.lexically_relative(root_);
    if (rel.empty() || *rel.begin() == "..") {
        return std::nullopt;
    }

    // シンボリックリンクで root_ 外に飛ばせないよう、canonical パスでも確認する。
    std::error_code ec;
    if (std::filesystem::exists(resolved, ec)) {
        auto real_resolved = std::filesystem::canonical(resolved, ec);
        if (ec) return std::nullopt;
        auto real_root = std::filesystem::canonical(root_, ec);
        if (ec) return std::nullopt;

        auto real_rel = real_resolved.lexically_relative(real_root);
        if (real_rel.empty() || *real_rel.begin() == "..") {
            return std::nullopt;
        }
        return real_resolved;
    }

    return resolved;
}

bool DiskSource::Exists(std::string_view normalized_path) const {
    auto resolved = SafeResolve(normalized_path);
    if (!resolved) return false;
    std::error_code ec;
    return std::filesystem::exists(*resolved, ec);
}

bool DiskSource::ReadFile(std::string_view normalized_path,
                          std::vector<uint8_t>& out) const {
    auto resolved = SafeResolve(normalized_path);
    if (!resolved) return false;

    std::ifstream ifs(*resolved, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;

    auto size = ifs.tellg();
    if (size < 0) return false;

    constexpr auto kMaxFileSize = static_cast<std::streamoff>(256 * 1024 * 1024);
    if (size > kMaxFileSize) return false;

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

std::vector<std::string> DiskSource::ListFiles(std::string_view normalized_dir) const {
    auto dir_path = normalized_dir.empty()
                        ? root_
                        : SafeResolve(normalized_dir).value_or(std::filesystem::path{});
    std::error_code dir_ec;
    if (dir_path.empty() || !std::filesystem::is_directory(dir_path, dir_ec)) {
        return {};
    }

    std::vector<std::string> result;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (entry.is_regular_file()) {
            auto rel = std::filesystem::relative(entry.path(), root_, ec);
            if (!ec) {
                auto normalized = detail::NormalizePath(rel.generic_string());
                if (normalized) {
                    result.push_back(std::move(*normalized));
                }
            }
        }
    }
    return result;
}

bool DiskSource::WriteFile(std::string_view normalized_path, const void* data,
                           size_t size) {
    auto resolved = SafeResolve(normalized_path);
    if (!resolved) return false;

    const auto& target_path = *resolved;

    std::error_code ec;
    if (target_path.has_parent_path()) {
        std::filesystem::create_directories(target_path.parent_path(), ec);
        if (ec) return false;

        // Symlink protection: verify parent directory is still under root
        auto real_parent = std::filesystem::canonical(target_path.parent_path(), ec);
        if (ec) return false;
        auto real_root = std::filesystem::canonical(root_, ec);
        if (ec) return false;
        auto parent_rel = real_parent.lexically_relative(real_root);
        if (parent_rel.empty() || *parent_rel.begin() == "..") return false;
    }

    // .tmp に書いてからアトミックに rename することでファイル破損を防ぐ。
    auto tmp_path = target_path; tmp_path += ".tmp";

    {
        std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) return false;
        if (size > 0) {
            ofs.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        }
        if (!ofs.good()) {
            std::filesystem::remove(tmp_path, ec);
            return false;
        }
    }

    if (std::filesystem::exists(target_path, ec)) {
        auto old_path = target_path; old_path += ".old";

        if (std::filesystem::exists(old_path, ec)) {
            std::filesystem::remove(old_path, ec);
        }

        std::filesystem::rename(target_path, old_path, ec);
        if (ec) {
            std::filesystem::remove(tmp_path, ec);
            return false;
        }

        std::filesystem::rename(tmp_path, target_path, ec);
        if (ec) {
            log::Error("VFS WriteFile: failed to rename tmp to target, attempting rollback");
            std::error_code restore_ec;
            std::filesystem::rename(old_path, target_path, restore_ec);
            if (restore_ec) {
                log::Error("VFS WriteFile: rollback failed, .old and .tmp files preserved");
            } else {
                std::filesystem::remove(tmp_path, restore_ec);
            }
            return false;
        }

        std::filesystem::remove(old_path, ec);
    } else {
        std::filesystem::rename(tmp_path, target_path, ec);
        if (ec) {
            std::filesystem::remove(tmp_path, ec);
            return false;
        }
    }

    return true;
}

}  // namespace witch::vfs
