#pragma once

namespace witch::platform {

struct WindowParams {
    int width;
    int height;
    const char* title;
};

// Creates and shows the main window. Returns an opaque native handle (HWND on Windows).
void* CreateMainWindow(const WindowParams& params);

// Dispatches all pending OS messages.
// Returns false when a quit/close message has been received.
bool PumpMessages();

} // namespace witch::platform
