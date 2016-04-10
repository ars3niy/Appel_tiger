#include "layeredmap.h"
#include <map>
#include <list>
#include <assert.h>

class LayeredMapPrivate {
public:
	typedef std::list<std::map<std::string, void*> > MapList;
	MapList maps;
	
	LayeredMapPrivate()
	{
		maps.resize(1);
	}
};

LayeredMap::LayeredMap()
{
	d = new LayeredMapPrivate();
}

LayeredMap::~LayeredMap()
{
	delete d;
}

void LayeredMap::add(const std::string &name, void *value)
{
	d->maps.back().insert(std::make_pair(name, value));
}

void *LayeredMap::lookup(const std::string &name)
{
	for (auto i = d->maps.rbegin(); i != d->maps.rend(); ++i) {
		auto entry = (*i).find(name);
		if (entry != (*i).end())
			return (*entry).second;
	}
	return NULL;
}

void *LayeredMap::lookup_last_layer(const std::string &name)
{
	auto entry = d->maps.back().find(name);
	if (entry != d->maps.back().end())
		return (*entry).second;
	return NULL;
}

void LayeredMap::newLayer()
{
	d->maps.push_back(std::map<std::string, void*>());
}

void LayeredMap::removeLastLayer()
{
	assert(d->maps.size() >= 1);
	d->maps.pop_back();
}
