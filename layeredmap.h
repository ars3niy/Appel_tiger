#ifndef _LAYEREDMAP_H
#define _LAYEREDMAP_H

#include <string>

class LayeredMapPrivate;

class LayeredMap {
private:
	LayeredMapPrivate *d;
public:
	LayeredMap();
	~LayeredMap();
	
	void add(const std::string &name, void *value);
	void *lookup(const std::string &name);
	void *lookup_last_layer(const std::string &name);
	void newLayer();
	void removeLastLayer();
};

#endif
