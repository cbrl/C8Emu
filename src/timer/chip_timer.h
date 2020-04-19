#pragma once

#include "util/stopwatch/stopwatch.h"


class Chip8Timer final {
public:
    Chip8Timer();

    //------------------------------------------------------------
    // Member Functions - Execution
    //------------------------------------------------------------

    // Update the timers
    void tick() noexcept;

    // Reset the timers
    void reset() noexcept;


    //------------------------------------------------------------
    // Member Functions - Delay Timer
    //------------------------------------------------------------

    // Get the delay timer value
    [[nodiscard]]
    uint8_t getDelay() const noexcept;

    // Set the delay timer
    void setDelay(uint8_t value) noexcept;


    //------------------------------------------------------------
    // Member Functions - Sound Timer
    //------------------------------------------------------------

    // Set the sound timer
    void setSound(uint8_t value) noexcept;

	// Returns true if sound should be produced
    [[nodiscard]]
	bool isSound() const noexcept;


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
