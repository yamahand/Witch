#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace witch::vfs {

// Vfs::Read の返り値。
struct FileData {
    std::vector<uint8_t> bytes;

    [[nodiscard]] bool Empty() const { return bytes.empty(); }
    [[nodiscard]] size_t Size() const { return bytes.size(); }
    [[nodiscard]] const uint8_t* Data() const { return bytes.data(); }

    // テキストデータとして文字列ビューを返す（ヌル終端なし）。
    [[nodiscard]] std::string_view AsStringView() const {
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }
};

}  // namespace witch::vfs
