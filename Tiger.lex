%{
#include <syntaxtree.h>
#include <errormsg.h>

#include <stdlib.h>
#include <string>

#include <parser.hpp>

int comment_nesting;
std::string svalue;
extern YYSTYPE yylval;

%}

/*
\n newline
\t tab
\^c fuck it
\ddd ASCII character
\" quote
\\ backslash
\<whatever space>\ ignore
*/

%x COMMENT STRING STRINGIGNORE

%%
" " 	{}
\n  	{Error::newline();}
\t  	{}
\r  	{}
"," 	{return SYM_COMMA;}
":" 	{return SYM_COLON;}
"{" 	{return SYM_OPENBRACE;}
"}" 	{return SYM_CLOSEBRACE;}
"=" 	{return SYM_EQUAL;}
":="	{return SYM_ASSIGN;}
"." 	{return SYM_DOT;}
"(" 	{return SYM_OPENPAREN;}
")" 	{return SYM_CLOSEPAREN;}
"[" 	{return SYM_OPENBRACKET;}
"]" 	{return SYM_CLOSEBRACKET;}
"+" 	{return SYM_PLUS;}
"-" 	{return SYM_MINUS;}
"*" 	{return SYM_ASTERISK;}
"/" 	{return SYM_SLASH;}
";" 	{return SYM_SEMICOLON;}
"<" 	{return SYM_LESS;}
">" 	{return SYM_GREATER;}
"<="	{return SYM_LESSEQUAL;}
">="	{return SYM_GREATEQUAL;}
"<>"	{return SYM_NONEQUAL;}
"&" 	{return SYM_AND;}
"|" 	{return SYM_OR;}

type 	{return SYM_TYPE;}
array	{return SYM_ARRAY;}
of   	{return SYM_OF;}
var  	{return SYM_VAR;}
nil  	{return SYM_NIL;}
function	{return SYM_FUNCTION;}
let  	{return SYM_LET;}
in   	{return SYM_IN;}
end  	{return SYM_END;}
if   	{return SYM_IF;}
then 	{return SYM_THEN;}
else 	{return SYM_ELSE;}
while	{return SYM_WHILE;}
do   	{return SYM_DO;}
for  	{return SYM_FOR;}
to   	{return SYM_TO;}
break	{return SYM_BREAK;}

\"          	{svalue = ""; BEGIN(STRING);}
<STRING>\"  	{yylval = new Syntax::StringValue(svalue); BEGIN(INITIAL); return SYM_STRING;}
<STRING>\\n 	{svalue += "\n";}
<STRING>\\r 	{svalue += "\r";}
<STRING>\\t 	{svalue += "\t";}
<STRING>\\[0-9]{3}	{svalue.append(1, (char)atoi(yytext+1));}
<STRING>\\\"	{svalue += "\"";}
<STRING>\\\\	{svalue += "\\";}
<STRING>\\  	{BEGIN(STRINGIGNORE);}
<STRING>.   	{svalue += yytext;}
<STRING>\n  	{Error::error("Unescaped end-of-line inside string constant");}
<STRING><<EOF>>	{Error::error("Unterminated string constant");}
<STRINGIGNORE>[ \t\r]	{}
<STRINGIGNORE>\n     	{Error::newline();}
<STRINGIGNORE>\\       	{BEGIN(STRING);}
<STRINGIGNORE><<EOF>>	{Error::error("Unterminated string constant");}
<STRINGIGNORE>.      	{Error::error("Backslash followed by other than valid escape sequence or white space and a backslash");}

"/*"	{comment_nesting = 1; BEGIN(COMMENT);}
<COMMENT>"/*"	{comment_nesting++;}
<COMMENT>"*/"	{comment_nesting--; if (comment_nesting == 0) {BEGIN(INITIAL);}}
<COMMENT>\n  	{Error::newline();}
<COMMENT>.   	{}

[a-zA-Z][a-zA-Z0-9_]*	{yylval = new Syntax::Identifier(yytext); return SYM_ID;}
[0-9]+	{yylval = new Syntax::IntValue(atoi(yytext)); return SYM_INT;}
.	{Error::error(std::string("Illegal character: ") + yytext);}
