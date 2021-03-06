#include "chip_timer.h"


Chip8Timer::Chip8Timer() {
    reset();
}

void Chip8Timer::pause() noexcept {
	stopwatch.pause();
}

void Chip8Timer::resume() noexcept {
	stopwatch.resume();
}

void Chip8Timer::reset() noexcept {
	stopwatch.reset();
	elapsed_time = std::chrono::duration<float>{0.0};
    delay_timer = 0;
    sound_timer = 0; 
}

void Chip8Timer::tick() noexcept {
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

uint8_t Chip8Timer::get_delay() const noexcept {
	return delay_timer;
}

void Chip8Timer::set_delay(uint8_t value) noexcept {
	delay_timer = value;
}

void Chip8Timer::set_sound(uint8_t value) noexcept {
	sound_timer = value;
}

bool Chip8Timer::is_sound() const noexcept {
	return sound_timer > 0;
}