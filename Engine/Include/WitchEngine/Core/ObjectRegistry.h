#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace witch {

class GameObject;

/// 文字列型名から GameObject を生成するファクトリ。
/// C++ にリフレクションがないため、レベルファイル上の型名文字列から実体を作るために必要。
class ObjectRegistry {
public:
    using Factory = std::function<std::unique_ptr<GameObject>()>;

    static ObjectRegistry& Instance();

    /// 型名と生成関数を登録する。通常は WITCH_REGISTER_OBJECT マクロ経由で呼ぶ。
    void Register(std::string_view name, Factory factory);
    /// 登録済みの型名からオブジェクトを生成する。未登録の場合は nullptr を返す。
    std::unique_ptr<GameObject> Create(std::string_view name) const;

private:
    ObjectRegistry() = default;
    std::unordered_map<std::string, Factory> factories_;
};

} // namespace witch

/// クラス名と異なる登録名で ObjectRegistry へ自動登録する。レベルエディタ上の
/// identifier（例: "Player"）と C++ クラス名（例: PlayerObject）を分離したいときに使う。
/// static 変数の初期化（main より前）を利用してプログラム起動時に登録が完了する。
#define WITCH_REGISTER_OBJECT_AS(Type, Name)                                \
    static const bool kRegistered_##Type = [] {                             \
        witch::ObjectRegistry::Instance().Register(                         \
            Name, [] { return std::make_unique<Type>(); });                 \
        return true;                                                         \
    }()

/// 型を定義している .cpp に 1 行書くだけで、クラス名そのままの登録名で自動登録する。
#define WITCH_REGISTER_OBJECT(Type) WITCH_REGISTER_OBJECT_AS(Type, #Type)
