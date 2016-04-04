#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

enum {INT_SIZE = 8};

void *__getmem(int64_t size)
{
	return malloc(size);
}

void *__getmem_fill(int64_t count, int64_t value)
{
	int64_t *result = (int64_t *)malloc(count*INT_SIZE);
	int64_t *dest = result;
	if (count > 0)
		while (count-- != 0)
			*dest++ = value;
	return result;
}

void __print(char *s)
{
	int i;
	uint64_t len = *((uint64_t *)s);
	s += INT_SIZE;
	while (len-- != 0)
		fputc(*s++, stdout);
}

void __flush()
{
	fflush(stdout);
}

static void *chr_impl(char c)
{
	char *res = (char *)malloc(INT_SIZE+1);
	*((int64_t *)res) = 1;
	res[INT_SIZE] = c;
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
		return s[INT_SIZE];
}

void *__chr(int64_t i)
{
	return chr_impl(i);
}

int64_t __size(char *s)
{
	return *((int64_t *)s);
}

void *__substring(char *s, int64_t first, int64_t n)
{
	int64_t len = *((int64_t *)s);
	if (first + n > len)
		n = len - first;
	if (n < 0)
		n = 0;
	char *res = (char *)malloc(n+INT_SIZE);
	*((int64_t *)res) = n;
	memmove(res+INT_SIZE, s+INT_SIZE+first, n);
	return res;
}

void *__concat(char *s1, char *s2)
{
	int64_t len1 = *((int64_t *)s1);
	int64_t len2 = *((int64_t *)s2);
	char *res = (char *)malloc(INT_SIZE+len1+len2);
	*((int64_t *)res) = len1+len2;
	memmove(res+INT_SIZE, s1+INT_SIZE, len1);
	memmove(res+INT_SIZE+len1, s2+INT_SIZE, len2);
	return res;
}

int64_t __strcmp(char *s1, char *s2)
{
	int64_t len1 = *((int64_t *)s1);
	int64_t len2 = *((int64_t *)s2);
	int64_t minlen = len1;
	if (len2 < minlen)
		minlen = len2;
	int ret = strncmp(s1+INT_SIZE, s2+INT_SIZE, minlen);
	if (ret != 0)
		return ret;
	else if (len1 == len2)
		return 0;
	else if (len1 < len2)
		return -1;
	else
		return 1;
}

int64_t __not(int64_t i)
{
	return !i;
}
