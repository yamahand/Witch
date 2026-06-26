#pragma once

namespace witch {

class IRenderer;
class Time;

// Non-owning view of engine-owned service instances.
// Engine sets each pointer during Init (in creation order) and clears them during Shutdown.
struct Services {
    IRenderer* renderer = nullptr;
    Time* time = nullptr;

    static Services& Instance() {
        static Services s;
        return s;
    }

private:
    Services() = default;
};

} // namespace witch
