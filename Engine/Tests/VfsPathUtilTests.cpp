// VFS パス正規化ユーティリティ（非公開ヘッダ Vfs/VfsPathUtil.h）のテスト。
// 純関数のため最初のテスト対象として最適。仕様はヘッダコメント。
#include "Vfs/VfsPathUtil.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace witch::vfs::detail;

TEST_CASE("NormalizePath unifies separators and strips leading slashes", "[VfsPathUtil]") {
    CHECK(NormalizePath("Assets\\Textures\\player.png") == "Assets/Textures/player.png");
    CHECK(NormalizePath("/Assets/player.png") == "Assets/player.png");
    CHECK(NormalizePath("//Assets//player.png") == "Assets/player.png");
    CHECK(NormalizePath("Assets/player.png") == "Assets/player.png");
}

TEST_CASE("NormalizePath removes '.' segments", "[VfsPathUtil]") {
    CHECK(NormalizePath("./Assets/./player.png") == "Assets/player.png");
    CHECK(NormalizePath(".") == "");
}

TEST_CASE("NormalizePath rejects traversal", "[VfsPathUtil]") {
    // ".." はトラバーサル攻撃の可能性があるため nullopt（打ち消し合う場合も拒否）。
    CHECK_FALSE(NormalizePath("../secret").has_value());
    CHECK_FALSE(NormalizePath("Assets/../player.png").has_value());
    CHECK_FALSE(NormalizePath("Assets\\..\\player.png").has_value());
}

TEST_CASE("NormalizePath keeps case and handles empty input", "[VfsPathUtil]") {
    CHECK(NormalizePath("Assets/Player.PNG") == "Assets/Player.PNG");
    CHECK(NormalizePath("") == "");
    CHECK(NormalizePath("///") == "");
}

TEST_CASE("NormalizePathForLock lowercases", "[VfsPathUtil]") {
    CHECK(NormalizePathForLock("Assets\\Player.PNG") == "assets/player.png");
    CHECK_FALSE(NormalizePathForLock("../secret").has_value());
}

TEST_CASE("GetExtension returns lowercase extension with dot", "[VfsPathUtil]") {
    CHECK(GetExtension("player.png") == ".png");
    CHECK(GetExtension("Assets/Player.PNG") == ".png");
    CHECK(GetExtension("archive.tar.gz") == ".gz");
    CHECK(GetExtension("Assets\\player.PnG") == ".png");
}

TEST_CASE("GetExtension returns empty when there is no extension", "[VfsPathUtil]") {
    CHECK(GetExtension("noext") == "");
    CHECK(GetExtension("dir.v2/file") == "");   // ドットがディレクトリ側にある
    CHECK(GetExtension(".gitignore") == "");    // 隠しファイルは拡張子なし扱い
    CHECK(GetExtension("dir/.hidden") == "");
    CHECK(GetExtension("file.") == "");         // 末尾ドット
    CHECK(GetExtension("") == "");
}

}  // namespace
