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
     STRING = 258,
     VARIABLE = 259,
     IDENTIFIER = 260,
     FOREACH = 261,
     COPY = 262,
     SKIP = 263,
     TO = 264,
     PAST = 265,
     END = 266,
     NEWLINE = 267,
     REPLACE = 268,
     THEN = 269,
     ELSE = 270,
     IF = 271,
     NOT = 272,
     SET = 273,
     PRINT = 274,
     MATCH = 275,
     STYLE = 276,
     NUMBER = 277,
     INPUT = 278,
     OUTPUT = 279,
     ERROR = 280,
     ALL = 281
   };
#endif
/* Tokens.  */
#define STRING 258
#define VARIABLE 259
#define IDENTIFIER 260
#define FOREACH 261
#define COPY 262
#define SKIP 263
#define TO 264
#define PAST 265
#define END 266
#define NEWLINE 267
#define REPLACE 268
#define THEN 269
#define ELSE 270
#define IF 271
#define NOT 272
#define SET 273
#define PRINT 274
#define MATCH 275
#define STYLE 276
#define NUMBER 277
#define INPUT 278
#define OUTPUT 279
#define ERROR 280
#define ALL 281




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union
#line 27 "grammar.ypp"
ParseResult {
  Statement * stmt;
  Expression * expr;
  int token;
  std::string * name;
  int integer;
}
/* Line 1529 of yacc.c.  */
#line 109 "grammar.tab.hpp"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

