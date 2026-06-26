#include "WitchEngine/Core/ObjectRegistry.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Scene/GameObject.h"

namespace witch {

ObjectRegistry& ObjectRegistry::Instance() {
    static ObjectRegistry instance;
    return instance;
}

void ObjectRegistry::Register(std::string_view name, Factory factory) {
    factories_.emplace(std::string(name), std::move(factory));
}

std::unique_ptr<GameObject> ObjectRegistry::Create(std::string_view name) const {
    auto it = factories_.find(std::string(name));
    if (it == factories_.end()) {
        log::Error("ObjectRegistry: unknown type '{}'.", name);
        return nullptr;
    }
    return it->second();
}

} // namespace witch
