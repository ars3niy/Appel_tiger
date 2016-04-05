#include "errormsg.h"
#include <stdio.h>
#include <stdlib.h>

namespace Error {

static int linenumber = 1;
static int errorCount = 0;

std::string filename;

void setFileName(const std::string& _filename)
{
	filename = _filename;
}

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

void global_error(const std::string &message)
{
	fputs((message + "\n").c_str(), stdout);
	errorCount++;
}

void error(const std::string &message, int _linenumber)
{
    printf("%s Error, line %d: %s\n",
		   filename.c_str(),
		   _linenumber > 0 ? _linenumber : Error::linenumber,
		   message.c_str());
	errorCount++;
}

void warning(const std::string &message, int _linenumber)
{
    printf("%s Warning, line %d: %s\n",
		   filename.c_str(),
		   _linenumber > 0 ? _linenumber : Error::linenumber,
		   message.c_str());
}

void fatalError(const std::string &message, int _linenumber)
{
    printf("%s Internal error, line %d: %s\n",
		   filename.c_str(),
		   _linenumber > 0 ? _linenumber : Error::linenumber,
		   message.c_str());
	exit(127);
}

}
