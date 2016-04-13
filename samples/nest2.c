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
	void g2(int F, int G2);
	void g3(int F, int G2);
	void g(int F, int G);
	void h1(int F, int G, int H1);
	void h(int F, int G, int H);
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
			printint("G1 = ", G1, "");
		}
		void g2(int F, int G2) {
			print("=== g2\n");
			printint("G2 = ", G2, "");
			g3(F, G2);
		}
		void g3(int F, int G3) {
			print("=== g3\n");
			printint("G3 = ", G3, "");
			g(F, G3);
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
				printint("H1 = ", H1, "");
			}
			void h(int F, int G, int H) {
				print("=== h\n");
				printint("H = ", H, "");
				printint("h[H=", H, "] -> h1");
				h1(F, G, H+1);
				if (H < 6) {
					printint("h[H=", H, "] -> p");
					p(F, G, H, H+1);
				}
			}
				void p(int F, int G, int H, int P) {
					print("=== p\n");
					printint("P = ", P, "");
					if (P < 6) {
						printint("p[P=", P, "] -> g");
						g(F, P+1);
						printint("p[P=", P, "] -> g1");
						g1(F, P+1);
						printint("p[P=", P, "] -> g2");
						g2(F, P+1);
					}
				}

int main ()
{
	f(0);
}
