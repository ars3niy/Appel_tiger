#ifndef _DEBUGPRINT_H
#define _DEBUGPRINT_H

#include <stdio.h>

class DebugPrinter {
private:
	FILE *f;
public:
	DebugPrinter(const char *filename);
	~DebugPrinter();
	void debug(const char *msg, ...);
};

#endif
