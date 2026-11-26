#ifndef NOPEUI_PLATFORM_AUTOMATION_H_
#define NOPEUI_PLATFORM_AUTOMATION_H_

#include <Windows.h>

namespace nopeui {

class Element;
class Window;

// Returns true when the message was an accessibility request and result holds
// the value to hand back to the caller.
bool HandleAutomationGetObject(
    Window* window,
    HWND hwnd,
    WPARAM wparam,
    LPARAM lparam,
    LRESULT* result) noexcept;

void ReleaseAutomation(Window* window) noexcept;

// Raises UIA's focus changed event for the element, if a client is listening.
void NotifyAutomationFocus(Window* window, Element* element) noexcept;

}  // namespace nopeui

#endif  // NOPEUI_PLATFORM_AUTOMATION_H_
