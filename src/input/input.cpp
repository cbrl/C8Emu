#include "input.h"


auto Input::reset() noexcept -> void {
    for (auto& [key, state] : key_states) {
        state = false;
    }
    on_keypress = nullptr;
}


auto Input::set_key_state(Keys key, bool pressed) -> void {
    key_states[key] = pressed;
    
    // Call the key event if the CPU is waiting for one
    if (pressed && on_keypress) {
        on_keypress(key);
        on_keypress = nullptr;
    }
}


auto Input::is_key_pressed(Keys key) noexcept -> bool {
    return key_states[key];
}


auto Input::register_keypress_event(const std::function<void(Keys key)>& func) noexcept -> void {
    on_keypress = func;
}
