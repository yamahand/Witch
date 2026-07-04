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

std::expected<void, std::string> Vfs::MountDisk(const std::filesystem::path& realPath) {
    if (sealed_) {
        log::Warn("VFS: MountDisk called after Seal");
        return std::unexpected(std::string("VFS is sealed"));
    }

    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(realPath, ec);
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
        if (it->canonicalPath == target) {
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
    writeDirSource_.reset();
}

std::expected<void, std::string> Vfs::SetWriteDir(const std::filesystem::path& realPath) {
    if (sealed_) {
        log::Warn("VFS: SetWriteDir called after Seal");
        return std::unexpected(std::string("VFS is sealed"));
    }

    std::error_code ec;
    std::filesystem::create_directories(realPath, ec);
    if (ec) return std::unexpected(std::format("create_directories failed: {}", ec.message()));

    auto canonical = std::filesystem::weakly_canonical(realPath, ec);
    if (ec) return std::unexpected(std::format("weakly_canonical failed: {}", ec.message()));

    writeDirSource_ = std::make_unique<DiskSource>(canonical);
    return {};
}

void Vfs::Seal() {
    sealed_ = true;
}

bool Vfs::IsSealed() const {
    return sealed_;
}

// パスごとに専用のミューテックスを持つとメモリが爆発するため、ストライプで近似する。
std::shared_mutex& Vfs::GetStripeLock(std::string_view normalizedPath) const {
    auto lockKey = detail::NormalizePathForLock(normalizedPath);
    size_t h = std::hash<std::string>{}(lockKey.value_or(""));
    return stripeLocks_[h % kStripeCount];
}

std::expected<FileData, std::string> Vfs::Read(std::string_view vfsPath) const {
    auto normalized = detail::NormalizePath(vfsPath);
    if (!normalized || normalized->empty()) {
        return std::unexpected(std::format("invalid path: {}", vfsPath));
    }

    std::shared_lock lock(GetStripeLock(*normalized));

    std::vector<uint8_t> raw;

    // IFileSource::ReadFile は失敗時に out をクリアする契約が無いため、
    // 呼び出し前に毎回クリアして前回の試行の残骸に依存しないようにする。
    // 書き込みディレクトリを最優先で参照し、次にマウント逆順（後マウントが優先）で探す。
    if (writeDirSource_) {
        raw.clear();
        if (writeDirSource_->ReadFile(*normalized, raw)) {
            return FileData{std::move(raw)};
        }
    }

    for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it) {
        raw.clear();
        if (it->source->ReadFile(*normalized, raw)) {
            return FileData{std::move(raw)};
        }
    }

    return std::unexpected(std::format("file not found: {}", *normalized));
}

bool Vfs::Exists(std::string_view vfsPath) const {
    auto normalized = detail::NormalizePath(vfsPath);
    if (!normalized || normalized->empty()) return false;

    std::shared_lock lock(GetStripeLock(*normalized));

    if (writeDirSource_ && writeDirSource_->Exists(*normalized)) {
        return true;
    }

    for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it) {
        if (it->source->Exists(*normalized)) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> Vfs::ListFiles(std::string_view vfsDir) const {
    auto normalized = detail::NormalizePath(vfsDir);
    if (!normalized) return {};

    std::shared_lock lock(GetStripeLock(*normalized));

    std::unordered_set<std::string> seen;
    std::vector<std::string> result;

    auto addFiles = [&](const std::vector<std::string>& files) {
        for (const auto& f : files) {
            auto lockKey = detail::NormalizePathForLock(f);
            if (lockKey && seen.insert(*lockKey).second) {
                result.push_back(f);
            }
        }
    };

    if (writeDirSource_) {
        addFiles(writeDirSource_->ListFiles(*normalized));
    }

    for (auto it = mounts_.rbegin(); it != mounts_.rend(); ++it) {
        addFiles(it->source->ListFiles(*normalized));
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::expected<void, std::string> Vfs::Write(std::string_view vfsPath, const void* data, size_t size) {
    if (!writeDirSource_) {
        return std::unexpected(std::string("no write directory configured"));
    }

    auto normalized = detail::NormalizePath(vfsPath);
    if (!normalized || normalized->empty()) {
        return std::unexpected(std::format("invalid path: {}", vfsPath));
    }

    std::unique_lock lock(GetStripeLock(*normalized));
    if (!writeDirSource_->WriteFile(*normalized, data, size)) {
        return std::unexpected(std::format("failed to write file: {}", *normalized));
    }
    return {};
}

std::expected<void, std::string> Vfs::Write(std::string_view vfsPath, std::span<const uint8_t> data) {
    return Write(vfsPath, data.data(), data.size());
}

std::string Vfs::Extension(std::string_view path) {
    return detail::GetExtension(path);
}

}  // namespace witch::vfs
