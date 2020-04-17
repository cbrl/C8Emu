#pragma once

#include <cstdint>
#include <map>
#include <functional>


/**
 * @enum Keys
 * @brief Represents each key in the CHIP-8 specification
 */
enum class Keys : uint8_t {
    Key0 = 0x0,
    Key1 = 0x1,
    Key2 = 0x2,
    Key3 = 0x3,
    Key4 = 0x4,
    Key5 = 0x5,
    Key6 = 0x6,
    Key7 = 0x7,
    Key8 = 0x8,
    Key9 = 0x9,
    KeyA = 0xA,
    KeyB = 0xB,
    KeyC = 0xC,
    KeyD = 0xD,
    KeyE = 0xE,
    KeyF = 0xF
};


/**
 * @class Input
 * @brief Stores the CHIP-8 key states, and optionally a function to be called on a key press.
 */
class Input final {
public:
    /// Reset the state of all keys
    void reset() noexcept;

    /**
     * @brief Set the state of a key
     *
     * @param[in] key      The key to set the state of
     * @param[in] pressed  The state of the key
     */
    void setKeyState(Keys key, bool pressed);

    /**
     * @brief Query the state of a key
     * 
     * @param[in] key  The key to get the state of
     */
    bool isKeyPressed(Keys key) noexcept;

    /**
     * @brief Provide a function to be called when a key is pressed
     * 
     * @param[in] func  The function to be called on keypress
     */
    void registerKeypressEvent(const std::function<void(Keys key)>& func) noexcept;

private:

    // Function called when a key is pressed
    std::function<void(Keys key)> on_keypress;

    // Key state map
    std::map<Keys, bool> key_states = {
        {Keys::Key0, false},
        {Keys::Key1, false},
        {Keys::Key2, false},
        {Keys::Key3, false},
        {Keys::Key4, false},
        {Keys::Key5, false},
        {Keys::Key6, false},
        {Keys::Key7, false},
        {Keys::Key8, false},
        {Keys::Key9, false},
        {Keys::KeyA, false},
        {Keys::KeyB, false},
        {Keys::KeyC, false},
        {Keys::KeyD, false},
        {Keys::KeyE, false},
        {Keys::KeyF, false},
    };
};