%{
  #include <stdlib.h>
  #include <string.h>
  #include <math.h>

  #include "global.h"
  #include "frontend.h"

  #define GrammarError(x...) do { currentContext->rememberError(linebuf, tokenPos); currentContext->Error(x); } while (0)
  #define currentScope	     (currentContext->getCurrentScope())
  
  extern char *linebuf;
  extern int tokenPos;
  extern ASTVignette *astResult;
  static int enumCounter = 0;

  void yyerror(char *); 
  int yylex();
  
  static SourceContext *getContext()
  
  {
    return new SourceContext(currentContext->filename, linebuf, currentContext->lineno, tokenPos);
  }
%}

%start body

%union {
  const char         *id;
  unsigned int	     integer;
  long double	     real;
  char		     *str;
  char		     chr;
  ASTExpression      *expr;
  ASTStatement       *stmt;
  ASTConstExpression *cons;
  SourceContext      *ctxt;
  ASTVariable        *var;
  FEParams           *par;
  ASTArrayExpression *arr;
}

%token	<id>		ID
%token	<integer>	LIT_INT
%token	<real>		LIT_REAL
%token	<str>		LIT_STRING
%token	<chr>		LIT_CHR

%token SEMI
%token LBRACE
%token RBRACE
%token COLON
%token COMMA
%token EQUAL
%token ARROW
%token DBLARROW
%token ADD
%token SUB
%token MUL
%token DIV
%token LPAREN
%token RPAREN
%token LT
%token GT
%token LBRACK
%token RBRACK
%token BTRUE
%token BFALSE
%token BEQUAL
%token OR
%token FOR
%token TO
%token DO
%token ENDFOR
%token BANG
%token PIPE
%token AND
%token IF
%token THEN
%token ELSE

%left MUL DIV MOD ADD SUB ARROW LOLLIPOP FINMAP
%right FUZZ

%type <id>   id
%type <expr> addex
%type <expr> logex
%type <expr> number
%type <expr> mulex
%type <expr> cmpex
%type <expr> term
%type <stmt> statement
%type <stmt> statements
%type <expr> expr
%type <par> params
%type <var> var
%type <arr> arrayinits

%%

body            : statements { astResult = new ASTVignette($1, getContext()); }
         
statements      : statements statement { $1->appendStatement($2); $$ = $1; }
                | statement
                ;

statement       : var EQUAL logex SEMI {
                  currentScope->add(SYM_LETBINDING, $1->getName());
                  $$ = new ASTLetBinding($1, $3, NULL, getContext());
                }
                | FOR id EQUAL LIT_INT TO LIT_INT DO statements ENDFOR SEMI {
                  $$ = new ASTForStatement($2, $4, $6, $8, getContext());
                }
                | IF expr THEN LBRACE statements RBRACE ELSE LBRACE statements RBRACE {
                    $$ = new ASTIfThenElseStatement($2, $5, $9, getContext());
                  }
                | IF expr THEN LBRACE statements RBRACE {
                    $$ = new ASTIfThenElseStatement($2, $5, NULL, getContext());
                  }
                | logex SEMI {
                  $$ = new ASTExpressionStatement($1, getContext());
                }
                ;

params          : params COMMA logex { $$ = $1; $$->add($3); }
                | logex { $$ = new FEParams(); $$->add($1); }
                ; 

expr            : logex
                ;

logex           : logex OR cmpex { $$ = new ASTInfixOpExpression($1, "||", $3, getContext()); }
                | logex AND cmpex { $$ = new ASTInfixOpExpression($1, "&&", $3, getContext()); }
                | BANG cmpex { panic("No bang implemented"); }
                | cmpex
                ;
                
cmpex           : cmpex LT addex { $$ = new ASTInfixOpExpression($1, "<", $3, getContext()); }
                | cmpex GT addex { $$ = new ASTInfixOpExpression($1, ">", $3, getContext()); }
                | cmpex LT EQUAL addex { $$ = new ASTInfixOpExpression($1, "<=", $4, getContext()); }
                | cmpex GT EQUAL addex { $$ = new ASTInfixOpExpression($1, ">=", $4, getContext()); }
                | cmpex BEQUAL addex  { $$ = new ASTInfixOpExpression($1, "==", $3, getContext()); }
                | addex
                ;
                
addex           : addex ADD mulex {
                    $$ = new ASTInfixOpExpression($1, "+", $3, getContext());
                  }
                | addex SUB mulex {
                    $$ = new ASTInfixOpExpression($1, "-", $3, getContext());
                  }
                | error ADD mulex { GrammarError("Expecting expression"); $$ = $3; }
                | error SUB mulex { GrammarError("Expecting expression"); $$ = $3; }
                | mulex
                ;
                
mulex           : mulex MUL term {
                    $$ = new ASTInfixOpExpression($1, "*", $3, getContext());
                  }
                | mulex DIV term  {
                    $$ = new ASTInfixOpExpression($1, "/", $3, getContext());
                  }
                | error MUL term { GrammarError("Expecting expression"); $$ = $3; }
                | error DIV term { GrammarError("Expecting expression"); $$ = $3; }
                | term
                ;      
                
term            : LPAREN expr RPAREN { $$ = $2; }
                | ID LPAREN params RPAREN {
                  if (currentScope->symbolExists($1, SYM_FUNCTION)) {
                    ASTFunctionExpression *fe = new ASTFunctionExpression($1, getContext());
                    for (int i=0; i<$3->getNum(); i++)
                      fe->addArgument($3->get(i));
                    $$ = fe;
                  } else {
                    GrammarError("Unknown function: '%s'", $1);
                    $$ = new ASTConstInt(0, getContext());
                  }
                }
                | var { $$ = $1; }
                | number
                | LBRACK arrayinits RBRACK  { $$ = $2; }
                | BTRUE { $$ = new ASTConstBool(true, getContext()); }
                | BFALSE { $$ = new ASTConstBool(false, getContext()); }
                ;

arrayinits      : arrayinits COMMA expr { $1->addElement($3); $$ = $1; }
                | expr { $$ = new ASTArrayExpression(getContext()); $$->addElement($1); }
                ;

var             : var LBRACK expr RBRACK {
                    $1->addIndex($3);
                    $$ = $1;
                  }
                | ID {
//                    if (currentScope->symbolExists($1, SYM_LETBINDING)) {
                    $$ = new ASTVariable($1, getContext());
//                    } else {
//                      GrammarError("Unknown identifier: '%s'", $1);
//                      $$ = new ASTVariable("foo", getContext());
//                    }
                  }

number          : LIT_INT { $$ = new ASTConstInt($1, getContext()); }
                | LIT_REAL { $$ = new ASTConstReal($1, getContext()); }
                ;

id		: ID
		;
                
%%

void
yyerror(char * s)
{
  GrammarError(s);
}
