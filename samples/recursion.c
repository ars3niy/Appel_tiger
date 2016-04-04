#include <stdio.h>

void print (const char *s)
{
	fputs(s, stdout);
}

	void printint(const char *prefix, int i, const char *suffix) {
		print(prefix);
		printf("%c", '0'+i);
		print(suffix);
		print("\n");
	}

	void g1(int F, int G1);
	void g(int F, int G);
	void h1(int F, int G, int H1);
	void h(int F, int G, int H);
	void p1(int F, int G, int H, int P1);
	void p(int F, int G, int H, int P);
	
	void f(int F) {
		print("=== f\n");
		printint("F = ", F, "");
		if (F < 6) {
			printint("f[F=", F, "] -> g");
			g(F, F+1);
		}
	}
		void g1(int F, int G1) {
			print("=== g1\n");
			printint("F = ", F, "");
			printint("G1 = ", G1, "");
		}
		void g(int F, int G) {
			print("=== g\n");
			printint("F = ", F, "");
			printint("G = ", G, "");
			if (G < 6) {
				printint("g[G=", G, "] -> h");
				h(F, G, G+1);
			}
		}
			void h1(int F, int G, int H1) {
				print("=== h1\n");
				printint("F = ", F, "");
				printint("G = ", G, "");
				printint("H1 = ", H1, "");
			}
			void h(int F, int G, int H) {
				print("=== h\n");
				printint("F = ", F, "");
				printint("G = ", G, "");
				printint("H = ", H, "");
				printint("h[H=", H, "] -> h1");
				h1(F, G, H+1);
				if (H < 6) {
					printint("h[H=", H, "] -> p");
					p(F, G, H, H+1);
				}
			}
				void p1(int F, int G, int H, int P1) {
					print("=== p1\n");
					printint("F = ", F, "");
					printint("G = ", G, "");
					printint("H = ", H, "");
					printint("P1 = ", P1, "");
					if (P1 < 6) {
						printint("p1[P1=", P1, "] -> p");
						p(F, G, H, P1+1);
					}
				}
				void p(int F, int G, int H, int P) {
					print("=== p\n");
					printint("F = ", F, "");
					printint("G = ", G, "");
					printint("H = ", H, "");
					printint("P = ", P, "");
					if (P < 6) {
						printint("p[P=", P, "] -> p1");
						p1(F, G, H, P+1);
						printint("p[P=", P, "] -> g");
						g(F, P+1);
					}
				}

int main ()
{
	f(0);
}
