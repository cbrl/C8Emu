#include "chip_timer.h"


Chip8Timer::Chip8Timer() {
    reset();
}

auto Chip8Timer::pause() noexcept -> void {
	stopwatch.pause();
}

auto Chip8Timer::resume() noexcept -> void {
	stopwatch.resume();
}

auto Chip8Timer::reset() noexcept -> void {
	stopwatch.reset();
	elapsed_time = std::chrono::duration<float>{0.0};
    delay_timer = 0;
    sound_timer = 0; 
}

auto Chip8Timer::tick() noexcept -> void {
	using namespace std::chrono_literals;
	static constexpr std::chrono::duration<float> clock{1.0 / 60.0};
    

	stopwatch.tick();
	elapsed_time += stopwatch.delta_time();

	if (elapsed_time >= clock) {
		if (delay_timer > 0) {
			--delay_timer;
		}
		if (sound_timer > 0) {
			--sound_timer;
		}
		
		elapsed_time = std::chrono::duration<float>{0.0};
	}
}

auto Chip8Timer::get_delay() const noexcept -> uint8_t {
	return delay_timer;
}

auto Chip8Timer::set_delay(uint8_t value) noexcept -> void {
	delay_timer = value;
}

auto Chip8Timer::set_sound(uint8_t value) noexcept -> void {
	sound_timer = value;
}

auto Chip8Timer::is_sound() const noexcept -> bool {
	return sound_timer > 0;
}
