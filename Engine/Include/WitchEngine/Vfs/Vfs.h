#pragma once

#include "WitchEngine/Vfs/DiskSource.h"
#include "WitchEngine/Vfs/FileData.h"
#include "WitchEngine/Vfs/IFileSource.h"

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace witch::vfs {

// 仮想ファイルシステム。複数のディスクパスをマウントし統一的に読み書きする。
// 読み取り: 後マウントが優先（マウント順によるオーバーライド）。
// 書き込み: SetWriteDir 先のみ。
//
// マウント構成 API（MountDisk/Unmount/UnmountAll/SetWriteDir/Seal）はシングルスレッドの
// 初期化フェーズでのみ呼び出すこと（内部状態をロック無しで変更するため）。
// Seal() 後は構成が確定し、Read/Exists/Write/ListFiles はスレッドセーフ。
class Vfs {
public:
    Vfs();
    ~Vfs();

    Vfs(const Vfs&) = delete;
    Vfs& operator=(const Vfs&) = delete;

    std::expected<void, std::string> MountDisk(const std::filesystem::path& realPath);
    std::expected<void, std::string> Unmount(const std::filesystem::path& path);
    void UnmountAll();
    // 書き込み先ディレクトリを設定する（存在しない場合は自動作成）。
    std::expected<void, std::string> SetWriteDir(const std::filesystem::path& realPath);

    // マウント構成を確定する。以降の Mount/Unmount/SetWriteDir 呼び出しは警告で無視される。
    void Seal();
    [[nodiscard]] bool IsSealed() const;

    [[nodiscard]] std::expected<FileData, std::string> Read(std::string_view vfsPath) const;
    [[nodiscard]] bool Exists(std::string_view vfsPath) const;
    [[nodiscard]] std::vector<std::string> ListFiles(std::string_view vfsDir) const;

    std::expected<void, std::string> Write(std::string_view vfsPath, const void* data, size_t size);
    std::expected<void, std::string> Write(std::string_view vfsPath, std::span<const uint8_t> data);

    [[nodiscard]] static std::string Extension(std::string_view path);

private:
    struct MountEntry {
        std::filesystem::path canonical_path;
        std::unique_ptr<IFileSource> source;
    };

    std::vector<MountEntry> mounts_;
    std::unique_ptr<DiskSource> write_dir_source_;
    bool sealed_ = false;

    static constexpr size_t kStripeCount = 256;
    mutable std::array<std::shared_mutex, kStripeCount> stripe_locks_;

    // GetStripeLock は Read/Exists/Write/ListFiles からのみ呼ばれ、それらは事前に不正/空パスを
    // 弾いているため、lockKey が nullopt になることは実質無い（到達時は "" にフォールバック）。
    [[nodiscard]] std::shared_mutex& GetStripeLock(std::string_view normalizedPath) const;
};

}  // namespace witch::vfs
