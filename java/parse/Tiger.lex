package parse;

%% 

%public
%type int
%implements Parser.yyInput

%{
private error.Error errors = null;

public Yylex(java.io.Reader in, error.Error err) {
	this(in);
	errors = err;
}

private void error(String message) {
	errors.error(message, errors.currentPosition());
}

private void a() {
	a(yytext());
}

private void a(String s) {
	errors.add(s);
}

public Node yylval;
public int token;

public boolean advance() throws java.io.IOException {
	yylval = null;
	token = yylex();
	return token != YYEOF;
}

public int token() {return token;}

public Node value() {return yylval;}

private String svalue = "";

private int comment_nesting = 0;

%}

%state COMMENT
%state STRING
%state STRINGIGNORE

%eofval{
	{errors.eof(); return YYEOF;}
%eofval}       


%%

<YYINITIAL> {

" " 	{a();}
"\r\n"	{errors.newline();}
\n  	{errors.newline();}
\t  	{a("    ");}
\r  	{a(" ");}
"," 	{a(); return Parser.SYM_COMMA;}
":" 	{a(); return Parser.SYM_COLON;}
"{" 	{a(); return Parser.SYM_OPENBRACE;}
"}" 	{a(); return Parser.SYM_CLOSEBRACE;}
"=" 	{a(); return Parser.SYM_EQUAL;}
":="	{a(); return Parser.SYM_ASSIGN;}
"." 	{a(); return Parser.SYM_DOT;}
"(" 	{a(); return Parser.SYM_OPENPAREN;}
")" 	{a(); return Parser.SYM_CLOSEPAREN;}
"[" 	{a(); return Parser.SYM_OPENBRACKET;}
"]" 	{a(); return Parser.SYM_CLOSEBRACKET;}
"+" 	{a(); return Parser.SYM_PLUS;}
"-" 	{a(); return Parser.SYM_MINUS;}
"*" 	{a(); return Parser.SYM_ASTERISK;}
"/" 	{a(); return Parser.SYM_SLASH;}
";" 	{a(); return Parser.SYM_SEMICOLON;}
"<" 	{a(); return Parser.SYM_LESS;}
">" 	{a(); return Parser.SYM_GREATER;}
"<="	{a(); return Parser.SYM_LESSEQUAL;}
">="	{a(); return Parser.SYM_GREATEQUAL;}
"<>"	{a(); return Parser.SYM_NONEQUAL;}
"&" 	{a(); return Parser.SYM_AND;}
"|" 	{a(); return Parser.SYM_OR;}

"type" 	{a(); return Parser.SYM_TYPE;}
"array"	{a(); return Parser.SYM_ARRAY;}
"of"   	{a(); return Parser.SYM_OF;}
"var"  	{a(); return Parser.SYM_VAR;}
"nil"  	{a(); return Parser.SYM_NIL;}
"function"	{a(); return Parser.SYM_FUNCTION;}
"let"  	{a(); return Parser.SYM_LET;}
"in"   	{a(); return Parser.SYM_IN;}
"end"  	{a(); return Parser.SYM_END;}
"if"   	{a(); return Parser.SYM_IF;}
"then" 	{a(); return Parser.SYM_THEN;}
"else" 	{a(); return Parser.SYM_ELSE;}
"while"	{a(); return Parser.SYM_WHILE;}
"do"   	{a(); return Parser.SYM_DO;}
"for"  	{a(); return Parser.SYM_FOR;}
"to"   	{a(); return Parser.SYM_TO;}
"break"	{a(); return Parser.SYM_BREAK;}

"\""	{a(); svalue = ""; yybegin(STRING);}
"/*"	{a(); comment_nesting = 1; yybegin(COMMENT);}
[a-zA-Z][a-zA-Z0-9_]*	{a(); yylval = new Identifier(yytext()); return Parser.SYM_ID;}
[0-9]+	{a(); yylval = new IntValue(Integer.parseInt(yytext())); return Parser.SYM_INT;}
.	{a(); error("Illegal character: " + yytext());}
}

<STRING>"\""  	{a(); yylval = new StringValue(svalue); yybegin(YYINITIAL); return Parser.SYM_STRING;}
<STRING>"\\n" 	{a(); svalue += "\n";}
<STRING>"\\r" 	{a(); svalue += "\r";}
<STRING>"\\t" 	{a(); svalue += "\t";}
<STRING>\\[0-9]{3}	{a(); svalue += Integer.parseInt(yytext().substring(1));}
<STRING>\\\"	{a(); svalue += "\"";}
<STRING>\\\\	{a(); svalue += "\\";}
<STRING>\\  	{a(); yybegin(STRINGIGNORE);}
<STRING>.   	{a(); svalue += yytext();}
<STRING>\n  	{error("Unescaped end-of-line inside string constant");}
<STRING><<EOF>>	{error("Unterminated string constant");}
<STRINGIGNORE>" "		{a();}
<STRINGIGNORE>[\t]		{a("    ");}
<STRINGIGNORE>[\r]		{a(" ");}
<STRINGIGNORE>\n     	{errors.newline();}
<STRINGIGNORE>\r\n     	{errors.newline();}
<STRINGIGNORE>\\       	{a(); yybegin(STRING);}
<STRINGIGNORE><<EOF>>	{error("Unterminated string constant"); errors.eof();}
<STRINGIGNORE>.      	{error("Backslash followed by other than valid escape sequence or white space and a backslash");}

<COMMENT>"/*"	{a(); comment_nesting++;}
<COMMENT>"*/"	{a(); comment_nesting--; if (comment_nesting == 0) {yybegin(YYINITIAL);}}
<COMMENT>\n  	{errors.newline();}
<COMMENT>\r\n  	{errors.newline();}
<COMMENT>\r  	{a(" ");}
<COMMENT>\t  	{a("    ");}
<COMMENT>.   	{a(); }
