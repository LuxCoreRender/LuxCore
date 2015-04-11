/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

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

#ifndef YY_LUXCORE_PARSERLXS_YY_HOME_DAVID_PROJECTS_LUXRENDER_DEV_LUXRAYS_SRC_LUXCORE_LUXPARSER_LUXPARSE_HPP_INCLUDED
# define YY_LUXCORE_PARSERLXS_YY_HOME_DAVID_PROJECTS_LUXRENDER_DEV_LUXRAYS_SRC_LUXCORE_LUXPARSER_LUXPARSE_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int luxcore_parserlxs_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    STRING = 258,
    ID = 259,
    NUM = 260,
    LBRACK = 261,
    RBRACK = 262,
    ACCELERATOR = 263,
    AREALIGHTSOURCE = 264,
    ATTRIBUTEBEGIN = 265,
    ATTRIBUTEEND = 266,
    CAMERA = 267,
    CONCATTRANSFORM = 268,
    COORDINATESYSTEM = 269,
    COORDSYSTRANSFORM = 270,
    EXTERIOR = 271,
    FILM = 272,
    IDENTITY = 273,
    INTERIOR = 274,
    LIGHTSOURCE = 275,
    LOOKAT = 276,
    MATERIAL = 277,
    MAKENAMEDMATERIAL = 278,
    MAKENAMEDVOLUME = 279,
    MOTIONBEGIN = 280,
    MOTIONEND = 281,
    NAMEDMATERIAL = 282,
    OBJECTBEGIN = 283,
    OBJECTEND = 284,
    OBJECTINSTANCE = 285,
    PORTALINSTANCE = 286,
    MOTIONINSTANCE = 287,
    LIGHTGROUP = 288,
    PIXELFILTER = 289,
    RENDERER = 290,
    REVERSEORIENTATION = 291,
    ROTATE = 292,
    SAMPLER = 293,
    SCALE = 294,
    SEARCHPATH = 295,
    PORTALSHAPE = 296,
    SHAPE = 297,
    SURFACEINTEGRATOR = 298,
    TEXTURE = 299,
    TRANSFORMBEGIN = 300,
    TRANSFORMEND = 301,
    TRANSFORM = 302,
    TRANSLATE = 303,
    VOLUME = 304,
    VOLUMEINTEGRATOR = 305,
    WORLDBEGIN = 306,
    WORLDEND = 307,
    HIGH_PRECEDENCE = 308
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 688 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y" /* yacc.c:1909  */

char string[1024];
float num;
ParamArray *ribarray;

#line 114 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.hpp" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE luxcore_parserlxs_yylval;

int luxcore_parserlxs_yyparse (void);

#endif /* !YY_LUXCORE_PARSERLXS_YY_HOME_DAVID_PROJECTS_LUXRENDER_DEV_LUXRAYS_SRC_LUXCORE_LUXPARSER_LUXPARSE_HPP_INCLUDED  */
