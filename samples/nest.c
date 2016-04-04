#include <stdio.h>
#include <stdint.h>
#include <assert.h>

int g(int f1, int f2, int F1, int F2, int g1, int g2);

int f(int f1, int f2, int f3)
{
	int F1 = f1+f2; printf("F1=%d\n", F1);
	int F2 = f1-f2; printf("F2=%d\n", F2);
	int F3 = f3 + g(f1, f2, F1, F2, F1, F2); printf("F3=%d\n", F3);
	return F1+F2+F3;
}

int h(int f1, int F1, int g1, int G1, int h1, int h2);

int g(int f1, int f2, int F1, int F2, int g1, int g2)
{
	printf("g1=%d\n", g1);
	printf("g2=%d\n", g2);
	int G1 = F2+f2+g1; printf("G1=%d\n", G1);
	int G2 = g2; printf("G2=%d\n", G2);
	int G3 = h(f1, F1, g1, G1, G1, G2); printf("G3=%d\n", G3);
	return G1+G2+G3;
}

int h(int f1, int F1, int g1, int G1, int h1, int h2)
{
	int H1 = f1 + F1 + g1 + G1 + h1; printf("H1=%d\n", H1);
	int H2 = h2; printf("H2=%d\n", H2);
	return H1+H2;
}

int main ()
{
	printf("%c\n", '0' + f(1,2,3));
}
