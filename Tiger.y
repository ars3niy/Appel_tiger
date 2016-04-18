%{
#include <syntaxtree.h>
#include <idmap.h>

#include <errormsg.h>
#include <string>
#include <stdio.h>

int yyerror(const char *s);
int yylex();
extern void ProcessTree(YYSTYPE tree);

static IdTracker idProvider;

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
program:	expression {ProcessTree($1);}

expression: lvalue
          | SYM_NIL {$$ = std::make_shared<Syntax::Node>(Syntax::NIL);}
          | SYM_OPENPAREN sequence_2plus SYM_CLOSEPAREN {$$ = std::make_shared<Syntax::Sequence>($2);}
          | SYM_INT {$$ = $1;}
          | SYM_STRING {$$ = $1;}
          | SYM_MINUS expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_MINUS, std::make_shared<Syntax::IntValue>(0), $2);} %prec UNARYMINUS
          | call
          | expression SYM_PLUS expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_PLUS, $1, $3);}
          | expression SYM_MINUS expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_MINUS, $1, $3);}
          | expression SYM_ASTERISK expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_ASTERISK, $1, $3);}
          | expression SYM_SLASH expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_SLASH, $1, $3);}
          | expression SYM_EQUAL expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_EQUAL, $1, $3);}
          | expression SYM_NONEQUAL expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_NONEQUAL, $1, $3);}
          | expression SYM_LESS expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_LESS, $1, $3);}
          | expression SYM_LESSEQUAL expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_LESSEQUAL, $1, $3);}
          | expression SYM_GREATER expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_GREATER, $1, $3);}
          | expression SYM_GREATEQUAL expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_GREATEQUAL, $1, $3);}
          | expression SYM_AND expression {$$ = std::make_shared<Syntax::IfElse>($1, $3, std::make_shared<Syntax::IntValue>(0), true);}
          | expression SYM_OR expression {$$ = std::make_shared<Syntax::IfElse>($1, std::make_shared<Syntax::IntValue>(1), $3, true);}
          | record_value
          | index_expression SYM_OF expression {
              std::shared_ptr<Syntax::ArrayInstantiation> a = std::make_shared<Syntax::ArrayInstantiation>($1, $3);
              $$ = a;
              if (! a->arrayIsSingleId()) yyerror("Invalid array definition, some shit instead of type name");
          }
          | lvalue SYM_ASSIGN expression {$$ = std::make_shared<Syntax::BinaryOp>(SYM_ASSIGN, $1, $3);}
          | SYM_IF expression SYM_THEN expression {$$ = std::make_shared<Syntax::If>($2, $4);}
          | SYM_IF expression SYM_THEN expression SYM_ELSE expression {$$ = std::make_shared<Syntax::IfElse>($2, $4, $6, false);}
          | SYM_WHILE expression SYM_DO expression {$$ = std::make_shared<Syntax::While>($2, $4);}
          | SYM_FOR SYM_ID SYM_ASSIGN expression SYM_TO expression SYM_DO expression {$$ = std::make_shared<Syntax::For>($2, $4, $6, $8, idProvider.getId());}
          | SYM_BREAK {$$ = std::make_shared<Syntax::Node>(Syntax::BREAK);}
          | SYM_LET declarations SYM_IN sequence SYM_END {$$ = std::make_shared<Syntax::Scope>($2, $4);}
          | SYM_OPENPAREN expression SYM_CLOSEPAREN {$$ = $2;}
          | SYM_OPENPAREN SYM_CLOSEPAREN {$$ = std::make_shared<Syntax::Sequence>(std::make_shared<Syntax::ExpressionList>());}

lvalue: SYM_ID {$$ = $1;}
      | lvalue SYM_DOT SYM_ID {$$ = std::make_shared<Syntax::RecordField>($1, $3);}
      | index_expression {$$ = $1;}

index_expression: lvalue SYM_OPENBRACKET expression SYM_CLOSEBRACKET {
                    $$ = std::make_shared<Syntax::ArrayIndexing>($1, $3);
                }

sequence_2plus: expression SYM_SEMICOLON expression {
                  std::shared_ptr<Syntax::ExpressionList> s = std::make_shared<Syntax::ExpressionList>();
                  s->prepend($3);
                  s->prepend($1);
                  $$ = s;
              }
              | expression SYM_SEMICOLON sequence_2plus {
                  std::shared_ptr<Syntax::ExpressionList> s = std::static_pointer_cast<Syntax::ExpressionList>($3);
                  s->prepend($1);
                  $$ = s;
              }

sequence: {$$ = std::make_shared<Syntax::ExpressionList>();}
        | sequence_nonempty {$$ = $1;}

sequence_nonempty: expression {
                     std::shared_ptr<Syntax::ExpressionList> s = std::make_shared<Syntax::ExpressionList>();
                     s->prepend($1);
                     $$ = s;
                 }
                 | expression SYM_SEMICOLON sequence_nonempty {
                     std::shared_ptr<Syntax::ExpressionList> s = std::static_pointer_cast<Syntax::ExpressionList>($3);
                     s->prepend($1);
                     $$ = s;
                 }
              
