#include "WitchEngine/Vfs/Vfs.h"

#include "WitchEngine/Vfs/VfsPathUtil.h"
#include "WitchEngine/Core/Logger.h"

#include <algorithm>
#include <format>
#include <functional>
#include <unordered_set>

namespace witch::vfs {

Vfs::Vfs() = default;
Vfs::~Vfs() = default;

std::expected<void, std::string> Vfs::MountDisk(const std::filesystem::path& real_path) {
    if (sealed_) {
        log::Warn("VFS: MountDisk called after Seal");
        return std::unexpected(std::string("VFS is sealed"));
    }

    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(real_path, ec);
    if (ec) return std::unexpected(std::format("weakly_canonical failed: {}", ec.message()));

    if (!std::filesystem::is_directory(canonical, ec)) {
        return std::unexpected(std::format("not a directory: {}", canonical.string()));
    }

    auto source = std::make_unique<DiskSource>(canonical);
    mounts_.push_back({std::move(canonical), std::move(source)});
    return {};
}

std::expected<void, std::string> Vfs::Unmount(const std::filesystem::path& path) {
    if (sealed_) {
        log::Warn("VFS: Unmount called after Seal");
        return std::unexpected(std::string("VFS is sealed"));
    }

    std::error_code ec;
    auto target = std::filesystem::weakly_canonical(path, ec);
    if (ec) return std::unexpected(std::format("weakly_canonical failed: {}", ec.message()));

    for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it) {
        if (it->canonical_path == target) {
            mounts_.erase(std::next(it).base());
            return {};
        }
    }
    return std::unexpected(std::format("not mounted: {}", target.string()));
}

void Vfs::UnmountAll() {
    if (sealed_) {
        log::Warn("VFS: UnmountAll called after Seal");
        return;
    }
    mounts_.clear();
    write_dir_source_.reset();
}

std::expected<void, std::string> Vfs::SetWriteDir(const std::filesystem::path& real_path) {
    if (sealed_) {
        log::Warn("VFS: SetWriteDir called after Seal");
        return std::unexpected(std::string("VFS is sealed"));
    }

    std::error_code ec;
    std::filesystem::create_directories(real_path, ec);

    auto canonical = std::filesystem::weakly_canonical(real_path, ec);
    if (ec) return std::unexpected(std::format("weakly_canonical failed: {}", ec.message()));

    write_dir_source_ = std::make_unique<DiskSource>(canonical);
    return {};
}

void Vfs::Seal() {
    sealed_ = true;
}

bool Vfs::IsSealed() const {
    return sealed_;
}

// パスごとに専用のミューテックスを持つとメモリが爆発するため、ストライプで近似する。
std::shared_mutex& Vfs::GetStripeLock(std::string_view normalized_path) const {
    auto lock_key = detail::NormalizePathForLock(normalized_path);
    size_t h = std::hash<std::string>{}(lock_key.value_or(""));
    return stripe_locks_[h % kStripeCount];
}

std::expected<FileData, std::string> Vfs::Read(std::string_view vfs_path) const {
    auto normalized = detail::NormalizePath(vfs_path);
    if (!normalized || normalized->empty()) {
        return std::unexpected(std::format("invalid path: {}", vfs_path));
    }

    std::shared_lock lock(GetStripeLock(*normalized));

    std::vector<uint8_t> raw;

    // 書き込みディレクトリを最優先で参照し、次にマウント逆順（後マウントが優先）で探す。
    if (write_dir_source_ && write_dir_source_->ReadFile(*normalized, raw)) {
        return FileData{std::move(raw)};
    }

    for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it) {
        if (it->source->ReadFile(*normalized, raw)) {
            return FileData{std::move(raw)};
        }
    }

    return std::unexpected(std::format("file not found: {}", *normalized));
}

bool Vfs::Exists(std::string_view vfs_path) const {
    auto normalized = detail::NormalizePath(vfs_path);
    if (!normalized || normalized->empty()) return false;

    std::shared_lock lock(GetStripeLock(*normalized));

    if (write_dir_source_ && write_dir_source_->Exists(*normalized)) {
        return true;
    }

    for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it) {
        if (it->source->Exists(*normalized)) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> Vfs::ListFiles(std::string_view vfs_dir) const {
    auto normalized = detail::NormalizePath(vfs_dir);
    if (!normalized) return {};

    std::unordered_set<std::string> seen;
    std::vector<std::string> result;

    auto add_files = [&](const std::vector<std::string>& files) {
        for (const auto& f : files) {
            auto lock_key = detail::NormalizePathForLock(f);
            if (lock_key && seen.insert(*lock_key).second) {
                result.push_back(f);
            }
        }
    };

    if (write_dir_source_) {
        add_files(write_dir_source_->ListFiles(*normalized));
    }

    for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it) {
        add_files(it->source->ListFiles(*normalized));
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::expected<void, std::string> Vfs::Write(std::string_view vfs_path, const void* data, size_t size) {
    if (!write_dir_source_) {
        return std::unexpected(std::string("no write directory configured"));
    }

    auto normalized = detail::NormalizePath(vfs_path);
    if (!normalized || normalized->empty()) {
        return std::unexpected(std::format("invalid path: {}", vfs_path));
    }

    std::unique_lock lock(GetStripeLock(*normalized));
    if (!write_dir_source_->WriteFile(*normalized, data, size)) {
        return std::unexpected(std::format("failed to write file: {}", *normalized));
    }
    return {};
}

std::expected<void, std::string> Vfs::Write(std::string_view vfs_path, std::span<const uint8_t> data) {
    return Write(vfs_path, data.data(), data.size());
}

std::string Vfs::Extension(std::string_view path) {
    return detail::GetExtension(path);
}

}  // namespace witch::vfs
