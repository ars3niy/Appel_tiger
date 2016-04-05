#include <stdint.h>

uint64_t shl(uint64_t x, uint64_t by)
{
	return x << by;
}

uint64_t shr(uint64_t x, uint64_t y)
{
	return x >> y;
}

uint64_t and(uint64_t x, uint64_t y)
{
	return x & y;
}

uint64_t xor(uint64_t x, uint64_t y)
{
	return x ^ y;
}
