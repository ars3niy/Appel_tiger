#ifndef _TIMER_H
#define _TIMER_H

extern bool print_timing;

struct timeval;

class Timer {
private:
	timeval *t1;
public:
	Timer();
	~Timer();
	int howMuchMorePassed();
	void print(const char *prefix) {print(prefix, howMuchMorePassed());}
	void print(const char *prefix, int value);
};

#endif

