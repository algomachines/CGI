
#pragma once

#ifdef WIN32
#include "sysinfoapi.h"
#define CLOCK_REALTIME 0
struct timespec { int64_t tv_sec; int32_t tv_nsec; };    //header part
int clock_gettime(int, struct timespec* spec)      //C-file part
{
	__int64 wintime; GetSystemTimeAsFileTime((FILETIME*)&wintime);
	wintime -= 116444736000000000i64;  //1jan1601 to 1jan1970
	spec->tv_sec = wintime / 10000000i64;           //seconds
	spec->tv_nsec = wintime % 10000000i64 * 100;      //nano-seconds
	return 0;
}

bool get_file_time_ms(const char* file_name, uint64_t &t_ms)
{
	WIN32_FIND_DATA data;
	HANDLE h = FindFirstFile(file_name, &data);
	if (h == INVALID_HANDLE_VALUE)
		return false;

	FindClose(h);
	
	memmove(&t_ms, &data.ftLastWriteTime, sizeof(t_ms));

	t_ms -= 116444736000000000i64;  //1jan1601 to 1jan1970
	t_ms /= 10000i64; // ms

	return true;
}
#endif

inline double compute_delta_time_ms(const timespec &t0, const timespec &t1)
{	
	uint64_t t0_us = t0.tv_sec * 1000000LLU;
	t0_us += t0.tv_nsec / 1000;
	
	uint64_t t1_us = t1.tv_sec * 1000000LLU;
	t1_us += t1.tv_nsec / 1000;
	
	if (t1_us >= t0_us)
	{
		return (t1_us - t0_us)/1000.0;
	}
		
	return (t0_us - t1_us)/1000.0;
}

inline timespec *get_future_time(timespec &t, uint64_t sec, uint64_t nsec=0)
{
	clock_gettime(CLOCK_REALTIME, &t);
	
	t.tv_sec += sec;
	
	if (nsec == 0)
		return &t;
	
	uint64_t n = nsec + t.tv_nsec;
	if (n >= 1000000000)
	{
		t.tv_sec++;
		t.tv_nsec = n%1000000000;
	}
		
	return &t;
}

// Returns the current time as a unix time (seconds since Jan 1, 1970) multiplied by 1000 (resolution ms)
inline uint64_t get_time_ms(void)
{
	timespec t;
	clock_gettime(CLOCK_REALTIME, &t);

	uint64_t t_ms = t.tv_sec * 1000LLU;
	t_ms += t.tv_nsec / 1000000LLU;

	return t_ms;
}
