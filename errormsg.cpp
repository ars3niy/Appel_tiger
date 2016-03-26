#include "errormsg.h"
#include <stdio.h>
#include <stdlib.h>

static int linenumber = 1;

void Err_newline()
{
    linenumber++;
}

void Err_error(const std::string &message)
{
    printf("Error, line %d: %s\n", linenumber, message.c_str());
	exit(1);
}
