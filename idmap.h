#ifndef _IDMAP_H
#define _IDMAP_H

#include <vector>
#include <string.h>

typedef int ObjectId;

enum {INVALID_OBJECT_ID = -1};

class IdTracker {
private:
	int last_id;
public:
	IdTracker() : last_id(0) {}
	
	ObjectId getId() {return last_id++;}
};

class IdMap {
private:
	std::vector<void *>values;
public:
	void *lookup(ObjectId id);
	void add(ObjectId id, void *value);
	
	IdMap()
	{
		values.resize(10, NULL);
	}
};

#endif