call: SYM_ID SYM_OPENPAREN commasequence SYM_CLOSEPAREN {
        $$ = std::make_shared<Syntax::FunctionCall>($1, $3);
    }

commasequence: {$$ = std::make_shared<Syntax::ExpressionList>();}
             | commasequence_nonempty {$$ = $1;}

commasequence_nonempty: expression {
                          std::shared_ptr<Syntax::ExpressionList> s = std::make_shared<Syntax::ExpressionList>();
                          s->prepend($1);
                          $$ = s;
                      }
                      | expression SYM_COMMA commasequence_nonempty {
                          std::shared_ptr<Syntax::ExpressionList> s = std::static_pointer_cast<Syntax::ExpressionList>($3);
                          s->prepend($1);
                          $$ = s;
                      }

record_value: SYM_ID SYM_OPENBRACE fieldlist SYM_CLOSEBRACE {
                $$ = std::make_shared<Syntax::RecordInstantiation>($1, $3);
            }

fieldlist: {$$ = std::make_shared<Syntax::ExpressionList>();}
         | fieldlist_nonempty {$$ = $1;}

fieldlist_nonempty: SYM_ID SYM_EQUAL expression {
                      std::shared_ptr<Syntax::ExpressionList> s = std::make_shared<Syntax::ExpressionList>();
                      s->prepend(std::make_shared<Syntax::BinaryOp>(SYM_ASSIGN, $1, $3));
                      $$ = s;
                  }
                  | SYM_ID SYM_EQUAL expression SYM_COMMA fieldlist_nonempty {
                      std::shared_ptr<Syntax::ExpressionList> s = std::static_pointer_cast<Syntax::ExpressionList>($5);
                      s->prepend(std::make_shared<Syntax::BinaryOp>(SYM_ASSIGN, $1, $3));
                      $$ = s;
                  }

declarations: {$$ = std::make_shared<Syntax::ExpressionList>();}
            | declaration declarations {
                std::shared_ptr<Syntax::ExpressionList> s = std::static_pointer_cast<Syntax::ExpressionList>($2);
                s->prepend($1);
                $$ = s;
            }

declaration: typedec {$$ = $1;}
           | vardec {$$ = $1;}
           | funcdec {$$ = $1;}

typedec: SYM_TYPE SYM_ID SYM_EQUAL typespec {$$ = std::make_shared<Syntax::TypeDeclaration>($2, $4);}

typespec: SYM_ID {$$ = $1;}
        | SYM_OPENBRACE fieldsdec SYM_CLOSEBRACE {$$ = std::make_shared<Syntax::RecordTypeDefinition>($2);}
        | SYM_ARRAY SYM_OF SYM_ID {$$ = std::make_shared<Syntax::ArrayTypeDefinition>($3);}

fieldsdec: {$$ = std::make_shared<Syntax::ExpressionList>();}
         | fieldsdec_nonempty {$$ = $1;}

fieldsdec_nonempty: SYM_ID SYM_COLON SYM_ID {
                      std::shared_ptr<Syntax::ExpressionList> s = std::make_shared<Syntax::ExpressionList>();
                      s->prepend(std::make_shared<Syntax::ParameterDeclaration>(idProvider.getId(), $1, $3));
                      $$ = s;
                  }
                  | SYM_ID SYM_COLON SYM_ID SYM_COMMA fieldsdec_nonempty {
                      std::shared_ptr<Syntax::ExpressionList> s = std::static_pointer_cast<Syntax::ExpressionList>($5);
                      s->prepend(std::make_shared<Syntax::ParameterDeclaration>(idProvider.getId(), $1, $3));
                      $$ = s;
                  }

vardec: SYM_VAR SYM_ID SYM_ASSIGN expression {$$ = std::make_shared<Syntax::VariableDeclaration>(idProvider.getId(), $2, $4);}
      | SYM_VAR SYM_ID SYM_COLON SYM_ID SYM_ASSIGN expression {$$ = std::make_shared<Syntax::VariableDeclaration>(idProvider.getId(), $2, $6, $4);}

funcdec: SYM_FUNCTION SYM_ID SYM_OPENPAREN fieldsdec SYM_CLOSEPAREN SYM_EQUAL expression {
           $$ = std::make_shared<Syntax::Function>(idProvider.getId(), $2, nullptr, $4, $7);
       }
       | SYM_FUNCTION SYM_ID SYM_OPENPAREN fieldsdec SYM_CLOSEPAREN {
           $$ = std::make_shared<Syntax::Function>(idProvider.getId(), $2, nullptr, $4, nullptr);
       }
       | SYM_FUNCTION SYM_ID SYM_OPENPAREN fieldsdec SYM_CLOSEPAREN SYM_COLON SYM_ID SYM_EQUAL expression {
           $$ = std::make_shared<Syntax::Function>(idProvider.getId(), $2, $7, $4, $9);
       }
       | SYM_FUNCTION SYM_ID SYM_OPENPAREN fieldsdec SYM_CLOSEPAREN SYM_COLON SYM_ID{
           $$ = std::make_shared<Syntax::Function>(idProvider.getId(), $2, $7, $4, nullptr);
       }

%%
int yyerror(const char *s)
{
	Error::error(s);
	exit(1);
}
