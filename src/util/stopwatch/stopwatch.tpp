template<typename ClockT>
Stopwatch<ClockT>::Stopwatch() {
	reset();
}


template<typename ClockT>
void Stopwatch<ClockT>::pause() {
	if (!paused) {
		pause_time = ClockT::now();
		delta_t    = typename ClockT::duration(0);
		paused     = true;
	}
}


template<typename ClockT>
void Stopwatch<ClockT>::resume() {
	if (paused) {
		const auto resume_time = ClockT::now();

		pause_duration += resume_time - prev_time;

		prev_time  = resume_time;
		pause_time = typename ClockT::time_point();
		paused     = false;
	}
}


template<typename ClockT>
void Stopwatch<ClockT>::reset() {
	base_time      = ClockT::now();
	prev_time      = ClockT::now();
	total_t        = typename ClockT::duration(0);
	delta_t        = typename ClockT::duration(0);
	pause_duration = typename ClockT::duration(0);
	paused         = false;
}


template<typename ClockT>
void Stopwatch<ClockT>::tick() {
	const auto curr_time = ClockT::now();

	if (!paused)
		delta_t = curr_time - prev_time;
	else
		pause_duration += curr_time - prev_time; //pause duration is used to correct total time

	total_t   = curr_time - base_time;
	prev_time = curr_time;
}


template<typename ClockT>
template<typename PeriodT>
std::chrono::duration<double, PeriodT> Stopwatch<ClockT>::delta_time() const {
	return std::chrono::duration_cast<std::chrono::duration<double, PeriodT>>(delta_t);
}


template<typename ClockT>
template<typename PeriodT>
std::chrono::duration<double, PeriodT> Stopwatch<ClockT>::total_time() const {
	return std::chrono::duration_cast<std::chrono::duration<double, PeriodT>>(total_t - pause_duration);
}