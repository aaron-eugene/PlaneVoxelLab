///////////////////////////////////////////////////////////////////////////////
// lab_debug/timing.h
// ==================
//
// Defines lightweight timing helpers for measuring temporary code sections.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <chrono>
#include <cstdio>

/***********************************************************
* Timing Types
************************************************************/

using TimingClock = std::chrono::steady_clock;
using TimingPoint = TimingClock::time_point;

/***********************************************************
* Timing Interface
************************************************************/

inline TimingPoint beginTiming()
{
	return TimingClock::now();
}

inline double getElapsedMilliseconds(
	const TimingPoint& start)
{
	const TimingPoint end =
		TimingClock::now();

	const std::chrono::duration<double, std::milli> elapsed =
		end - start;

	return elapsed.count();
}

inline void printElapsedMilliseconds(
	const char* label,
	const TimingPoint& start)
{
	std::printf(
		"%s: %.3f ms\n",
		label,
		getElapsedMilliseconds(start));
}
