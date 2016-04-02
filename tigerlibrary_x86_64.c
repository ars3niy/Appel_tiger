#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void *__getmem(int64_t size)
{
	return malloc(size);
}

void *__getmem_fill(int64_t count, int64_t value)
{
	int64_t *result = (int64_t *)malloc(count*value);
	if (count > 0)
		while (count-- != 0)
			*result++ = value;
	return result;
}

void __flush()
{
	fflush(stdout);
}

static void *chr_impl(char c)
{
	char *res = (char *)malloc(9);
	*((int64_t *)res) = 1;
	res[8] = c;
	return res;
}

void *__getchar()
{
	return chr_impl(fgetc(stdin));
}

int64_t __ord(unsigned char *s)
{
	if (*((int64_t *)s) == 0)
		return -1;
	else
		return s[8];
}

void *__chr(int64_t i)
{
	return chr_impl(i);
}

int64_t __size(char *s)
{
	return *((int64_t *)s);
}

void *__substring(char *s, int first, int n)
{
	int64_t len = *((int64_t *)s);
	if (first + n > len)
		n = len - first;
	if (n < 0)
		n = 0;
	char *res = (char *)malloc(n+8);
	*((int64_t *)res) = n;
	memmove(res+8, s+8+first, n);
	return res;
}

void *__concat(char *s1, char *s2)
{
	int64_t len1 = *((int64_t *)s1);
	int64_t len2 = *((int64_t *)s2);
	char *res = (char *)malloc(8+len1+len2);
	*((int64_t *)res) = len1+len2;
	memmove(res+8, s1+8, len1);
	memmove(res+8+len1, s2+8, len2);
}

int64_t __not(int64_t i)
{
	return !i;
}
