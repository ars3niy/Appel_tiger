#include "errormsg.h"
#include <stdio.h>
#include <stdlib.h>

namespace Error {

static int linenumber = 1;
static int errorCount = 0;

int getLineNumber()
{
	return linenumber;
}

int getErrorCount()
{
	return errorCount;
}

void newline()
{
    linenumber++;
}

void error(const std::string &message, int _linenumber)
{
    printf("Error, line %d: %s\n",
		   _linenumber > 0 ? _linenumber : Error::linenumber,
		   message.c_str());
	errorCount++;
}

void warning(const std::string &message, int _linenumber)
{
    printf("Warning, line %d: %s\n",
		   _linenumber > 0 ? _linenumber : Error::linenumber,
		   message.c_str());
}

void fatalError(const std::string &message, int _linenumber)
{
    printf("Fatal error, line %d: %s\n",
		   _linenumber > 0 ? _linenumber : Error::linenumber,
		   message.c_str());
	exit(127);
}

}
