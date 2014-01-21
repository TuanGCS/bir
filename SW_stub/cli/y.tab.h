/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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
     T_SHOW = 258,
     T_QUESTION = 259,
     T_NEWLINE = 260,
     T_ALL = 261,
     T_VNS = 262,
     T_USER = 263,
     T_SERVER = 264,
     T_VHOST = 265,
     T_LHOST = 266,
     T_TOPOLOGY = 267,
     T_IP = 268,
     T_ROUTE = 269,
     T_INTF = 270,
     T_ARP = 271,
     T_OSPF = 272,
     T_HW = 273,
     T_NEIGHBORS = 274,
     T_ADD = 275,
     T_DEL = 276,
     T_UP = 277,
     T_DOWN = 278,
     T_PURGE = 279,
     T_STATIC = 280,
     T_DYNAMIC = 281,
     T_ABOUT = 282,
     T_PING = 283,
     T_TRACE = 284,
     T_HELP = 285,
     T_EXIT = 286,
     T_SHUTDOWN = 287,
     T_FLOOD = 288,
     T_SET = 289,
     T_UNSET = 290,
     T_OPTION = 291,
     T_VERBOSE = 292,
     T_DATE = 293,
     TAV_INT = 294,
     TAV_IP = 295,
     TAV_MAC = 296,
     TAV_STR = 297
   };
#endif
/* Tokens.  */
#define T_SHOW 258
#define T_QUESTION 259
#define T_NEWLINE 260
#define T_ALL 261
#define T_VNS 262
#define T_USER 263
#define T_SERVER 264
#define T_VHOST 265
#define T_LHOST 266
#define T_TOPOLOGY 267
#define T_IP 268
#define T_ROUTE 269
#define T_INTF 270
#define T_ARP 271
#define T_OSPF 272
#define T_HW 273
#define T_NEIGHBORS 274
#define T_ADD 275
#define T_DEL 276
#define T_UP 277
#define T_DOWN 278
#define T_PURGE 279
#define T_STATIC 280
#define T_DYNAMIC 281
#define T_ABOUT 282
#define T_PING 283
#define T_TRACE 284
#define T_HELP 285
#define T_EXIT 286
#define T_SHUTDOWN 287
#define T_FLOOD 288
#define T_SET 289
#define T_UNSET 290
#define T_OPTION 291
#define T_VERBOSE 292
#define T_DATE 293
#define TAV_INT 294
#define TAV_IP 295
#define TAV_MAC 296
#define TAV_STR 297




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 81 "cli_parser.y"

    int intVal;
    addr_ip_t ip;
    addr_mac_t mac;
    char string[MAX_STR_LEN];



/* Line 2068 of yacc.c  */
#line 143 "y.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


