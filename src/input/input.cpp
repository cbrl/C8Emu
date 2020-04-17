#include "input.h"


void Input::reset() noexcept {
    for (auto& [key, state] : key_states) {
        state = false;
    }
    on_keypress = nullptr;
}


void Input::setKeyState(Keys key, bool pressed) {
    key_states[key] = pressed;
    
    // Call the key event if the CPU is waiting for one
    if (pressed && on_keypress) {
        on_keypress(key);
        on_keypress = nullptr;
    }
}


bool Input::isKeyPressed(Keys key) noexcept {
    return key_states[key];
}


void Input::registerKeypressEvent(const std::function<void(Keys key)>& func) noexcept {
    on_keypress = func;
}