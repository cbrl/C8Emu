#pragma once

#include <chrono>

template<typename ClockT = std::chrono::steady_clock>
class Stopwatch {
public:
	using clock_t = ClockT;

	Stopwatch();

	// Pause the timer
	void pause();

	// Start the timer (if it's paused)
	void resume();

	// Reset the timer
	void reset();

	// Update the timer
	void tick();

	// Get the time elapsed since the last update in the specified units (default: seconds)
	template<typename PeriodT = std::ratio<1>>
	[[nodiscard]]
	std::chrono::duration<double, PeriodT> deltaTime() const;

	// Get the time elapsed since the timer was created or reset in the specified units (default: seconds)
	template<typename PeriodT = std::ratio<1>>
	[[nodiscard]]
	std::chrono::duration<double, PeriodT> totalTime() const;

private:

	bool paused = false;

	using time_point = typename ClockT::time_point;
	using duration   = typename ClockT::duration;

	time_point base_time;
	time_point prev_time;
	duration   delta_time;
	duration   total_time;

	time_point pause_time;
	duration   pause_duration;
};

#include "stopwatch.tpp"