#include "errormsg.h"
#include <stdio.h>
#include <stdlib.h>

namespace Error {

static int linenumber = 1;

int getLineNumber()
{
	return linenumber;
}

void newline()
{
    linenumber++;
}

void error(const std::string &message)
{
    printf("Error, line %d: %s\n", linenumber, message.c_str());
	exit(1);
}

}
