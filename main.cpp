#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "tokens.h"

extern "C" {
extern FILE *yyin;
}

extern int ivalue;
extern std::string svalue;
extern int yylex();

const char *TOKENS[] = {
	"SYM_NONE",
	"SYM_COMMA",
	"SYM_COLON",
	"SYM_EQUAL",
	"SYM_ASSIGN",
	"SYM_OPENPAREN",
	"SYM_CLOSEPAREN",
	"SYM_DOT",
	"SYM_OPENBRACKET",
	"SYM_CLOSEBRACKET",
	"SYM_OPENBRACE",
	"SYM_CLOSEBRACE",
	"SYM_PLUS",
	"SYM_MINUS",
	"SYM_ASTERISK",
	"SYM_SLASH",
	"SYM_SEMICOLON",
	"SYM_LESS",
	"SYM_GREATER",
	"SYM_LESSEQUAL",
	"SYM_GREATEQUAL",
	"SYM_NONEQUAL",
	"SYM_AND",
	"SYM_OR",
	
	"SYM_TYPE",
	"SYM_ARRAY",
	"SYM_OF",
	"SYM_VAR",
	"SYM_NIL",
	"SYM_FUNCTION",
	"SYM_LET",
	"SYM_IN",
	"SYM_END",
	"SYM_IF",
	"SYM_THEN",
	"SYM_ELSE",
	"SYM_WHILE",
	"SYM_DO",
	"SYM_FOR",
	"SYM_TO",
	"SYM_BREAK",
	
	"SYM_STRING",
	"SYM_INT",
	"SYM_ID"
};

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
	while (1) {
		int token = yylex();
		if (token == 0)
			break;
		printf("%s", TOKENS[token]);
		if (token == SYM_INT)
			printf(" %d", ivalue);
		else if (token == SYM_STRING)
			printf(" \"%s\"", svalue.c_str());
		else if (token == SYM_ID)
			printf(" %s", svalue.c_str());
		printf("\n");
	}
	fclose(yyin);
}
