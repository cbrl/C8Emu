#pragma once

#include <chrono>

template<typename ClockT = std::chrono::steady_clock>
class Stopwatch {
public:
	using clock_t = ClockT;

	Stopwatch() {
		reset();
	}

	// Pause the timer
	auto pause() -> void {
		if (!paused) {
			pause_time = ClockT::now();
			delta_t    = typename ClockT::duration(0);
			paused     = true;
		}
	}

	// Start the timer (if it's paused)
	auto resume() -> void {
		if (paused) {
			const auto resume_time = ClockT::now();

			pause_duration += resume_time - prev_time;

			prev_time  = resume_time;
			pause_time = typename ClockT::time_point();
			paused     = false;
		}
	}

	// Reset the timer
	auto reset() -> void {
		base_time      = ClockT::now();
		prev_time      = ClockT::now();
		total_t        = typename ClockT::duration(0);
		delta_t        = typename ClockT::duration(0);
		pause_duration = typename ClockT::duration(0);
		paused         = false;
	}

	// Update the timer
	auto tick() -> void {
		const auto curr_time = ClockT::now();

		if (!paused)
			delta_t = curr_time - prev_time;
		else
			pause_duration += curr_time - prev_time; //pause duration is used to correct total time

		total_t   = curr_time - base_time;
		prev_time = curr_time;
	}

	// Get the time elapsed since the last update in the specified units (default: econds)
	template<typename PeriodT = std::ratio<1>>
	[[nodiscard]]
	auto delta_time() const -> std::chrono::duration<double, PeriodT> {
		return std::chrono::duration_cast<std::chrono::duration<double, PeriodT>>(delta_t);
	}

	// Get the time elapsed since the timer was created or reset in the specified units (default: seconds)
	template<typename PeriodT = std::ratio<1>>
	[[nodiscard]]
	auto total_time() const -> std::chrono::duration<double, PeriodT> {
		return std::chrono::duration_cast<std::chrono::duration<double, PeriodT>>(total_t - pause_duration);
	}

private:

	bool paused = false;

	using time_point = typename ClockT::time_point;
	using duration   = typename ClockT::duration;

	time_point base_time;
	time_point prev_time;
	duration   delta_t;
	duration   total_t;

	time_point pause_time;
	duration   pause_duration;
};
