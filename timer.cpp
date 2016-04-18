#include "timer.h"
#include <sys/time.h>
#include <stdio.h>

bool print_timing = false;

Timer::Timer()
{
	if (print_timing) {
		t1 = new timeval;
		gettimeofday(t1, NULL);
	} else
		t1 = NULL;
}

Timer::~Timer()
{
	delete t1;
}

int Timer::howMuchMorePassed()
{
	if (! print_timing)
		return 0;
	struct timeval t2;
	gettimeofday(&t2, NULL);
	int result = 1000000*(t2.tv_sec - t1->tv_sec) + t2.tv_usec - t1->tv_usec;
	*t1 = t2;
	return result;
}

void Timer::print(const char* prefix, int value)
{
	if (print_timing)
		printf("%s: %d\n", prefix, value);
}
