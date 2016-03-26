%{
#include <errormsg.h>
#include <string>
#include <stdio.h>

extern int ivalue;
extern std::string svalue;

int yyerror(char *s);
int yylex();
%}

%token SYM_COMMA
%token SYM_COLON
%token SYM_EQUAL
%token SYM_ASSIGN
%token SYM_OPENPAREN
%token SYM_CLOSEPAREN
%token SYM_DOT
%token SYM_OPENBRACKET
%token SYM_CLOSEBRACKET
%token SYM_OPENBRACE
%token SYM_CLOSEBRACE
%token SYM_PLUS
%token SYM_MINUS
%token SYM_ASTERISK
%token SYM_SLASH
%token SYM_SEMICOLON
%token SYM_LESS
%token SYM_GREATER
%token SYM_LESSEQUAL
%token SYM_GREATEQUAL
%token SYM_NONEQUAL
%token SYM_AND
%token SYM_OR

%token SYM_TYPE
%token SYM_ARRAY
%token SYM_OF
%token SYM_VAR
%token SYM_NIL
%token SYM_FUNCTION
%token SYM_LET
%token SYM_IN
%token SYM_END
%token SYM_IF
%token SYM_THEN
%token SYM_ELSE
%token SYM_WHILE
%token SYM_DO
%token SYM_FOR
%token SYM_TO
%token SYM_BREAK

%token SYM_STRING
%token SYM_INT
%token SYM_ID

%nonassoc SYM_ASSIGN
%left  SYM_WHILE SYM_DO SYM_FOR SYM_TO SYM_OF
%right SYM_IF SYM_THEN SYM_ELSE
%left SYM_OR
%left SYM_AND
%nonassoc SYM_EQUAL SYM_NONEQUAL SYM_LESS SYM_LESSEQUAL SYM_GREATER SYM_GREATEQUAL
%left SYM_PLUS SYM_MINUS
%left SYM_ASTERISK SYM_SLASH
%left UNARYMINUS
%left SYM_DOT SYM_OPENBRACKET

%start program
%error-verbose

%%
program:	expression

expression: lvalue
          | SYM_NIL {printf("nil\n");}
          | SYM_OPENPAREN sequence_2plus SYM_CLOSEPAREN {printf("Sequence\n");}
          | SYM_INT {printf("Integer %d\n", ivalue);}
          | SYM_STRING {printf("String %s\n", svalue.c_str());}
          | SYM_MINUS expression {printf("Negated\n");} %prec UNARYMINUS
          | call
          //| SYM_ID SYM_OPENPAREN arguments SYM_CLOSEPAREN {printf("Call to function %s\n", svalue.c_str());}
          | expression SYM_PLUS expression {printf("Addition\n");}
          | expression SYM_MINUS expression {printf("Subtraction\n");}
          | expression SYM_ASTERISK expression {printf("Multiplication\n");}
          | expression SYM_SLASH expression {printf("Division\n");}
          | expression SYM_EQUAL expression {printf("Equality\n");}
          | expression SYM_NONEQUAL expression {printf("Nonequality\n");}
          | expression SYM_LESS expression {printf("Less\n");}
          | expression SYM_LESSEQUAL expression {printf("Less-equal\n");}
          | expression SYM_GREATER expression {printf("Greater\n");}
          | expression SYM_GREATEQUAL expression {printf("Greater-equal\n");}
          | expression SYM_AND expression {printf("Conqunction\n");}
          | expression SYM_OR expression {printf("Disjunction\n");}
          | record_value
          | index_expression SYM_OF expression {printf("Instance of array %s\n", svalue.c_str());}
          | expression SYM_ASSIGN expression {printf("Assignment\n");}
          | SYM_IF expression SYM_THEN expression {printf("If, no else\n");}
          | SYM_IF expression SYM_THEN expression SYM_ELSE expression {printf("If, with else\n");}
          | SYM_WHILE expression SYM_DO expression {printf("While\n");}
          | SYM_FOR SYM_ID SYM_ASSIGN expression SYM_TO expression SYM_DO expression {printf("For\n");}
          | SYM_BREAK {printf("Break\n");}
          | SYM_LET declarations SYM_IN sequence SYM_END {printf("Scope\n");}
          | SYM_OPENPAREN expression SYM_CLOSEPAREN

lvalue: SYM_ID {printf("Identifier\n");}
      | lvalue SYM_DOT SYM_ID {printf("Record field\n");}
      | index_expression

index_expression: lvalue SYM_OPENBRACKET expression SYM_CLOSEBRACKET {printf("Array index\n");}

sequence_2plus: expression SYM_SEMICOLON expression
              | expression SYM_SEMICOLON sequence_2plus

sequence:
        | sequence_nonempty

sequence_nonempty: expression
                 | expression SYM_SEMICOLON sequence_nonempty
              
call: SYM_ID SYM_OPENPAREN commasequence SYM_CLOSEPAREN {printf("Function call %s\n", svalue.c_str());}

commasequence:
             | commasequence_nonempty

commasequence_nonempty: expression
                      | expression SYM_COMMA commasequence_nonempty

record_value: SYM_ID SYM_OPENBRACE fieldlist SYM_CLOSEBRACE {printf("Instance of record %s\n", svalue.c_str());}

fieldlist:
         | fieldlist_nonempty

fieldlist_nonempty: SYM_ID SYM_EQUAL expression
                  | SYM_ID SYM_EQUAL expression SYM_COMMA fieldlist_nonempty

declarations:
            | declaration declarations

declaration: typedec
           | vardec
           | funcdec

typedec: SYM_TYPE SYM_ID SYM_EQUAL typespec {printf("Declare type %s\n", svalue.c_str());}

typespec: SYM_ID
        | SYM_OPENBRACE fieldsdec
        | SYM_ARRAY SYM_OF SYM_ID

fieldsdec:
         | fieldsdec_nonempty

fieldsdec_nonempty: SYM_ID SYM_COLON SYM_ID
                  | SYM_ID SYM_COLON SYM_ID SYM_COMMA fieldsdec_nonempty

vardec: SYM_VAR SYM_ID SYM_ASSIGN expression
      | SYM_VAR SYM_ID SYM_COLON SYM_ID SYM_ASSIGN expression

funcdec: SYM_FUNCTION SYM_ID SYM_OPENPAREN fieldsdec SYM_CLOSEPAREN SYM_EQUAL expression
       | SYM_FUNCTION SYM_ID SYM_OPENPAREN fieldsdec SYM_CLOSEPAREN SYM_COLON SYM_ID SYM_EQUAL expression

%%
int yyerror(char *s)
{
	Err_error(s);
}
