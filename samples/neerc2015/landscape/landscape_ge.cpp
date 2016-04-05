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

const double pi = acos(-1.0);

const long long inf = (long long) 2e18;
const long long sinf = 1000ll * 1000 * 1000;
const int size = 200 * 1000 + 100;

int n;
long long have;
long long h[size];
long long event[size];
long long psum[size];
long long events[size];
long long left[size];
long long right[size];

long long getsum(long long lb, long long rb) {
	lb = max(lb, 0ll);
	rb = min(rb, n - 1ll);
	if (lb > rb)
		return 0ll;
	return psum[rb + 1] - psum[lb];
}

void calcleft(long long th) {
	for (int i = 0; i < n; i++)
		events[i] = i - th;
	long long cur = -th;
	for (int i = 0; i < n; i++) {
		long long sfrom = max(i + 0ll, i + (th - h[i]));
		if (sfrom < n)
			events[(int) sfrom] = max(events[(int) sfrom], i + 0ll);

		cur = max(cur, events[i]);
		left[i] = cur;
	}
}

void calcright(long long th) {
	for (int i = 0; i < n; i++)
		events[i] = i + th;
	long long cur = n + th;
	for (int i = n - 1; i >= 0; i--) {
		long long sfrom = min(i + 0ll, i - (th - h[i]));
		if (sfrom >= 0)
			events[(int) sfrom] = min(events[(int) sfrom], i + 0ll);

		cur = min(cur, events[i]);
		right[i] = cur;
	}
}

long long getans(long long mid) {
	printf("%d\n", mid);
	calcleft(mid);
	calcright(mid);

	long long ans = inf;

	for (int i = 0; i < n; i++) {
		printf("loop\n");
		printf("left[i] = %d\n", left[i]);
		printf("right[i] = %d\n", right[i]);
		if (left[i] < -1 || right[i] > n) {
			printf("continue\n");
			continue;
		}
		long long cur = 0;
		if (left[i] < i - 1)
			cur += (mid - 1 + mid - (i - left[i]) + 1) * (i - left[i] - 1)  / 2;
		if (right[i] > i + 1)
			cur += (mid - 1 + mid - (right[i] - i) + 1) * (right[i] - i - 1) / 2;
		if (left[i] < i && right[i] > i)
			cur += mid;

		cur -= getsum(left[i] + 1, right[i] - 1);

		//cerr << mid << ' ' << i << ' ' << left[i] << ' ' << right[i] << ' ' << cur << endl;

		printf("ans = %ld, cur = %ld\n", ans, cur);
		ans = min(ans, cur);
	}
	printf("getans = %ld\n", ans);

	return ans;
}

int main() {
/*
    freopen("landscape.in", "w", stdout);
    int n = 100 * 1000;
    int hv = 1000 * 1000 * 1000;
    cout << n << ' ' << hv * 1ll * hv << endl;
    for (int i = 0; i < n; i++)
    	cout << hv << endl;
    return 0;
 */   
    freopen("landscape.in", "r", stdin);
    freopen("landscape.out", "w", stdout);

    cin >> n >> have;
//    scanf("%d%lld", &n, &have);
    for (int i = 0; i < n; i++)
   		cin >> h[i];
   // 	scanf("%lld", &h[i]);
    psum[0] = 0ll;
    for (int i = 0; i < n; i++)
    	psum[i + 1] = psum[i] + h[i];

    long long lb = 0;
    long long rb = 2 * sinf;

    //getans(have);
    //return 0;

    while (lb < rb) {
    	long long mid = (lb + rb + 1) / 2;

    	if (getans(mid) <= have)
    		lb = mid;
    	else
    		rb = mid - 1;
    }

    cout << lb << endl;

    return 0;
}