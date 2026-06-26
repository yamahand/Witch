#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace witch {

class GameObject;

class ObjectRegistry {
public:
    using Factory = std::function<std::unique_ptr<GameObject>()>;

    static ObjectRegistry& Instance();

    void Register(std::string_view name, Factory factory);
    std::unique_ptr<GameObject> Create(std::string_view name) const;

private:
    ObjectRegistry() = default;
    std::unordered_map<std::string, Factory> factories_;
};

} // namespace witch

// Place WITCH_REGISTER_OBJECT(MyType) in the .cpp that defines MyType.
#define WITCH_REGISTER_OBJECT(Type)                                         \
    static const bool kRegistered_##Type = [] {                             \
        witch::ObjectRegistry::Instance().Register(                         \
            #Type, [] { return std::make_unique<Type>(); });                \
        return true;                                                         \
    }()
