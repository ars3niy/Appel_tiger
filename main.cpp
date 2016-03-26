#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <parser.hpp>

extern "C" {
extern FILE *yyin;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: compiler <input-file>\n");
		return 1;
	}
	yyin = fopen(argv[1], "r");
	if (yyin == NULL) {
		printf("Cannot open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}
	
	yyparse();
	
	fclose(yyin);
	return 0;
}
