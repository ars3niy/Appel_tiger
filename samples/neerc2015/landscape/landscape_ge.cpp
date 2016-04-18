#pragma comment(linker, "/STACK:1000000000")
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cassert>
#include <ctime>
#include <cstring>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <list>
#include <deque>
#include <queue>
#include <sstream>
#include <iostream>
#include <algorithm>

using namespace std;

#define pb push_back
#define mp make_pair
#define fs first
#define sc second
#define sinf sdkfjsd 
#define left sdfdsfaaa
#define right sdfdssdfa

typedef long long Int;

const Int size = 200 * 1000 + 100;

#include "minmax.h"


struct Global {
	long long inf;
	long long sinf;

	Int n;
	long long have;
	long long h[size];
	long long event[size];
	long long psum[size];
	long long events[size];
	long long left[size];
	long long right[size];
};

long long getsum(Global &g, long long lb, long long rb) {
	lb = max(lb, 0ll);
	rb = min(rb, g.n - 1ll);
	if (lb > rb)
		return 0ll;
	return g.psum[rb + 1] - g.psum[lb];
}

void calcleft(Global &g, long long th) {
	for (Int i = 0; i < g.n; i++)
		g.events[i] = i - th;
	long long cur = -th;
	for (Int i = 0; i < g.n; i++) {
		long long sfrom = max(i + 0ll, i + (th - g.h[i]));
		if (sfrom < g.n)
			g.events[(Int) sfrom] = max(g.events[(Int) sfrom], i + 0ll);

		cur = max(cur, g.events[i]);
		g.left[i] = cur;
	}
}

void calcright(Global &g, long long th) {
	for (Int i = 0; i < g.n; i++)
		g.events[i] = i + th;
	long long cur = g.n + th;
	for (Int i = g.n - 1; i >= 0; i--) {
		long long sfrom = min(i + 0ll, i - (th - g.h[i]));
		if (sfrom >= 0)
			g.events[(Int) sfrom] = min(g.events[(Int) sfrom], i + 0ll);

		cur = min(cur, g.events[i]);
		g.right[i] = cur;
	}
}

long long getans(Global &g, long long mid) {
 	printf("%d\n", mid);
	calcleft(g, mid);
	calcright(g, mid);

	long long ans = g.inf;

	for (Int i = 0; i < g.n; i++) {
 		printf("loop\n");
 		printf("left[i] = %d\n", g.left[i]);
 		printf("right[i] = %d\n", g.right[i]);
		if (g.left[i] < -1 || g.right[i] > g.n) {
 			printf("continue\n");
			continue;
		}
		long long cur = 0;
		if (g.left[i] < i - 1)
			cur += (mid - 1 + mid - (i - g.left[i]) + 1) * (i - g.left[i] - 1)  / 2;
		if (g.right[i] > i + 1)
			cur += (mid - 1 + mid - (g.right[i] - i) + 1) * (g.right[i] - i - 1) / 2;
		if (g.left[i] < i && g.right[i] > i)
			cur += mid;

		cur -= getsum(g, g.left[i] + 1, g.right[i] - 1);

		//cerr << mid << ' ' << i << ' ' << left[i] << ' ' << right[i] << ' ' << cur << endl;

 		printf("ans = %ld, cur = %ld\n", ans, cur);
		ans = min(ans, cur);
	}
 	printf("getans = %ld\n", ans);

	return ans;
}

int main() {
	Global *p_glob = (Global *)malloc(sizeof(Global));
	Global &g = *p_glob;
	g.inf = (long long) 2e18;
	g.sinf = 1000ll * 1000 * 1000;
/*
    freopen("landscape.in", "w", stdout);
    Int n = 100 * 1000;
    Int hv = 1000 * 1000 * 1000;
    cout << n << ' ' << hv * 1ll * hv << endl;
    for (Int i = 0; i < n; i++)
    	cout << hv << endl;
    return 0;
 */   
//     freopen("landscape.in", "r", stdin);
//     freopen("landscape.out", "w", stdout);

    cin >> g.n >> g.have;
//    scanf("%d%lld", &n, &have);
    for (Int i = 0; i < g.n; i++)
   		cin >> g.h[i];
   // 	scanf("%lld", &h[i]);
   for (int repeat = 0; repeat < 10; repeat++) {
    g.psum[0] = 0ll;
    for (Int i = 0; i < g.n; i++)
    	g.psum[i + 1] = g.psum[i] + g.h[i];

    long long lb = 0;
    long long rb = 2 * g.sinf;

    //getans(have);
    //return 0;

    while (lb < rb) {
    	long long mid = (lb + rb + 1) / 2;

    	if (getans(g, mid) <= g.have)
    		lb = mid;
    	else
    		rb = mid - 1;
    }

    cout << lb << endl;
    }

    return 0;
}
