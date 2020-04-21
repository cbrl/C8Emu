#pragma once

#include "util/stopwatch/stopwatch.h"


class Chip8Timer final {
public:
    Chip8Timer();

    //------------------------------------------------------------
    // Member Functions - Execution
    //------------------------------------------------------------

    /// Update the timers
    void tick() noexcept;

    /// Pause the timers
    void pause() noexcept;

    /// Resume the timers
    void resume() noexcept;

    // Reset the timers
    void reset() noexcept;


    //------------------------------------------------------------
    // Member Functions - Delay Timer
    //------------------------------------------------------------

    /**
     * @brief Get the delay timer value
     * @return The number of ticks before the delay timer hits 0
     */
    [[nodiscard]]
    uint8_t get_delay() const noexcept;

    /**
     * @brief Set the delay timer
     * 
     * @details The delay timer counts down from a user-provided value
     *          to 0 at a rate of 60Hz
     * 
     * @param[in] value  The number of ticks to count down from
     */
    void set_delay(uint8_t value) noexcept;


    //------------------------------------------------------------
    // Member Functions - Sound Timer
    //------------------------------------------------------------

    /**
     * @brief Set the sound timer
     * 
     * @details The sound timer counts down from a user-provided value
     *          to 0 at a rate of 60Hz. While the count is non-zero,
     *          a sound will be produced.
     * 
     * @param[in] value  The number of ticks to count down from
     */
    void set_sound(uint8_t value) noexcept;

	/**
     * @brief Determine if a sound should be produced
     * @return True if sound should be produced
     */
    [[nodiscard]]
	bool is_sound() const noexcept;

private:

    //------------------------------------------------------------
    // Member Variables
    //------------------------------------------------------------

    // The stopwatch used to keep time
    Stopwatch<> stopwatch;

    // The elapsed time
    std::chrono::duration<float> elapsed_time;

    // Delay timer. Used for timing events.
	uint8_t delay_timer;

    // Sound timer. Creates sound when the value is non-zero.
	uint8_t sound_timer;
};
