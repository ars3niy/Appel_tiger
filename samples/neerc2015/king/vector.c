#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

typedef int64_t int_t;
enum {MARKER = 0x13371337};

int_t *empty()
{
	enum {STARTLEN = 10};
	int_t *a = (int_t *)malloc((3 + STARTLEN)*sizeof(int_t)) + 3;
	a[-3] = STARTLEN;
	a[-2] = 0;
	a[-1] = MARKER;
	return a;
}

int_t *push_back(int_t *a, int_t v)
{
	assert(a[-1] == MARKER);
	int_t len = a[-2];
	int_t allocated = a[-3];
	
	int reallocate = 0;
	while (allocated < len+1) {
		reallocate = 1;
		allocated *= 2;
	}
	
	if (reallocate) {
		a = (int_t *)realloc(a-3, (3 + allocated)*sizeof(int_t)) + 3;
		a[-3] = allocated;
	}
	
	a[len] = v;
	a[-2] = len+1;
	return a;
}

int_t *pop_back(int_t *a)
{
	assert(a[-1] == MARKER);
	int_t len = a[-2];
	if (len == 0)
		return;
	
	a[-2] = len-1;
	return a;
}

void reverse(int_t *a)
{
	assert(a[-1] == MARKER);
	int_t *end = a + a[-2]-1;
	while (end > a) {
		int_t tmp = *a;
		*a = *end;
		*end = tmp;
		a++;
		end--;
	}
}

int_t get_size(int_t *a)
{
	assert(a[-1] == MARKER);
	return a[-2];
}

int_t *vcopy(int_t *a)
{
	assert(a[-1] == MARKER);
	int_t allocated = a[-3];
	int_t *b = (int_t *)malloc((3 + allocated)*sizeof(int_t)) + 3;
	memmove(b-3, a-3, (3 + a[-2]) * sizeof(int_t));
	return b;
}
