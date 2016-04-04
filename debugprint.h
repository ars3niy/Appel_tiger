#ifndef _DEBUGPRINT_H
#define _DEBUGPRINT_H

#include <stdio.h>
#include <string>

class DebugPrinter {
private:
	FILE *f;
	std::string name;
public:
	DebugPrinter(const char *filename);
	~DebugPrinter();
	void debug(const char *msg, ...);
};

#endif
