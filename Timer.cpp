#include "timer.h"

Timer::Timer(int time)
	:time_{ time } {
}

void Timer::Reset(void) {
	count_ = 0;
}

void Timer::CountUp(void) {
	if (count_ >= time_) return;

	count_++;
}

bool Timer::IsReady(void) const {
	return (count_ >= time_);
}