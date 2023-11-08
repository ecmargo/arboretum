/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ID = 258,
     LIT_INT = 259,
     LIT_REAL = 260,
     LIT_STRING = 261,
     LIT_CHR = 262,
     SEMI = 263,
     LBRACE = 264,
     RBRACE = 265,
     COLON = 266,
     COMMA = 267,
     EQUAL = 268,
     ARROW = 269,
     DBLARROW = 270,
     ADD = 271,
     SUB = 272,
     MUL = 273,
     DIV = 274,
     LPAREN = 275,
     RPAREN = 276,
     LT = 277,
     GT = 278,
     LBRACK = 279,
     RBRACK = 280,
     BTRUE = 281,
     BFALSE = 282,
     BEQUAL = 283,
     OR = 284,
     FOR = 285,
     TO = 286,
     DO = 287,
     ENDFOR = 288,
     BANG = 289,
     PIPE = 290,
     AND = 291,
     IF = 292,
     THEN = 293,
     ELSE = 294,
     FINMAP = 295,
     LOLLIPOP = 296,
     MOD = 297,
     FUZZ = 298
   };
#endif
/* Tokens.  */
#define ID 258
#define LIT_INT 259
#define LIT_REAL 260
#define LIT_STRING 261
#define LIT_CHR 262
#define SEMI 263
#define LBRACE 264
#define RBRACE 265
#define COLON 266
#define COMMA 267
#define EQUAL 268
#define ARROW 269
#define DBLARROW 270
#define ADD 271
#define SUB 272
#define MUL 273
#define DIV 274
#define LPAREN 275
#define RPAREN 276
#define LT 277
#define GT 278
#define LBRACK 279
#define RBRACK 280
#define BTRUE 281
#define BFALSE 282
#define BEQUAL 283
#define OR 284
#define FOR 285
#define TO 286
#define DO 287
#define ENDFOR 288
#define BANG 289
#define PIPE 290
#define AND 291
#define IF 292
#define THEN 293
#define ELSE 294
#define FINMAP 295
#define LOLLIPOP 296
#define MOD 297
#define FUZZ 298




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 29 "src/parser.yy"
{
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
/* Line 1529 of yacc.c.  */
#line 150 "parser.hh"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

