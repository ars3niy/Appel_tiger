#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void *getmem(int64_t size)
{
	return malloc(size);
}

void *getmem_fill(int64_t count, int64_t value)
{
	int64_t *result = (int64_t *)malloc(count*value);
	if (count > 0)
		while (count-- != 0)
			*result++ = value;
	return result;
}
