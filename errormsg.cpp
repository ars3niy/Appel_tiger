#include "errormsg.h"
#include <stdio.h>
#include <stdlib.h>

namespace Error {

InputPos DEFAULT_POS = {.linenumber = -1};

static InputPos position = {.linenumber = 1};
static int errorCount = 0;

std::string filename;

void setFileName(const std::string& _filename)
{
	filename = _filename;
}

const InputPos &getPosition()
{
	return position;
}

int getErrorCount()
{
	return errorCount;
}

void newline()
{
    position.linenumber++;
}

void global_error(const std::string &message)
{
	fputs((message + "\n").c_str(), stdout);
	errorCount++;
}

void error(const std::string &message, const InputPos &position)
{
    printf("%s Error, line %d: %s\n",
		   filename.c_str(),
		   position.linenumber > 0 ? position.linenumber : Error::position.linenumber,
		   message.c_str());
	errorCount++;
}

void warning(const std::string &message, const InputPos &position)
{
    printf("%s Warning, line %d: %s\n",
		   filename.c_str(),
		   position.linenumber > 0 ? position.linenumber : Error::position.linenumber,
		   message.c_str());
}

void fatalError(const std::string &message, const InputPos &position)
{
    printf("%s Internal error, line %d: %s\n",
		   filename.c_str(),
		   position.linenumber > 0 ? position.linenumber : Error::position.linenumber,
		   message.c_str());
	exit(127);
}

}
