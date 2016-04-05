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

const double pi = acos(-1.0);
const int size = 200 * 1000 + 100;
const int ssize = 23;
const int inf = 1000 * 1000 * 1000;

int indeg[size];
int outdeg[size];

vector <int> way[ssize][ssize];
int len[ssize][ssize];
vector <int> vertex[size];
int n, m;
bool intr[size];
int myinum[size];
int icnt = 0;
int dp[1 << ssize][ssize];
int from[1 << ssize][ssize];

vector <int> curpath;
bool used[size];

void dfs(int vs, int v) {
	//cerr << vs << ' ' << v << endl;
	if (used[v])
		return;

	used[v] = true;
	curpath.pb(v);
	if (intr[v]) {
	//	cerr << vs << ' ' << v << ' ' << curpath.size() << ' ' << way[myinum[vs]][myinum[v]].size() << endl;
		//if (way[myinum[vs]][myinum[v]].size() > 1u && curpath.size() > 1u) {
		// 	printf("There is no route, Karl!\n");
   	//		exit(0);
   			//return 0;
   		
	//	}
		if (way[myinum[vs]][myinum[v]].size() < curpath.size())
			way[myinum[vs]][myinum[v]] = curpath;

		used[v] = false;
		curpath.pop_back();

		return;
	}

	for (int i = 0; i < (int) vertex[v].size(); i++)
		dfs(vs, vertex[v][i]);

	used[v] = false;
	curpath.pop_back();
}

int main() {

    scanf("%d%d", &n, &m);
    for (int i = 0; i < m; i++) {
    	int v, u;
    	scanf("%d%d", &v, &u);
    	v--, u--;
//    	assert(v != u);

    	if (v != u) {
    		vertex[v].pb(u);
    		indeg[u]++;
    		outdeg[v]++;
    	}
    }

    for (int i = 0; i < n; i++)
    	if (outdeg[i] > 1) {
    		intr[i] = true;
    		myinum[i] = icnt;
//    		inum[icnt] = i;
    		icnt++;
    	}

    for (int i = 0; i < n; i++)
    	if (icnt < 2 && !intr[i]) {
    		intr[i] = true;
    		myinum[i] = icnt;
  //  		inum[icnt] = i;
    		icnt++;
    	}

    for (int i = 0; i < n; i++)
    	if (indeg[i] == 0 || outdeg[i] == 0) {
    	 	printf("There is no route, Karl!\n");
	   		return 0;
    	}

    if (icnt > 20) {
    	 	printf("There is no route, Karl!\n");
   		return 0;
    }

    //for (int i = 0; i < n; i++)
    //	cerr << "i " << i << ' ' << intr[i] << endl;

    for (int i = 0; i < n; i++)
    	if (intr[i]) {
    		for (int j = 0; j < (int) vertex[i].size(); j++)
    			dfs(i, vertex[i][j]);
    	}

    int h = icnt;
    for (int i = 0; i < h; i++)
    	for (int j = 0; j < h; j++) {
    	    if (way[i][j].empty())
    	    	len[i][j] = -1;
    	    else
    			len[i][j] = (int) way[i][j].size();
    	}	

    //cerr << "h = " << h << endl;

    //for (int i = 0; i < h; i++)
    //	for (int j = 0; j < h; j++) {
    //		cerr << i << ' ' << j << ' ' << len[i][j] << endl;
    //	}

    for (int i = 0; i < (1 << h); i++)
    	for (int j = 0; j < h; j++)
    		dp[i][j] = -inf;

    dp[1][0] = 0;
    for (int i = 2; i < (1 << h); i++) {
    	for (int j = 0; j < h; j++) {
    		dp[i][j] = -inf;
    		if ((i & 1) == 0 || (((i >> j) & 1) == 0)) {
    			continue;
    		}	

    		for (int k = 0; k < h; k++) {
    			if (k == j || (((i >> k) & 1) == 0) || len[k][j] == -1) {
    				continue;
    			}

    			if (dp[i ^ (1 << j)][k] + len[k][j] > dp[i][j]) {
    				dp[i][j] = dp[i ^ (1 << j)][k] + len[k][j];
    				from[i][j] = k;
    			}
    		}
    	}
    }

    //cerr << "here" << endl;

    int best = -inf;
    int ba = -1;
    for (int i = 1; i < h; i++)
    	if (len[i][0] != -1 && dp[(1 << h) - 1][i] + len[i][0] > best) {
    		best = dp[(1 << h) - 1][i] + len[i][0];
    		ba = i;
    	}

    //cerr << best << ' ' << ba << endl;

    //cerr << "look " << from[3][1] << endl;

    if (best < n) {
    	printf("There is no route, Karl!\n");
    } else {
    	vector <int> vans;
    	int cur = (1 << h) - 1;
    	while (cur != 0) {
  //  		cerr << cur << ' ' << ba << endl;
    		vans.pb(ba);
    		int nba = from[cur][ba];
    		cur ^= (1 << ba);

    		ba = nba;	
    	}

//    	if (!((int) vans.size() == h)) {
  //  	 	printf("There is no route, Karl!\n");
   	//		return 0;
   	//	}	
    	assert((int) vans.size() == h);

    	reverse(vans.begin(), vans.end());
    	vans.pb(0);

    	vector <int> rans;
    	for (int i = 0; i < h; i++) {
    		for (int j = 0; j < (int) way[vans[i]][vans[i + 1]].size(); j++)
    			rans.pb(way[vans[i]][vans[i + 1]][j]);
//    		rans.insert(rans.end(), way[rans[i]][rans[i + 1]].begin(), way[rans[i]][rans[i + 1]].end());
    	}
    	
//    	if (!((int) rans.size() == n)) {
//    	 	printf("There is no route, Karl!\n");
//   			return 0;
//   		}	
    	
    	assert((int) rans.size() == n);

    	for (int i = 0; i < (int) rans.size(); i++) {
    		if (rans[i] == 0) {
    			for (int j = i; j < (int) rans.size(); j++)
    				printf("%d ", rans[j] + 1);
    			for (int j = 0; j <= i; j++)
    				printf("%d ", rans[j] + 1);
    		
    			break;
    		}
    	}


    }

    return 0;
}