#include <stdint.h>

enum {ssize = 23};

static int64_t __dp[(1 << ssize)*ssize];
static int64_t __from[(1 << ssize)*ssize];

int64_t *get_dp()
{
	return __dp;
}

int64_t *get_from()
{
	return __from;
}
