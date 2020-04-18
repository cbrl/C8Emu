#include "input.h"


void Input::reset() noexcept {
    for (auto& [key, state] : key_states) {
        state = false;
    }
    on_keypress = nullptr;
}


void Input::set_key_state(Keys key, bool pressed) {
    key_states[key] = pressed;
    
    // Call the key event if the CPU is waiting for one
    if (pressed && on_keypress) {
        on_keypress(key);
        on_keypress = nullptr;
    }
}


bool Input::is_key_pressed(Keys key) noexcept {
    return key_states[key];
}


void Input::register_keypress_event(const std::function<void(Keys key)>& func) noexcept {
    on_keypress = func;
}