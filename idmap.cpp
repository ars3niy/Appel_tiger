#include "idmap.h"
#include <string.h>

void* IdMap::lookup(ObjectId id)
{
	if ((ObjectId)values.size() <= id)
		return NULL;
	else
		return values[id];
}

void IdMap::add(ObjectId id, void* value)
{
	if ((ObjectId)values.size() <= id) {
		int newsize = 2*values.size();
		if (newsize <= id)
			newsize = id;
		values.resize(newsize, NULL);
	}
	values[id] = value;
}
