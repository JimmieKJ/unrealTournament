/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse         _mesa_hlsl_parse
#define yylex           _mesa_hlsl_lex
#define yyerror         _mesa_hlsl_error
#define yylval          _mesa_hlsl_lval
#define yychar          _mesa_hlsl_char
#define yydebug         _mesa_hlsl_debug
#define yynerrs         _mesa_hlsl_nerrs
#define yylloc          _mesa_hlsl_lloc

/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 1 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:
//#define YYDEBUG 1
/*
 * Copyright © 2008, 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ShaderCompilerCommon.h"
#include "ast.h"
#include "glsl_parser_extras.h"
#include "glsl_types.h"

#define YYLEX_PARAM state->scanner

#undef yyerror

static void yyerror(YYLTYPE *loc, _mesa_glsl_parse_state *st, const char *msg)
{
   _mesa_glsl_error(loc, st, "%s", msg);
}


/* Line 268 of yacc.c  */
#line 125 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     CONST_TOK = 258,
     BOOL_TOK = 259,
     FLOAT_TOK = 260,
     INT_TOK = 261,
     UINT_TOK = 262,
     BREAK = 263,
     CONTINUE = 264,
     DO = 265,
     ELSE = 266,
     FOR = 267,
     IF = 268,
     DISCARD = 269,
     RETURN = 270,
     SWITCH = 271,
     CASE = 272,
     DEFAULT = 273,
     BVEC2 = 274,
     BVEC3 = 275,
     BVEC4 = 276,
     IVEC2 = 277,
     IVEC3 = 278,
     IVEC4 = 279,
     UVEC2 = 280,
     UVEC3 = 281,
     UVEC4 = 282,
     VEC2 = 283,
     VEC3 = 284,
     VEC4 = 285,
     HALF_TOK = 286,
     HVEC2 = 287,
     HVEC3 = 288,
     HVEC4 = 289,
     IN_TOK = 290,
     OUT_TOK = 291,
     INOUT_TOK = 292,
     UNIFORM = 293,
     VARYING = 294,
     GLOBALLYCOHERENT = 295,
     SHARED = 296,
     CENTROID = 297,
     NOPERSPECTIVE = 298,
     NOINTERPOLATION = 299,
     LINEAR = 300,
     MAT2X2 = 301,
     MAT2X3 = 302,
     MAT2X4 = 303,
     MAT3X2 = 304,
     MAT3X3 = 305,
     MAT3X4 = 306,
     MAT4X2 = 307,
     MAT4X3 = 308,
     MAT4X4 = 309,
     HMAT2X2 = 310,
     HMAT2X3 = 311,
     HMAT2X4 = 312,
     HMAT3X2 = 313,
     HMAT3X3 = 314,
     HMAT3X4 = 315,
     HMAT4X2 = 316,
     HMAT4X3 = 317,
     HMAT4X4 = 318,
     FMAT2X2 = 319,
     FMAT2X3 = 320,
     FMAT2X4 = 321,
     FMAT3X2 = 322,
     FMAT3X3 = 323,
     FMAT3X4 = 324,
     FMAT4X2 = 325,
     FMAT4X3 = 326,
     FMAT4X4 = 327,
     SAMPLERSTATE = 328,
     SAMPLERSTATE_CMP = 329,
     BUFFER = 330,
     TEXTURE1D = 331,
     TEXTURE1D_ARRAY = 332,
     TEXTURE2D = 333,
     TEXTURE2D_ARRAY = 334,
     TEXTURE2DMS = 335,
     TEXTURE2DMS_ARRAY = 336,
     TEXTURE3D = 337,
     TEXTURECUBE = 338,
     TEXTURECUBE_ARRAY = 339,
     RWBUFFER = 340,
     RWTEXTURE1D = 341,
     RWTEXTURE1D_ARRAY = 342,
     RWTEXTURE2D = 343,
     RWTEXTURE2D_ARRAY = 344,
     RWTEXTURE3D = 345,
     RWSTRUCTUREDBUFFER = 346,
     RWBYTEADDRESSBUFFER = 347,
     POINT_TOK = 348,
     LINE_TOK = 349,
     TRIANGLE_TOK = 350,
     LINEADJ_TOK = 351,
     TRIANGLEADJ_TOK = 352,
     POINTSTREAM = 353,
     LINESTREAM = 354,
     TRIANGLESTREAM = 355,
     INPUTPATCH = 356,
     OUTPUTPATCH = 357,
     STRUCT = 358,
     VOID_TOK = 359,
     WHILE = 360,
     CBUFFER = 361,
     IDENTIFIER = 362,
     TYPE_IDENTIFIER = 363,
     NEW_IDENTIFIER = 364,
     FLOATCONSTANT = 365,
     INTCONSTANT = 366,
     UINTCONSTANT = 367,
     BOOLCONSTANT = 368,
     STRINGCONSTANT = 369,
     FIELD_SELECTION = 370,
     LEFT_OP = 371,
     RIGHT_OP = 372,
     INC_OP = 373,
     DEC_OP = 374,
     LE_OP = 375,
     GE_OP = 376,
     EQ_OP = 377,
     NE_OP = 378,
     AND_OP = 379,
     OR_OP = 380,
     MUL_ASSIGN = 381,
     DIV_ASSIGN = 382,
     ADD_ASSIGN = 383,
     MOD_ASSIGN = 384,
     LEFT_ASSIGN = 385,
     RIGHT_ASSIGN = 386,
     AND_ASSIGN = 387,
     XOR_ASSIGN = 388,
     OR_ASSIGN = 389,
     SUB_ASSIGN = 390,
     INVARIANT = 391,
     VERSION_TOK = 392,
     EXTENSION = 393,
     LINE = 394,
     COLON = 395,
     EOL = 396,
     INTERFACE = 397,
     OUTPUT = 398,
     PRAGMA_DEBUG_ON = 399,
     PRAGMA_DEBUG_OFF = 400,
     PRAGMA_OPTIMIZE_ON = 401,
     PRAGMA_OPTIMIZE_OFF = 402,
     PRAGMA_INVARIANT_ALL = 403,
     ASM = 404,
     CLASS = 405,
     UNION = 406,
     ENUM = 407,
     TYPEDEF = 408,
     TEMPLATE = 409,
     THIS = 410,
     PACKED_TOK = 411,
     GOTO = 412,
     INLINE_TOK = 413,
     NOINLINE = 414,
     VOLATILE = 415,
     PUBLIC_TOK = 416,
     STATIC = 417,
     EXTERN = 418,
     EXTERNAL = 419,
     LONG_TOK = 420,
     SHORT_TOK = 421,
     DOUBLE_TOK = 422,
     HALF = 423,
     FIXED_TOK = 424,
     UNSIGNED = 425,
     DVEC2 = 426,
     DVEC3 = 427,
     DVEC4 = 428,
     FVEC2 = 429,
     FVEC3 = 430,
     FVEC4 = 431,
     SAMPLER2DRECT = 432,
     SAMPLER3DRECT = 433,
     SAMPLER2DRECTSHADOW = 434,
     SIZEOF = 435,
     CAST = 436,
     NAMESPACE = 437,
     USING = 438,
     ERROR_TOK = 439,
     COMMON = 440,
     PARTITION = 441,
     ACTIVE = 442,
     SAMPLERBUFFER = 443,
     FILTER = 444,
     IMAGE1D = 445,
     IMAGE2D = 446,
     IMAGE3D = 447,
     IMAGECUBE = 448,
     IMAGE1DARRAY = 449,
     IMAGE2DARRAY = 450,
     IIMAGE1D = 451,
     IIMAGE2D = 452,
     IIMAGE3D = 453,
     IIMAGECUBE = 454,
     IIMAGE1DARRAY = 455,
     IIMAGE2DARRAY = 456,
     UIMAGE1D = 457,
     UIMAGE2D = 458,
     UIMAGE3D = 459,
     UIMAGECUBE = 460,
     UIMAGE1DARRAY = 461,
     UIMAGE2DARRAY = 462,
     IMAGE1DSHADOW = 463,
     IMAGE2DSHADOW = 464,
     IMAGEBUFFER = 465,
     IIMAGEBUFFER = 466,
     UIMAGEBUFFER = 467,
     IMAGE1DARRAYSHADOW = 468,
     IMAGE2DARRAYSHADOW = 469,
     ROW_MAJOR = 470,
     COLUMN_MAJOR = 471
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 293 of yacc.c  */
#line 61 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"

   int n;
   float real;
   const char *identifier;
   const char *string_literal;

   struct ast_type_qualifier type_qualifier;

   ast_node *node;
   ast_type_specifier *type_specifier;
   ast_fully_specified_type *fully_specified_type;
   ast_function *function;
   ast_parameter_declarator *parameter_declarator;
   ast_function_definition *function_definition;
   ast_compound_statement *compound_statement;
   ast_expression *expression;
   ast_declarator_list *declarator_list;
   ast_struct_specifier *struct_specifier;
   ast_declaration *declaration;
   ast_switch_body *switch_body;
   ast_case_label *case_label;
   ast_case_label_list *case_label_list;
   ast_case_statement *case_statement;
   ast_case_statement_list *case_statement_list;

   struct {
	  ast_node *cond;
	  ast_expression *rest;
   } for_rest_statement;

   struct {
	  ast_node *then_statement;
	  ast_node *else_statement;
   } selection_rest_statement;

   ast_attribute* attribute;
   ast_attribute_arg* attribute_arg;



/* Line 293 of yacc.c  */
#line 418 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 443 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
	     && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   6025

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  241
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  104
/* YYNRULES -- Number of rules.  */
#define YYNRULES  359
/* YYNRULES -- Number of states.  */
#define YYNSTATES  550

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   471

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   225,     2,     2,     2,   229,   232,     2,
     217,   218,   227,   223,   222,   224,   221,   228,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   236,   238,
     230,   237,   231,   235,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   219,     2,   220,   233,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   239,   234,   240,   226,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     7,     9,    11,    13,    15,    18,
      20,    22,    24,    26,    28,    30,    32,    36,    38,    43,
      45,    49,    52,    55,    57,    59,    61,    65,    68,    71,
      74,    76,    79,    83,    86,    88,    90,    92,    95,    98,
     101,   103,   106,   110,   113,   115,   118,   121,   124,   129,
     131,   133,   135,   137,   139,   143,   147,   151,   153,   157,
     161,   163,   167,   171,   173,   177,   181,   185,   189,   191,
     195,   199,   201,   205,   207,   211,   213,   217,   219,   223,
     225,   229,   231,   237,   239,   243,   245,   247,   249,   251,
     253,   255,   257,   259,   261,   263,   265,   267,   271,   273,
     276,   278,   281,   284,   289,   291,   293,   296,   300,   304,
     309,   312,   317,   322,   325,   333,   337,   341,   346,   349,
     352,   354,   358,   361,   363,   365,   367,   370,   373,   375,
     377,   379,   381,   383,   385,   387,   391,   397,   401,   409,
     415,   421,   423,   426,   431,   434,   441,   446,   451,   454,
     456,   459,   461,   463,   465,   467,   470,   473,   475,   477,
     479,   481,   483,   486,   489,   493,   495,   497,   499,   502,
     504,   506,   509,   512,   514,   516,   518,   520,   523,   526,
     528,   530,   532,   534,   536,   540,   544,   549,   554,   556,
     558,   565,   570,   575,   580,   587,   594,   596,   598,   600,
     602,   604,   606,   608,   610,   612,   614,   616,   618,   620,
     622,   624,   626,   628,   630,   632,   634,   636,   638,   640,
     642,   644,   646,   648,   650,   652,   654,   656,   658,   660,
     662,   664,   666,   668,   670,   672,   674,   676,   678,   680,
     682,   684,   686,   688,   690,   692,   694,   696,   698,   700,
     702,   704,   706,   708,   710,   712,   714,   716,   718,   720,
     722,   724,   730,   738,   743,   748,   755,   759,   765,   770,
     772,   775,   779,   781,   784,   786,   789,   792,   794,   796,
     800,   802,   804,   808,   815,   820,   825,   827,   831,   833,
     837,   840,   842,   844,   846,   849,   852,   854,   856,   858,
     860,   862,   864,   867,   868,   873,   875,   877,   880,   884,
     886,   889,   891,   894,   900,   904,   906,   908,   913,   919,
     922,   926,   930,   933,   935,   938,   941,   944,   946,   949,
     955,   963,   970,   972,   974,   976,   977,   980,   984,   987,
     990,   993,   997,  1000,  1002,  1004,  1006,  1008,  1010,  1014,
    1018,  1022,  1029,  1036,  1039,  1041,  1044,  1048,  1050,  1053
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     242,     0,    -1,    -1,   243,   245,    -1,   107,    -1,   108,
      -1,   109,    -1,   338,    -1,   245,   338,    -1,   107,    -1,
     109,    -1,   246,    -1,   111,    -1,   112,    -1,   110,    -1,
     113,    -1,   217,   276,   218,    -1,   247,    -1,   248,   219,
     249,   220,    -1,   250,    -1,   248,   221,   244,    -1,   248,
     118,    -1,   248,   119,    -1,   276,    -1,   251,    -1,   252,
      -1,   248,   221,   257,    -1,   254,   218,    -1,   253,   218,
      -1,   255,   104,    -1,   255,    -1,   255,   274,    -1,   254,
     222,   274,    -1,   256,   217,    -1,   295,    -1,   246,    -1,
     115,    -1,   259,   218,    -1,   258,   218,    -1,   260,   104,
      -1,   260,    -1,   260,   274,    -1,   259,   222,   274,    -1,
     246,   217,    -1,   248,    -1,   118,   261,    -1,   119,   261,
      -1,   262,   261,    -1,   217,   298,   218,   261,    -1,   223,
      -1,   224,    -1,   225,    -1,   226,    -1,   261,    -1,   263,
     227,   261,    -1,   263,   228,   261,    -1,   263,   229,   261,
      -1,   263,    -1,   264,   223,   263,    -1,   264,   224,   263,
      -1,   264,    -1,   265,   116,   264,    -1,   265,   117,   264,
      -1,   265,    -1,   266,   230,   265,    -1,   266,   231,   265,
      -1,   266,   120,   265,    -1,   266,   121,   265,    -1,   266,
      -1,   267,   122,   266,    -1,   267,   123,   266,    -1,   267,
      -1,   268,   232,   267,    -1,   268,    -1,   269,   233,   268,
      -1,   269,    -1,   270,   234,   269,    -1,   270,    -1,   271,
     124,   270,    -1,   271,    -1,   272,   125,   271,    -1,   272,
      -1,   272,   235,   276,   236,   274,    -1,   273,    -1,   261,
     275,   274,    -1,   237,    -1,   126,    -1,   127,    -1,   129,
      -1,   128,    -1,   135,    -1,   130,    -1,   131,    -1,   132,
      -1,   133,    -1,   134,    -1,   274,    -1,   276,   222,   274,
      -1,   273,    -1,   280,   238,    -1,   278,    -1,   288,   238,
      -1,   281,   218,    -1,   281,   218,   236,   244,    -1,   283,
      -1,   282,    -1,   283,   285,    -1,   282,   222,   285,    -1,
     290,   246,   217,    -1,   158,   290,   246,   217,    -1,   295,
     244,    -1,   295,   244,   237,   277,    -1,   295,   244,   236,
     244,    -1,   295,   312,    -1,   295,   244,   219,   277,   220,
     236,   244,    -1,   292,   286,   284,    -1,   286,   292,   284,
      -1,   292,   286,   292,   284,    -1,   286,   284,    -1,   292,
     284,    -1,   284,    -1,   292,   286,   287,    -1,   286,   287,
      -1,    35,    -1,    36,    -1,    37,    -1,    35,    36,    -1,
      36,    35,    -1,    93,    -1,    94,    -1,    95,    -1,    96,
      -1,    97,    -1,   295,    -1,   289,    -1,   288,   222,   244,
      -1,   288,   222,   244,   219,   220,    -1,   288,   222,   312,
      -1,   288,   222,   244,   219,   220,   237,   313,    -1,   288,
     222,   312,   237,   313,    -1,   288,   222,   244,   237,   313,
      -1,   290,    -1,   290,   244,    -1,   290,   244,   219,   220,
      -1,   290,   312,    -1,   290,   244,   219,   220,   237,   313,
      -1,   290,   312,   237,   313,    -1,   290,   244,   237,   313,
      -1,   136,   246,    -1,   295,    -1,   293,   295,    -1,    45,
      -1,    44,    -1,    43,    -1,   291,    -1,    42,   291,    -1,
     291,    42,    -1,    42,    -1,     3,    -1,    38,    -1,   294,
      -1,   291,    -1,   291,   294,    -1,   136,   294,    -1,   136,
     291,   294,    -1,   136,    -1,     3,    -1,    39,    -1,    42,
      39,    -1,    35,    -1,    36,    -1,    42,    35,    -1,    42,
      36,    -1,    38,    -1,   215,    -1,   216,    -1,   162,    -1,
       3,   162,    -1,   162,     3,    -1,    40,    -1,    41,    -1,
     296,    -1,   298,    -1,   297,    -1,   298,   219,   220,    -1,
     297,   219,   220,    -1,   298,   219,   277,   220,    -1,   297,
     219,   277,   220,    -1,   299,    -1,   300,    -1,   300,   230,
     299,   222,   111,   231,    -1,   300,   230,   299,   231,    -1,
      91,   230,   108,   231,    -1,   301,   230,   108,   231,    -1,
     302,   230,   108,   222,   111,   231,    -1,   303,   230,   108,
     222,   111,   231,    -1,   304,    -1,   108,    -1,   104,    -1,
       5,    -1,    31,    -1,     6,    -1,     7,    -1,     4,    -1,
      28,    -1,    29,    -1,    30,    -1,    32,    -1,    33,    -1,
      34,    -1,    19,    -1,    20,    -1,    21,    -1,    22,    -1,
      23,    -1,    24,    -1,    25,    -1,    26,    -1,    27,    -1,
      46,    -1,    47,    -1,    48,    -1,    49,    -1,    50,    -1,
      51,    -1,    52,    -1,    53,    -1,    54,    -1,    55,    -1,
      56,    -1,    57,    -1,    58,    -1,    59,    -1,    60,    -1,
      61,    -1,    62,    -1,    63,    -1,    73,    -1,    74,    -1,
      75,    -1,    76,    -1,    77,    -1,    78,    -1,    79,    -1,
      80,    -1,    81,    -1,    82,    -1,    83,    -1,    84,    -1,
      85,    -1,    92,    -1,    86,    -1,    87,    -1,    88,    -1,
      89,    -1,    90,    -1,    98,    -1,    99,    -1,   100,    -1,
     101,    -1,   102,    -1,   103,   244,   239,   306,   240,    -1,
     103,   244,   236,   108,   239,   306,   240,    -1,   103,   239,
     306,   240,    -1,   103,   244,   239,   240,    -1,   103,   244,
     236,   108,   239,   240,    -1,   103,   239,   240,    -1,   106,
     244,   239,   306,   240,    -1,   106,   244,   239,   240,    -1,
     307,    -1,   306,   307,    -1,   308,   310,   238,    -1,   295,
      -1,   309,   295,    -1,   291,    -1,    42,   291,    -1,   291,
      42,    -1,    42,    -1,   311,    -1,   310,   222,   311,    -1,
     244,    -1,   312,    -1,   244,   236,   244,    -1,   244,   219,
     277,   220,   236,   244,    -1,   244,   219,   277,   220,    -1,
     312,   219,   277,   220,    -1,   274,    -1,   239,   314,   240,
      -1,   313,    -1,   314,   222,   313,    -1,   314,   222,    -1,
     279,    -1,   318,    -1,   317,    -1,   342,   318,    -1,   342,
     317,    -1,   315,    -1,   323,    -1,   324,    -1,   327,    -1,
     333,    -1,   337,    -1,   239,   240,    -1,    -1,   239,   319,
     322,   240,    -1,   321,    -1,   317,    -1,   239,   240,    -1,
     239,   322,   240,    -1,   316,    -1,   322,   316,    -1,   238,
      -1,   276,   238,    -1,    13,   217,   276,   218,   325,    -1,
     316,    11,   316,    -1,   316,    -1,   276,    -1,   290,   244,
     237,   313,    -1,    16,   217,   276,   218,   328,    -1,   239,
     240,    -1,   239,   332,   240,    -1,    17,   276,   236,    -1,
      18,   236,    -1,   329,    -1,   330,   329,    -1,   330,   316,
      -1,   331,   316,    -1,   331,    -1,   332,   331,    -1,   105,
     217,   326,   218,   320,    -1,    10,   316,   105,   217,   276,
     218,   238,    -1,    12,   217,   334,   336,   218,   320,    -1,
     323,    -1,   315,    -1,   326,    -1,    -1,   335,   238,    -1,
     335,   238,   276,    -1,     9,   238,    -1,     8,   238,    -1,
      15,   238,    -1,    15,   276,   238,    -1,    14,   238,    -1,
     343,    -1,   344,    -1,   277,    -1,   114,    -1,   339,    -1,
     340,   222,   339,    -1,   219,   244,   220,    -1,   219,   299,
     220,    -1,   219,   244,   217,   340,   218,   220,    -1,   219,
     299,   217,   340,   218,   220,    -1,   342,   341,    -1,   341,
      -1,   280,   321,    -1,   342,   280,   321,    -1,   278,    -1,
     288,   238,    -1,   305,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   278,   278,   278,   290,   291,   292,   296,   304,   315,
     316,   320,   327,   334,   348,   355,   362,   369,   370,   376,
     380,   387,   393,   402,   406,   410,   411,   420,   421,   425,
     426,   430,   436,   448,   452,   458,   465,   475,   476,   480,
     481,   485,   491,   503,   514,   515,   521,   527,   533,   544,
     545,   546,   547,   551,   552,   558,   564,   573,   574,   580,
     589,   590,   596,   605,   606,   612,   618,   624,   633,   634,
     640,   649,   650,   659,   660,   669,   670,   679,   680,   689,
     690,   699,   700,   709,   710,   719,   720,   721,   722,   723,
     724,   725,   726,   727,   728,   729,   733,   737,   753,   757,
     765,   769,   776,   779,   787,   788,   792,   797,   805,   816,
     830,   840,   851,   862,   874,   890,   897,   904,   912,   917,
     922,   923,   934,   946,   951,   956,   962,   968,   974,   979,
     984,   989,   994,  1002,  1006,  1007,  1017,  1027,  1036,  1046,
    1056,  1070,  1077,  1086,  1095,  1104,  1113,  1123,  1132,  1146,
    1153,  1164,  1169,  1174,  1182,  1183,  1188,  1193,  1198,  1203,
    1211,  1212,  1213,  1218,  1223,  1229,  1237,  1242,  1247,  1253,
    1258,  1263,  1268,  1273,  1278,  1283,  1288,  1293,  1299,  1305,
    1310,  1318,  1325,  1326,  1330,  1336,  1341,  1347,  1356,  1362,
    1368,  1375,  1381,  1387,  1393,  1400,  1407,  1413,  1422,  1423,
    1424,  1425,  1426,  1427,  1428,  1429,  1430,  1431,  1432,  1433,
    1434,  1435,  1436,  1437,  1438,  1439,  1440,  1441,  1442,  1443,
    1444,  1445,  1446,  1447,  1448,  1449,  1450,  1451,  1452,  1453,
    1454,  1455,  1456,  1457,  1458,  1459,  1460,  1461,  1462,  1497,
    1498,  1499,  1500,  1501,  1502,  1503,  1504,  1505,  1506,  1507,
    1508,  1509,  1510,  1511,  1512,  1513,  1517,  1518,  1519,  1523,
    1527,  1531,  1538,  1545,  1551,  1558,  1565,  1574,  1580,  1588,
    1593,  1601,  1611,  1618,  1629,  1630,  1635,  1640,  1648,  1653,
    1661,  1668,  1673,  1681,  1691,  1697,  1706,  1707,  1716,  1721,
    1726,  1733,  1739,  1740,  1741,  1746,  1754,  1755,  1756,  1757,
    1758,  1759,  1763,  1770,  1769,  1783,  1784,  1788,  1794,  1803,
    1813,  1825,  1831,  1840,  1849,  1854,  1862,  1866,  1884,  1892,
    1897,  1905,  1910,  1918,  1926,  1934,  1942,  1950,  1958,  1966,
    1973,  1980,  1990,  1991,  1995,  1997,  2003,  2008,  2017,  2023,
    2029,  2035,  2041,  2050,  2051,  2055,  2061,  2070,  2074,  2082,
    2088,  2094,  2101,  2111,  2116,  2123,  2133,  2147,  2151,  2159
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CONST_TOK", "BOOL_TOK", "FLOAT_TOK",
  "INT_TOK", "UINT_TOK", "BREAK", "CONTINUE", "DO", "ELSE", "FOR", "IF",
  "DISCARD", "RETURN", "SWITCH", "CASE", "DEFAULT", "BVEC2", "BVEC3",
  "BVEC4", "IVEC2", "IVEC3", "IVEC4", "UVEC2", "UVEC3", "UVEC4", "VEC2",
  "VEC3", "VEC4", "HALF_TOK", "HVEC2", "HVEC3", "HVEC4", "IN_TOK",
  "OUT_TOK", "INOUT_TOK", "UNIFORM", "VARYING", "GLOBALLYCOHERENT",
  "SHARED", "CENTROID", "NOPERSPECTIVE", "NOINTERPOLATION", "LINEAR",
  "MAT2X2", "MAT2X3", "MAT2X4", "MAT3X2", "MAT3X3", "MAT3X4", "MAT4X2",
  "MAT4X3", "MAT4X4", "HMAT2X2", "HMAT2X3", "HMAT2X4", "HMAT3X2",
  "HMAT3X3", "HMAT3X4", "HMAT4X2", "HMAT4X3", "HMAT4X4", "FMAT2X2",
  "FMAT2X3", "FMAT2X4", "FMAT3X2", "FMAT3X3", "FMAT3X4", "FMAT4X2",
  "FMAT4X3", "FMAT4X4", "SAMPLERSTATE", "SAMPLERSTATE_CMP", "BUFFER",
  "TEXTURE1D", "TEXTURE1D_ARRAY", "TEXTURE2D", "TEXTURE2D_ARRAY",
  "TEXTURE2DMS", "TEXTURE2DMS_ARRAY", "TEXTURE3D", "TEXTURECUBE",
  "TEXTURECUBE_ARRAY", "RWBUFFER", "RWTEXTURE1D", "RWTEXTURE1D_ARRAY",
  "RWTEXTURE2D", "RWTEXTURE2D_ARRAY", "RWTEXTURE3D", "RWSTRUCTUREDBUFFER",
  "RWBYTEADDRESSBUFFER", "POINT_TOK", "LINE_TOK", "TRIANGLE_TOK",
  "LINEADJ_TOK", "TRIANGLEADJ_TOK", "POINTSTREAM", "LINESTREAM",
  "TRIANGLESTREAM", "INPUTPATCH", "OUTPUTPATCH", "STRUCT", "VOID_TOK",
  "WHILE", "CBUFFER", "IDENTIFIER", "TYPE_IDENTIFIER", "NEW_IDENTIFIER",
  "FLOATCONSTANT", "INTCONSTANT", "UINTCONSTANT", "BOOLCONSTANT",
  "STRINGCONSTANT", "FIELD_SELECTION", "LEFT_OP", "RIGHT_OP", "INC_OP",
  "DEC_OP", "LE_OP", "GE_OP", "EQ_OP", "NE_OP", "AND_OP", "OR_OP",
  "MUL_ASSIGN", "DIV_ASSIGN", "ADD_ASSIGN", "MOD_ASSIGN", "LEFT_ASSIGN",
  "RIGHT_ASSIGN", "AND_ASSIGN", "XOR_ASSIGN", "OR_ASSIGN", "SUB_ASSIGN",
  "INVARIANT", "VERSION_TOK", "EXTENSION", "LINE", "COLON", "EOL",
  "INTERFACE", "OUTPUT", "PRAGMA_DEBUG_ON", "PRAGMA_DEBUG_OFF",
  "PRAGMA_OPTIMIZE_ON", "PRAGMA_OPTIMIZE_OFF", "PRAGMA_INVARIANT_ALL",
  "ASM", "CLASS", "UNION", "ENUM", "TYPEDEF", "TEMPLATE", "THIS",
  "PACKED_TOK", "GOTO", "INLINE_TOK", "NOINLINE", "VOLATILE", "PUBLIC_TOK",
  "STATIC", "EXTERN", "EXTERNAL", "LONG_TOK", "SHORT_TOK", "DOUBLE_TOK",
  "HALF", "FIXED_TOK", "UNSIGNED", "DVEC2", "DVEC3", "DVEC4", "FVEC2",
  "FVEC3", "FVEC4", "SAMPLER2DRECT", "SAMPLER3DRECT",
  "SAMPLER2DRECTSHADOW", "SIZEOF", "CAST", "NAMESPACE", "USING",
  "ERROR_TOK", "COMMON", "PARTITION", "ACTIVE", "SAMPLERBUFFER", "FILTER",
  "IMAGE1D", "IMAGE2D", "IMAGE3D", "IMAGECUBE", "IMAGE1DARRAY",
  "IMAGE2DARRAY", "IIMAGE1D", "IIMAGE2D", "IIMAGE3D", "IIMAGECUBE",
  "IIMAGE1DARRAY", "IIMAGE2DARRAY", "UIMAGE1D", "UIMAGE2D", "UIMAGE3D",
  "UIMAGECUBE", "UIMAGE1DARRAY", "UIMAGE2DARRAY", "IMAGE1DSHADOW",
  "IMAGE2DSHADOW", "IMAGEBUFFER", "IIMAGEBUFFER", "UIMAGEBUFFER",
  "IMAGE1DARRAYSHADOW", "IMAGE2DARRAYSHADOW", "ROW_MAJOR", "COLUMN_MAJOR",
  "'('", "')'", "'['", "']'", "'.'", "','", "'+'", "'-'", "'!'", "'~'",
  "'*'", "'/'", "'%'", "'<'", "'>'", "'&'", "'^'", "'|'", "'?'", "':'",
  "'='", "';'", "'{'", "'}'", "$accept", "translation_unit", "$@1",
  "any_identifier", "external_declaration_list", "variable_identifier",
  "primary_expression", "postfix_expression", "integer_expression",
  "function_call", "function_call_or_method", "function_call_generic",
  "function_call_header_no_parameters",
  "function_call_header_with_parameters", "function_call_header",
  "function_identifier", "method_call_generic",
  "method_call_header_no_parameters", "method_call_header_with_parameters",
  "method_call_header", "unary_expression", "unary_operator",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_or_expression",
  "conditional_expression", "assignment_expression", "assignment_operator",
  "expression", "constant_expression", "function_decl", "declaration",
  "function_prototype", "function_declarator",
  "function_header_with_parameters", "function_header",
  "parameter_declarator", "parameter_declaration", "parameter_qualifier",
  "parameter_type_specifier", "init_declarator_list", "single_declaration",
  "fully_specified_type", "interpolation_qualifier",
  "parameter_type_qualifier", "type_qualifier", "storage_qualifier",
  "type_specifier", "type_specifier_no_prec", "type_specifier_array",
  "type_specifier_nonarray", "basic_type_specifier_nonarray",
  "texture_type_specifier_nonarray",
  "outputstream_type_specifier_nonarray",
  "inputpatch_type_specifier_nonarray",
  "outputpatch_type_specifier_nonarray", "struct_specifier",
  "cbuffer_declaration", "struct_declaration_list", "struct_declaration",
  "struct_type_specifier", "struct_type_qualifier",
  "struct_declarator_list", "struct_declarator", "array_identifier",
  "initializer", "initializer_list", "declaration_statement", "statement",
  "simple_statement", "compound_statement", "$@2",
  "statement_no_new_scope", "compound_statement_no_new_scope",
  "statement_list", "expression_statement", "selection_statement",
  "selection_rest_statement", "condition", "switch_statement",
  "switch_body", "case_label", "case_label_list", "case_statement",
  "case_statement_list", "iteration_statement", "for_init_statement",
  "conditionopt", "for_rest_statement", "jump_statement",
  "external_declaration", "attribute_arg", "attribute_arg_list",
  "attribute", "attribute_list", "function_definition",
  "global_declaration", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
     445,   446,   447,   448,   449,   450,   451,   452,   453,   454,
     455,   456,   457,   458,   459,   460,   461,   462,   463,   464,
     465,   466,   467,   468,   469,   470,   471,    40,    41,    91,
      93,    46,    44,    43,    45,    33,   126,    42,    47,    37,
      60,    62,    38,    94,   124,    63,    58,    61,    59,   123,
     125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   241,   243,   242,   244,   244,   244,   245,   245,   246,
     246,   247,   247,   247,   247,   247,   247,   248,   248,   248,
     248,   248,   248,   249,   250,   251,   251,   252,   252,   253,
     253,   254,   254,   255,   256,   256,   256,   257,   257,   258,
     258,   259,   259,   260,   261,   261,   261,   261,   261,   262,
     262,   262,   262,   263,   263,   263,   263,   264,   264,   264,
     265,   265,   265,   266,   266,   266,   266,   266,   267,   267,
     267,   268,   268,   269,   269,   270,   270,   271,   271,   272,
     272,   273,   273,   274,   274,   275,   275,   275,   275,   275,
     275,   275,   275,   275,   275,   275,   276,   276,   277,   278,
     279,   279,   280,   280,   281,   281,   282,   282,   283,   283,
     284,   284,   284,   284,   284,   285,   285,   285,   285,   285,
     285,   285,   285,   286,   286,   286,   286,   286,   286,   286,
     286,   286,   286,   287,   288,   288,   288,   288,   288,   288,
     288,   289,   289,   289,   289,   289,   289,   289,   289,   290,
     290,   291,   291,   291,   292,   292,   292,   292,   292,   292,
     293,   293,   293,   293,   293,   293,   294,   294,   294,   294,
     294,   294,   294,   294,   294,   294,   294,   294,   294,   294,
     294,   295,   296,   296,   297,   297,   297,   297,   298,   298,
     298,   298,   298,   298,   298,   298,   298,   298,   299,   299,
     299,   299,   299,   299,   299,   299,   299,   299,   299,   299,
     299,   299,   299,   299,   299,   299,   299,   299,   299,   299,
     299,   299,   299,   299,   299,   299,   299,   299,   299,   299,
     299,   299,   299,   299,   299,   299,   299,   299,   299,   300,
     300,   300,   300,   300,   300,   300,   300,   300,   300,   300,
     300,   300,   300,   300,   300,   300,   301,   301,   301,   302,
     303,   304,   304,   304,   304,   304,   304,   305,   305,   306,
     306,   307,   308,   308,   309,   309,   309,   309,   310,   310,
     311,   311,   311,   311,   312,   312,   313,   313,   314,   314,
     314,   315,   316,   316,   316,   316,   317,   317,   317,   317,
     317,   317,   318,   319,   318,   320,   320,   321,   321,   322,
     322,   323,   323,   324,   325,   325,   326,   326,   327,   328,
     328,   329,   329,   330,   330,   331,   331,   332,   332,   333,
     333,   333,   334,   334,   335,   335,   336,   336,   337,   337,
     337,   337,   337,   338,   338,   339,   339,   340,   340,   341,
     341,   341,   341,   342,   342,   343,   343,   344,   344,   344
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     1,     1,     2,     1,
       1,     1,     1,     1,     1,     1,     3,     1,     4,     1,
       3,     2,     2,     1,     1,     1,     3,     2,     2,     2,
       1,     2,     3,     2,     1,     1,     1,     2,     2,     2,
       1,     2,     3,     2,     1,     2,     2,     2,     4,     1,
       1,     1,     1,     1,     3,     3,     3,     1,     3,     3,
       1,     3,     3,     1,     3,     3,     3,     3,     1,     3,
       3,     1,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     5,     1,     3,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     3,     1,     2,
       1,     2,     2,     4,     1,     1,     2,     3,     3,     4,
       2,     4,     4,     2,     7,     3,     3,     4,     2,     2,
       1,     3,     2,     1,     1,     1,     2,     2,     1,     1,
       1,     1,     1,     1,     1,     3,     5,     3,     7,     5,
       5,     1,     2,     4,     2,     6,     4,     4,     2,     1,
       2,     1,     1,     1,     1,     2,     2,     1,     1,     1,
       1,     1,     2,     2,     3,     1,     1,     1,     2,     1,
       1,     2,     2,     1,     1,     1,     1,     2,     2,     1,
       1,     1,     1,     1,     3,     3,     4,     4,     1,     1,
       6,     4,     4,     4,     6,     6,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     5,     7,     4,     4,     6,     3,     5,     4,     1,
       2,     3,     1,     2,     1,     2,     2,     1,     1,     3,
       1,     1,     3,     6,     4,     4,     1,     3,     1,     3,
       2,     1,     1,     1,     2,     2,     1,     1,     1,     1,
       1,     1,     2,     0,     4,     1,     1,     2,     3,     1,
       2,     1,     2,     5,     3,     1,     1,     4,     5,     2,
       3,     3,     2,     1,     2,     2,     2,     1,     2,     5,
       7,     6,     1,     1,     1,     0,     2,     3,     2,     2,
       2,     3,     2,     1,     1,     1,     1,     1,     3,     3,
       3,     6,     6,     2,     1,     2,     3,     1,     2,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,   166,   203,   199,   201,   202,   210,
     211,   212,   213,   214,   215,   216,   217,   218,   204,   205,
     206,   200,   207,   208,   209,   169,   170,   173,   167,   179,
     180,     0,   153,   152,   151,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245,   246,   247,   248,   249,   251,   252,   253,   254,
     255,     0,   250,   256,   257,   258,   259,   260,     0,   198,
       0,   197,   165,     0,   176,   174,   175,     0,     3,   357,
       0,     0,   105,   104,     0,   134,   141,   161,     0,   160,
     149,   181,   183,   182,   188,   189,     0,     0,     0,   196,
     359,     7,   354,     0,   343,   344,   177,   171,   172,   168,
       0,     4,     5,     6,     0,     0,     0,     9,    10,   148,
       0,   163,   165,     0,   178,     0,     0,     8,    99,     0,
     355,   102,     0,   158,   123,   124,   125,   159,   157,   128,
     129,   130,   131,   132,   120,   106,     0,   154,     0,     0,
       0,   358,     4,     6,   142,     0,   144,   162,   150,     0,
       0,     0,     0,     0,     0,     0,     0,   353,     0,   277,
     266,   274,   272,     0,   269,     0,     0,     0,     0,     0,
     164,     0,     0,   349,     0,   350,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    14,    12,    13,    15,    36,
       0,     0,     0,    49,    50,    51,    52,   311,   303,   307,
      11,    17,    44,    19,    24,    25,     0,     0,    30,     0,
      53,     0,    57,    60,    63,    68,    71,    73,    75,    77,
      79,    81,    83,    96,     0,   100,   291,     0,     0,   149,
     296,   309,   293,   292,     0,   297,   298,   299,   300,   301,
       0,     0,   107,   126,   127,   155,   118,   122,     0,   133,
     156,   119,     0,   110,   113,   135,   137,     0,     0,   108,
       0,     0,   185,    53,    98,     0,    34,   184,     0,     0,
       0,     0,     0,   356,   192,   275,   276,   263,   270,   280,
       0,   278,   281,   273,     0,   264,     0,   268,     0,   109,
     346,   345,   347,     0,     0,   339,   338,     0,     0,     0,
     342,   340,     0,     0,     0,    45,    46,     0,   182,   302,
       0,    21,    22,     0,     0,    28,    27,     0,   198,    31,
      33,    86,    87,    89,    88,    91,    92,    93,    94,    95,
      90,    85,     0,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   312,   101,   308,   310,   295,   294,
     103,   116,   115,   121,     0,     0,     0,     0,     0,     0,
       0,   143,     0,     0,   286,   147,     0,   146,   187,   186,
       0,   191,   193,     0,     0,     0,     0,     0,   271,     0,
     261,   267,     0,     0,     0,     0,   333,   332,   335,     0,
     341,     0,   316,     0,     0,    16,     0,     0,     0,    23,
      20,     0,    26,     0,     0,    40,    32,    84,    54,    55,
      56,    58,    59,    61,    62,    66,    67,    64,    65,    69,
      70,    72,    74,    76,    78,    80,     0,    97,   117,     0,
     112,   111,   136,   140,   139,     0,   284,   288,     0,   285,
       0,     0,     0,     0,   282,   279,   265,     0,   351,   348,
     352,     0,   334,     0,     0,     0,     0,     0,     0,    48,
     304,    18,    43,    38,    37,     0,   198,    41,     0,   284,
       0,   145,   290,   287,   190,   194,   195,   284,   262,     0,
     336,     0,   315,   313,     0,   318,     0,   306,   329,   305,
      42,    82,     0,   138,   289,     0,     0,   337,   331,     0,
       0,     0,   319,   323,     0,   327,     0,   317,   114,   283,
     330,   314,     0,   322,   325,   324,   326,   320,   328,   321
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,   273,    88,   220,   221,   222,   428,   223,
     224,   225,   226,   227,   228,   229,   432,   433,   434,   435,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   243,   352,   244,   311,   245,   246,   247,
      91,    92,    93,   154,   155,   156,   267,   248,    95,    96,
      97,   158,    98,    99,   286,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   183,   184,   185,   186,   300,
     301,   274,   395,   468,   250,   251,   252,   253,   330,   518,
     519,   254,   255,   256,   513,   424,   257,   515,   533,   534,
     535,   536,   258,   418,   483,   484,   259,   111,   312,   313,
     112,   260,   114,   115
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -250
static const yytype_int16 yypact[] =
{
    -250,   127,  5178,  -250,   -29,  -250,  -250,  -250,  -250,  -250,
    -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,
    -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,
    -250,    81,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,
    -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,
    -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,
    -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,
    -250,   -77,  -250,  -250,  -250,  -250,  -250,  -250,   -72,  -250,
     141,  -250,   165,  5472,   133,  -250,  -250,   731,  5178,  -250,
    -193,   -71,   -58,  5616,  -143,  -250,   183,   305,  5917,  -250,
    -250,  -250,   -46,   -43,  -250,   -51,   -31,    -3,    -1,  -250,
    -250,  -250,  -250,  5325,  -250,  -250,  -250,  -250,  -250,  -250,
      94,  -250,  -250,  -250,  2019,  -146,    -4,  -250,  -250,  -250,
     305,  -250,   180,    37,  -250,   -82,   -59,  -250,  -250,   608,
    -250,   -54,  5616,  -250,   201,   206,  -250,  -250,   251,  -250,
    -250,  -250,  -250,  -250,  -250,  -250,  5722,   217,  5812,   141,
     141,  -250,    26,    52,  -176,    54,  -171,  -250,  -250,  3703,
    3884,   964,   168,   173,   175,    47,    37,  -250,    56,   251,
    -250,   255,  -250,  2109,  -250,   141,  5917,   191,  2218,  2308,
    -250,    88,  4065,  -250,  4065,  -250,    73,    77,  1559,    95,
     100,    80,  3265,   102,   103,  -250,  -250,  -250,  -250,  -250,
    4608,  4608,  4608,  -250,  -250,  -250,  -250,  -250,    83,  -250,
     104,  -250,   -41,  -250,  -250,  -250,   106,   -70,  4789,   108,
     239,  4608,    74,  -141,   137,   -89,    -8,    98,    93,    97,
     211,   -84,  -250,  -250,  -138,  -250,  -250,   110,  -137,   132,
    -250,  -250,  -250,  -250,   846,  -250,  -250,  -250,  -250,  -250,
    1559,   141,  -250,  -250,  -250,  -250,  -250,  -250,  5917,   141,
    -250,  -250,  5722,  -172,   134,  -168,  -164,  4246,  3042,  -250,
    4608,  3042,  -250,  -250,  -250,   135,  -250,  -250,   138,  -101,
     121,   139,   142,  -250,  -250,  -250,  -250,  -250,  -250,  -161,
    -133,  -250,   134,  -250,   115,  -250,  2417,  -250,  2507,  -250,
    -250,  -250,  -250,   -53,   -48,  -250,  -250,   252,  2819,  4608,
    -250,  -250,  -125,  4608,  3489,  -250,  -250,   -33,    38,  -250,
    1559,  -250,  -250,  4608,   183,  -250,  -250,  4608,   157,  -250,
    -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,
    -250,  -250,  4608,  -250,  4608,  4608,  4608,  4608,  4608,  4608,
    4608,  4608,  4608,  4608,  4608,  4608,  4608,  4608,  4608,  4608,
    4608,  4608,  4608,  4608,  -250,  -250,  -250,  -250,  -250,  -250,
    -250,  -250,  -250,  -250,  5917,  4608,   141,  4608,  4427,  3042,
    3042,   122,   140,  3042,  -250,  -250,   156,  -250,  -250,  -250,
     266,  -250,  -250,   267,   268,  4608,   141,   141,  -250,  2616,
    -250,  -250,   163,  4065,   164,   169,  -250,  -250,  3489,   -28,
    -250,   -27,   166,   141,   171,  -250,  4608,  1084,   170,   166,
    -250,   174,  -250,   176,   -25,  4970,  -250,  -250,  -250,  -250,
    -250,    74,    74,  -141,  -141,   137,   137,   137,   137,   -89,
     -89,    -8,    98,    93,    97,   211,  -183,  -250,  -250,   172,
    -250,  -250,   161,  -250,  -250,  3042,  -250,  -250,  -152,  -250,
     162,   177,   178,   179,  -250,  -250,  -250,  2706,  -250,  -250,
    -250,  4608,  -250,   167,   182,  1559,   181,   184,  1796,  -250,
    -250,  -250,  -250,  -250,  -250,  4608,   188,  -250,  4608,   186,
    3042,  -250,  3042,  -250,  -250,  -250,  -250,   187,  -250,    14,
    4608,  1796,   390,  -250,     0,  -250,  3042,  -250,  -250,  -250,
    -250,  -250,   141,  -250,  -250,   141,   189,   166,  -250,  1559,
    4608,   190,  -250,  -250,  1322,  1559,     2,  -250,  -250,  -250,
    -250,  -250,  -142,  -250,  -250,  -250,  -250,  -250,  1559,  -250
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -250,  -250,  -250,   -73,  -250,   -74,  -250,  -250,  -250,  -250,
    -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,  -250,
     193,  -250,   -94,   -81,  -150,   -56,    35,    42,    43,    41,
      44,  -250,  -120,  -218,  -250,  -174,  -154,    10,  -250,    19,
    -250,  -250,  -250,  -129,   271,   256,   145,    22,  -250,   -79,
     -80,  -147,  -250,   -26,    -2,  -250,  -250,   207,   -47,  -250,
    -250,  -250,  -250,  -250,  -250,  -163,  -177,  -250,  -250,  -250,
      11,   -93,  -227,  -250,   111,  -197,  -249,   185,  -250,   -78,
     -57,   101,   112,  -250,  -250,    16,  -250,  -250,  -106,  -250,
    -104,  -250,  -250,  -250,  -250,  -250,  -250,   347,    23,   243,
     -83,    40,  -250,  -250
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -40
static const yytype_int16 yytable[] =
{
     100,   317,   130,   166,   133,   125,   298,   126,   129,   268,
     339,   378,    89,   157,   135,   285,   288,   530,   531,   530,
     531,    90,   165,   164,    94,   306,   308,   266,   322,   271,
     177,   361,   362,   140,   176,   121,   122,   123,   327,   373,
     136,   371,   113,   277,   181,   138,   139,   385,   280,   284,
     284,   388,   130,   498,   397,   280,   131,   377,   405,   191,
     394,   278,   157,   394,   386,   387,   281,   276,   265,   389,
     502,   167,   284,   390,   284,   406,   157,   331,   332,   160,
     373,   100,   357,   358,   373,   160,   100,   275,   503,   407,
     187,   159,   302,   188,   549,   161,   168,   373,    89,   295,
     374,   375,   165,   181,   190,   408,   131,    90,   181,   181,
      94,   100,   299,   420,   365,   366,   117,   118,   293,   436,
     119,   400,   182,   392,   289,   384,   396,     3,   113,   298,
     401,   298,   175,   116,   437,   192,   134,   249,   193,   381,
     159,   363,   364,   382,   127,   419,   128,   141,   336,   421,
     422,   372,   337,   120,   269,   457,   159,   284,   194,   429,
     284,   195,   463,   464,   142,   412,   467,   124,     4,   413,
     414,   394,   394,   169,   413,   394,   170,   177,   333,   171,
     334,   182,   261,     4,   303,   425,   182,   182,   380,   373,
     485,   486,   157,   494,   373,   373,   249,   495,   456,   172,
      25,    26,   178,    27,    28,    29,    30,    31,    32,    33,
      34,   445,   446,   447,   448,    25,    26,   497,    27,    28,
      29,    30,    31,    32,    33,    34,   181,   173,   181,   174,
     377,   459,   526,   461,   392,   189,   373,   263,   501,   517,
     532,   264,   547,    -9,   422,   423,   477,   394,   121,   122,
     123,   473,   249,   359,   360,   458,   426,   170,   249,   270,
     431,   430,   517,   441,   442,   284,   159,   284,   284,   -10,
     269,   279,   127,   523,   128,   524,   290,   520,   443,   444,
     521,   291,   394,   292,   394,   284,   139,   294,   512,   537,
     162,   122,   163,   284,    32,    33,    34,   296,   394,   304,
     298,   354,   355,   356,   182,   309,   182,   509,     4,   449,
     450,   315,   318,   460,   302,   316,   249,   319,   320,   323,
     324,   -35,   249,   329,   335,   340,   368,    84,   249,   181,
     367,   369,   541,   474,   299,   370,   527,   544,   546,   423,
      25,    26,    84,    27,    28,    29,    30,    31,   138,   -34,
     487,   546,   402,   280,   409,   398,   542,   415,   399,   465,
     466,   403,   283,   283,   404,   341,   342,   343,   344,   345,
     346,   347,   348,   349,   350,   -29,   469,   470,   471,   472,
      85,    86,   159,   478,   480,   283,   481,   283,   373,   488,
     491,   492,   499,   504,   493,    85,    86,   181,   500,   507,
     511,   529,   451,   325,   326,   510,   -39,   182,   505,   506,
     452,   454,   453,   262,   272,   455,   249,   383,   475,   328,
     514,   516,   522,   525,   353,   249,   543,   540,   545,   416,
     417,   427,   548,   528,   482,   137,   479,   314,     0,     0,
       0,     0,     0,     0,     0,   379,     0,     0,     0,   538,
       0,     0,   539,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    84,     0,     0,
     283,     0,     0,   283,     0,   182,   351,     0,     0,     0,
       0,     0,     0,   249,     0,     0,   249,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   249,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      85,    86,     0,     0,     0,     0,     0,   249,     0,     0,
       0,     0,   249,   249,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   249,   438,   439,   440,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   283,     0,
     283,   283,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   283,     0,
       0,     0,     0,     0,     0,     0,   283,     0,     0,     0,
       0,     4,     5,     6,     7,     8,   196,   197,   198,   489,
     199,   200,   201,   202,   203,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,     0,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,     0,     0,     0,     0,     0,    73,    74,    75,    76,
      77,    78,    79,   204,     0,   127,    81,   128,   205,   206,
     207,   208,     0,   209,     0,     0,   210,   211,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     8,     0,
       0,     0,     0,     0,    82,     0,     0,     0,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    83,     0,     0,     0,
      84,     0,     0,     0,     0,     0,     0,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    53,    54,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    85,    86,   212,     0,    87,     0,     0,
       0,   213,   214,   215,   216,    79,     0,     0,   121,   122,
     123,     0,     0,     0,     0,     0,   217,   218,   219,     4,
       5,     6,     7,     8,   196,   197,   198,     0,   199,   200,
     201,   202,   203,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,     0,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,     0,
       0,     0,     0,     0,    73,    74,    75,    76,    77,    78,
      79,   204,     0,   127,    81,   128,   205,   206,   207,   208,
       0,   209,     0,     0,   210,   211,     0,     0,     5,     6,
       7,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    82,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
       0,     0,     0,     0,    83,     0,     0,     0,    84,     0,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    85,    86,   212,     0,    87,     0,     0,    79,   213,
     214,   215,   216,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   217,   218,   376,     4,     5,     6,
       7,     8,   196,   197,   198,     0,   199,   200,   201,   202,
     203,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,     0,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,     0,     0,     0,
       0,     0,    73,    74,    75,    76,    77,    78,    79,   204,
       0,   127,    81,   128,   205,   206,   207,   208,     0,   209,
       0,     0,   210,   211,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      82,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    83,     0,     0,     0,    84,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    85,
      86,   212,     0,    87,     0,     0,     0,   213,   214,   215,
     216,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   217,   218,   490,     4,     5,     6,     7,     8,
     196,   197,   198,     0,   199,   200,   201,   202,   203,   530,
     531,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,     0,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,     0,     0,     0,     0,     0,
      73,    74,    75,    76,    77,    78,    79,   204,     0,   127,
      81,   128,   205,   206,   207,   208,     0,   209,     0,     0,
     210,   211,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    82,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      83,     0,     0,     0,    84,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    85,    86,   212,
       0,    87,     0,     0,     0,   213,   214,   215,   216,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     217,   218,     4,     5,     6,     7,     8,   196,   197,   198,
       0,   199,   200,   201,   202,   203,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,     0,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,     0,     0,     0,     0,     0,    73,    74,    75,
      76,    77,    78,    79,   204,     0,   127,    81,   128,   205,
     206,   207,   208,     0,   209,     0,     0,   210,   211,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    82,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    83,     0,     0,
       0,    84,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    85,    86,   212,     0,    87,     0,
       0,     0,   213,   214,   215,   216,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   217,   218,     4,
       5,     6,     7,     8,   196,   197,   198,     0,   199,   200,
     201,   202,   203,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,     0,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,     0,
       0,     0,     0,     0,    73,    74,    75,    76,    77,    78,
      79,   204,     0,   127,    81,   128,   205,   206,   207,   208,
       0,   209,     0,     0,   210,   211,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    82,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    83,     0,     0,     0,    84,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    85,    86,   212,     0,     0,     0,     0,     0,   213,
     214,   215,   216,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,     0,   217,   139,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,     0,     0,     0,     0,
       0,   179,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,     0,     5,     6,     7,     8,    73,    74,    75,
      76,    77,    78,    79,     0,     0,     0,    81,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,     0,     0,     0,     0,
       0,   179,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,     0,     0,     0,     0,     0,    73,    74,    75,
      76,    77,    78,    79,     0,     0,     0,    81,     0,     0,
       0,     0,     5,     6,     7,     8,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,     0,     0,     0,     0,     0,   180,
     179,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,     0,     5,     6,     7,     8,    73,    74,    75,    76,
      77,    78,    79,     0,     0,     0,    81,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,     0,     0,     0,     0,     0,   297,
     179,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,     0,     0,     0,     0,     0,    73,    74,    75,    76,
      77,    78,    79,     0,     0,     0,    81,     0,     0,     0,
       0,     5,     6,     7,     8,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,     0,     0,     0,     0,   305,   179,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
       0,     5,     6,     7,     8,    73,    74,    75,    76,    77,
      78,    79,     0,     0,     0,    81,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,     0,     0,     0,     0,   307,   179,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
       0,     0,     0,     0,     0,    73,    74,    75,    76,    77,
      78,    79,     0,     0,     0,    81,     0,     0,     0,     0,
       5,     6,     7,     8,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,     0,     0,     0,     0,     0,   410,   179,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,     0,
       5,     6,     7,     8,    73,    74,    75,    76,    77,    78,
      79,     0,     0,     0,    81,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,     0,     0,     0,     0,     0,   411,   179,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,     0,
       0,     0,     0,     0,    73,    74,    75,    76,    77,    78,
      79,     0,     0,     0,    81,     0,     0,     0,     0,     0,
       0,     0,     4,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,   476,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,     0,     0,     0,     0,     0,    73,    74,    75,
      76,    77,    78,    79,     0,     0,   127,    81,   128,   205,
     206,   207,   208,     0,   209,     0,     0,   210,   211,     0,
       0,     0,     0,     0,     0,     0,   508,     0,     0,     0,
       0,     0,     0,     0,     0,    82,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    83,     0,     0,
       0,    84,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    85,    86,   212,     0,     0,     0,
       0,     0,   213,   214,   215,   216,     5,     6,     7,     8,
       0,     0,     0,     0,     0,     0,     0,   217,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,     0,     0,     0,     0,     0,
      73,    74,    75,    76,    77,    78,    79,     0,     0,   127,
      81,   128,   205,   206,   207,   208,     0,   209,     0,     0,
     210,   211,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   212,
       0,     0,     0,     0,     0,   213,   214,   215,   216,     5,
       6,     7,     8,     0,     0,     0,     0,     0,     0,     0,
       0,   393,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,     0,     0,
       0,     0,     0,    73,    74,    75,    76,    77,    78,    79,
       0,     0,   127,    81,   128,   205,   206,   207,   208,     0,
     209,     0,     0,   210,   211,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   212,     0,     0,     0,     0,     0,   213,   214,
     215,   216,     4,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,   321,     0,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,     0,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,     0,     0,     0,     0,     0,    73,    74,    75,
      76,    77,    78,    79,     0,     0,   127,    81,   128,   205,
     206,   207,   208,     0,   209,     0,     0,   210,   211,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   132,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    84,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    85,    86,   212,     5,     6,     7,
       8,     0,   213,   214,   215,   216,     0,     0,     0,     0,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,     0,     0,     0,     0,
       0,    73,    74,    75,    76,    77,    78,    79,     0,     0,
     127,    81,   128,   205,   206,   207,   208,     0,   209,     0,
       0,   210,   211,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     8,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
     212,     0,     0,   282,     0,     0,   213,   214,   215,   216,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,     0,     0,     0,
       0,     0,    73,    74,    75,    76,    77,    78,    79,     0,
       0,   127,    81,   128,   205,   206,   207,   208,     0,   209,
       0,     0,   210,   211,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     8,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
       0,   212,     0,     0,   287,     0,     0,   213,   214,   215,
     216,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,     0,     0,
       0,     0,     0,    73,    74,    75,    76,    77,    78,    79,
       0,     0,   127,    81,   128,   205,   206,   207,   208,   310,
     209,     0,     0,   210,   211,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     8,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,   212,     0,     0,     0,     0,     0,   213,   214,
     215,   216,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,     0,
       0,     0,     0,     0,    73,    74,    75,    76,    77,    78,
      79,     0,     0,   127,    81,   128,   205,   206,   207,   208,
       0,   209,     0,     0,   210,   211,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     8,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,   212,     0,     0,   391,     0,     0,   213,
     214,   215,   216,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
       0,     0,     0,     0,     0,    73,    74,    75,    76,    77,
      78,    79,     0,     0,   127,    81,   128,   205,   206,   207,
     208,     0,   209,     0,     0,   210,   211,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     8,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,   212,     0,     0,   462,     0,     0,
     213,   214,   215,   216,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,     0,     0,     0,     0,     0,    73,    74,    75,    76,
      77,    78,    79,     0,     0,   127,    81,   128,   205,   206,
     207,   208,     0,   209,     0,     0,   210,   211,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,   212,     0,     0,     0,     0,
       0,   213,   214,   215,   216,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,     0,     0,     0,     0,     0,    73,    74,    75,
      76,    77,    78,   338,     0,     0,   127,    81,   128,   205,
     206,   207,   208,     0,   209,     0,     0,   210,   211,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     8,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,   212,     0,     0,     0,
       0,     0,   213,   214,   215,   216,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,     0,     0,     0,     0,     0,    73,    74,
      75,    76,    77,    78,   496,     0,     0,   127,    81,   128,
     205,   206,   207,   208,     0,   209,     0,     0,   210,   211,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     4,     5,     6,     7,     8,     0,   212,     0,     0,
       0,     0,     0,   213,   214,   215,   216,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,     0,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,     0,     0,     0,     0,     0,    73,    74,    75,    76,
      77,    78,    79,     0,    80,     0,    81,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    82,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     4,     5,
       6,     7,     8,     0,     0,     0,    83,     0,     0,     0,
      84,     0,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,     0,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,     0,
       0,     0,     0,    85,    86,     0,     0,    87,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,     0,     0,
       0,     0,     0,    73,    74,    75,    76,    77,    78,    79,
       0,     0,     0,    81,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   132,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     4,     5,     6,     7,     8,
       0,     0,     0,    83,     0,     0,     0,    84,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,     0,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
      85,    86,     0,     0,    87,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,     0,     0,     0,     0,     0,
      73,    74,    75,    76,    77,    78,    79,     0,     0,     0,
      81,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   132,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   143,
       5,     6,     7,     8,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    84,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,   144,   145,   146,   147,     0,     0,     0,   148,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,    85,    86,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,   149,
     150,   151,   152,   153,    73,    74,    75,    76,    77,    78,
      79,     0,     0,     0,    81,   143,     5,     6,     7,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,     0,     0,
     147,     0,     0,     0,   148,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,     0,     5,     6,     7,     8,
      73,    74,    75,    76,    77,    78,    79,     0,     0,     0,
      81,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,   144,   145,   146,
       0,     0,     0,     0,     0,     0,     0,     0,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,   149,   150,   151,   152,   153,
      73,    74,    75,    76,    77,    78,    79,     0,     0,     0,
      81,     5,     6,     7,     8,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
       0,     0,     0,     0,     0,    73,    74,    75,    76,    77,
      78,    79,     0,     0,     0,    81
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-250))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
       2,   198,    82,    96,    83,    78,   183,    80,    82,   156,
     228,   260,     2,    93,    87,   169,   170,    17,    18,    17,
      18,     2,    96,    96,     2,   188,   189,   156,   202,   158,
     113,   120,   121,    90,   113,   107,   108,   109,   212,   222,
      87,   125,     2,   219,   124,   238,   239,   219,   219,   169,
     170,   219,   132,   236,   281,   219,    82,   254,   219,   133,
     278,   237,   142,   281,   236,   237,   237,   160,   148,   237,
     222,    97,   192,   237,   194,   236,   156,   118,   119,   222,
     222,    83,   223,   224,   222,   222,    88,   160,   240,   222,
     236,    93,   185,   239,   236,   238,    98,   222,    88,   179,
     238,   238,   176,   183,   130,   238,   132,    88,   188,   189,
      88,   113,   185,   238,   122,   123,    35,    36,   175,   337,
      39,   222,   124,   277,   171,   272,   280,     0,    88,   306,
     231,   308,   113,   162,   352,   217,     3,   139,   220,   268,
     142,   230,   231,   272,   107,   319,   109,   218,   218,   323,
     324,   235,   222,   230,   156,   373,   158,   277,   217,   333,
     280,   220,   389,   390,   222,   218,   393,   239,     3,   222,
     218,   389,   390,   219,   222,   393,   219,   260,   219,   230,
     221,   183,   236,     3,   186,   218,   188,   189,   261,   222,
     218,   218,   272,   218,   222,   222,   198,   222,   372,   230,
      35,    36,   108,    38,    39,    40,    41,    42,    43,    44,
      45,   361,   362,   363,   364,    35,    36,   435,    38,    39,
      40,    41,    42,    43,    44,    45,   306,   230,   308,   230,
     427,   385,   218,   387,   388,   239,   222,    36,   465,   488,
     240,    35,   240,   217,   418,   324,   409,   465,   107,   108,
     109,   405,   254,   116,   117,   384,   218,   219,   260,    42,
     334,   334,   511,   357,   358,   385,   268,   387,   388,   217,
     272,   217,   107,   500,   109,   502,   108,   495,   359,   360,
     498,   108,   500,   108,   502,   405,   239,   231,   485,   516,
     107,   108,   109,   413,    43,    44,    45,    42,   516,   108,
     477,   227,   228,   229,   306,   217,   308,   481,     3,   365,
     366,   238,   217,   386,   407,   238,   318,   217,   238,   217,
     217,   217,   324,   240,   218,   217,   233,   162,   330,   409,
     232,   234,   529,   406,   407,   124,   510,   534,   535,   418,
      35,    36,   162,    38,    39,    40,    41,    42,   238,   217,
     423,   548,   231,   219,   239,   220,   530,   105,   220,   237,
     220,   222,   169,   170,   222,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   218,   220,   111,   111,   111,
     215,   216,   384,   220,   220,   192,   217,   194,   222,   218,
     220,   217,   220,   231,   218,   215,   216,   477,   237,   220,
     218,    11,   367,   210,   211,   238,   218,   409,   231,   231,
     368,   370,   369,   142,   158,   371,   418,   272,   407,   212,
     239,   237,   236,   236,   231,   427,   236,   238,   534,   318,
     318,   330,   536,   511,   418,    88,   413,   194,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   260,    -1,    -1,    -1,   522,
      -1,    -1,   525,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   162,    -1,    -1,
     277,    -1,    -1,   280,    -1,   477,   237,    -1,    -1,    -1,
      -1,    -1,    -1,   485,    -1,    -1,   488,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   511,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     215,   216,    -1,    -1,    -1,    -1,    -1,   529,    -1,    -1,
      -1,    -1,   534,   535,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   548,   354,   355,   356,
     357,   358,   359,   360,   361,   362,   363,   364,   365,   366,
     367,   368,   369,   370,   371,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   385,    -1,
     387,   388,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   405,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   413,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,     8,     9,    10,   426,
      12,    13,    14,    15,    16,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    -1,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,   101,
     102,   103,   104,   105,    -1,   107,   108,   109,   110,   111,
     112,   113,    -1,   115,    -1,    -1,   118,   119,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     4,     5,     6,     7,    -1,
      -1,    -1,    -1,    -1,   136,    -1,    -1,    -1,    -1,    -1,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,   158,    -1,    -1,    -1,
     162,    -1,    -1,    -1,    -1,    -1,    -1,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    73,    74,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   215,   216,   217,    -1,   219,    -1,    -1,
      -1,   223,   224,   225,   226,   104,    -1,    -1,   107,   108,
     109,    -1,    -1,    -1,    -1,    -1,   238,   239,   240,     3,
       4,     5,     6,     7,     8,     9,    10,    -1,    12,    13,
      14,    15,    16,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    -1,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    -1,
      -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,   103,
     104,   105,    -1,   107,   108,   109,   110,   111,   112,   113,
      -1,   115,    -1,    -1,   118,   119,    -1,    -1,     4,     5,
       6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   136,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
      -1,    -1,    -1,    -1,   158,    -1,    -1,    -1,   162,    -1,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   215,   216,   217,    -1,   219,    -1,    -1,   104,   223,
     224,   225,   226,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   238,   239,   240,     3,     4,     5,
       6,     7,     8,     9,    10,    -1,    12,    13,    14,    15,
      16,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    -1,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    -1,    -1,    -1,
      -1,    -1,    98,    99,   100,   101,   102,   103,   104,   105,
      -1,   107,   108,   109,   110,   111,   112,   113,    -1,   115,
      -1,    -1,   118,   119,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     136,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   158,    -1,    -1,    -1,   162,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   215,
     216,   217,    -1,   219,    -1,    -1,    -1,   223,   224,   225,
     226,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   238,   239,   240,     3,     4,     5,     6,     7,
       8,     9,    10,    -1,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    -1,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    -1,    -1,    -1,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,   105,    -1,   107,
     108,   109,   110,   111,   112,   113,    -1,   115,    -1,    -1,
     118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   136,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     158,    -1,    -1,    -1,   162,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   215,   216,   217,
      -1,   219,    -1,    -1,    -1,   223,   224,   225,   226,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     238,   239,     3,     4,     5,     6,     7,     8,     9,    10,
      -1,    12,    13,    14,    15,    16,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,
     101,   102,   103,   104,   105,    -1,   107,   108,   109,   110,
     111,   112,   113,    -1,   115,    -1,    -1,   118,   119,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   136,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   158,    -1,    -1,
      -1,   162,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   215,   216,   217,    -1,   219,    -1,
      -1,    -1,   223,   224,   225,   226,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   238,   239,     3,
       4,     5,     6,     7,     8,     9,    10,    -1,    12,    13,
      14,    15,    16,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    -1,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    -1,
      -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,   103,
     104,   105,    -1,   107,   108,   109,   110,   111,   112,   113,
      -1,   115,    -1,    -1,   118,   119,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   136,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   158,    -1,    -1,    -1,   162,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   215,   216,   217,    -1,    -1,    -1,    -1,    -1,   223,
     224,   225,   226,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   238,   239,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    -1,     4,     5,     6,     7,    98,    99,   100,
     101,   102,   103,   104,    -1,    -1,    -1,   108,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,
     101,   102,   103,   104,    -1,    -1,    -1,   108,    -1,    -1,
      -1,    -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,    -1,    -1,    -1,    -1,    -1,   240,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    -1,     4,     5,     6,     7,    98,    99,   100,   101,
     102,   103,   104,    -1,    -1,    -1,   108,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,    -1,    -1,    -1,    -1,    -1,   240,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,   101,
     102,   103,   104,    -1,    -1,    -1,   108,    -1,    -1,    -1,
      -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,    -1,   240,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      -1,     4,     5,     6,     7,    98,    99,   100,   101,   102,
     103,   104,    -1,    -1,    -1,   108,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,    -1,   240,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      -1,    -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,
     103,   104,    -1,    -1,    -1,   108,    -1,    -1,    -1,    -1,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,    -1,    -1,    -1,    -1,    -1,   240,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    -1,
       4,     5,     6,     7,    98,    99,   100,   101,   102,   103,
     104,    -1,    -1,    -1,   108,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,    -1,    -1,    -1,    -1,    -1,   240,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    -1,
      -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,   103,
     104,    -1,    -1,    -1,   108,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,   240,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,
     101,   102,   103,   104,    -1,    -1,   107,   108,   109,   110,
     111,   112,   113,    -1,   115,    -1,    -1,   118,   119,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   240,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   136,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   158,    -1,    -1,
      -1,   162,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   215,   216,   217,    -1,    -1,    -1,
      -1,    -1,   223,   224,   225,   226,     4,     5,     6,     7,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   238,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    -1,    -1,    -1,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,    -1,    -1,   107,
     108,   109,   110,   111,   112,   113,    -1,   115,    -1,    -1,
     118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   217,
      -1,    -1,    -1,    -1,    -1,   223,   224,   225,   226,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   239,    -1,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    -1,    -1,
      -1,    -1,    -1,    98,    99,   100,   101,   102,   103,   104,
      -1,    -1,   107,   108,   109,   110,   111,   112,   113,    -1,
     115,    -1,    -1,   118,   119,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   217,    -1,    -1,    -1,    -1,    -1,   223,   224,
     225,   226,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,   238,    -1,    -1,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    -1,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,
     101,   102,   103,   104,    -1,    -1,   107,   108,   109,   110,
     111,   112,   113,    -1,   115,    -1,    -1,   118,   119,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   136,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   162,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   215,   216,   217,     4,     5,     6,
       7,    -1,   223,   224,   225,   226,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    -1,    -1,    -1,    -1,
      -1,    98,    99,   100,   101,   102,   103,   104,    -1,    -1,
     107,   108,   109,   110,   111,   112,   113,    -1,   115,    -1,
      -1,   118,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,     5,
       6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
     217,    -1,    -1,   220,    -1,    -1,   223,   224,   225,   226,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    -1,    -1,    -1,
      -1,    -1,    98,    99,   100,   101,   102,   103,   104,    -1,
      -1,   107,   108,   109,   110,   111,   112,   113,    -1,   115,
      -1,    -1,   118,   119,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      -1,   217,    -1,    -1,   220,    -1,    -1,   223,   224,   225,
     226,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    -1,    -1,
      -1,    -1,    -1,    98,    99,   100,   101,   102,   103,   104,
      -1,    -1,   107,   108,   109,   110,   111,   112,   113,   114,
     115,    -1,    -1,   118,   119,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,   217,    -1,    -1,    -1,    -1,    -1,   223,   224,
     225,   226,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    -1,
      -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,   103,
     104,    -1,    -1,   107,   108,   109,   110,   111,   112,   113,
      -1,   115,    -1,    -1,   118,   119,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,   217,    -1,    -1,   220,    -1,    -1,   223,
     224,   225,   226,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      -1,    -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,
     103,   104,    -1,    -1,   107,   108,   109,   110,   111,   112,
     113,    -1,   115,    -1,    -1,   118,   119,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,   217,    -1,    -1,   220,    -1,    -1,
     223,   224,   225,   226,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,   101,
     102,   103,   104,    -1,    -1,   107,   108,   109,   110,   111,
     112,   113,    -1,   115,    -1,    -1,   118,   119,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,   217,    -1,    -1,    -1,    -1,
      -1,   223,   224,   225,   226,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,
     101,   102,   103,   104,    -1,    -1,   107,   108,   109,   110,
     111,   112,   113,    -1,   115,    -1,    -1,   118,   119,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     4,     5,     6,     7,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    -1,   217,    -1,    -1,    -1,
      -1,    -1,   223,   224,   225,   226,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    -1,    -1,    -1,    -1,    -1,    98,    99,
     100,   101,   102,   103,   104,    -1,    -1,   107,   108,   109,
     110,   111,   112,   113,    -1,   115,    -1,    -1,   118,   119,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,    -1,   217,    -1,    -1,
      -1,    -1,    -1,   223,   224,   225,   226,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    -1,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    -1,    -1,    -1,    -1,    -1,    98,    99,   100,   101,
     102,   103,   104,    -1,   106,    -1,   108,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   136,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,   158,    -1,    -1,    -1,
     162,    -1,    -1,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    -1,
      -1,    -1,    -1,   215,   216,    -1,    -1,   219,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    -1,    -1,
      -1,    -1,    -1,    98,    99,   100,   101,   102,   103,   104,
      -1,    -1,    -1,   108,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   136,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    -1,   158,    -1,    -1,    -1,   162,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    -1,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
     215,   216,    -1,    -1,   219,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    -1,    -1,    -1,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,    -1,    -1,    -1,
     108,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   136,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   162,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    -1,    -1,    -1,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   215,   216,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,    -1,    -1,    -1,   108,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,    -1,    -1,
      38,    -1,    -1,    -1,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    -1,     4,     5,     6,     7,
      98,    99,   100,   101,   102,   103,   104,    -1,    -1,    -1,
     108,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,    -1,    -1,    -1,
     108,     4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      -1,    -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,
     103,   104,    -1,    -1,    -1,   108
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   242,   243,     0,     3,     4,     5,     6,     7,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    98,    99,   100,   101,   102,   103,   104,
     106,   108,   136,   158,   162,   215,   216,   219,   245,   278,
     280,   281,   282,   283,   288,   289,   290,   291,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   338,   341,   342,   343,   344,   162,    35,    36,    39,
     230,   107,   108,   109,   239,   244,   244,   107,   109,   246,
     291,   294,   136,   290,     3,   244,   299,   338,   238,   239,
     321,   218,   222,     3,    35,    36,    37,    38,    42,    93,
      94,    95,    96,    97,   284,   285,   286,   291,   292,   295,
     222,   238,   107,   109,   244,   246,   312,   294,   295,   219,
     219,   230,   230,   230,   230,   280,   290,   341,   108,    42,
     240,   291,   295,   306,   307,   308,   309,   236,   239,   239,
     294,   246,   217,   220,   217,   220,     8,     9,    10,    12,
      13,    14,    15,    16,   105,   110,   111,   112,   113,   115,
     118,   119,   217,   223,   224,   225,   226,   238,   239,   240,
     246,   247,   248,   250,   251,   252,   253,   254,   255,   256,
     261,   262,   263,   264,   265,   266,   267,   268,   269,   270,
     271,   272,   273,   274,   276,   278,   279,   280,   288,   295,
     315,   316,   317,   318,   322,   323,   324,   327,   333,   337,
     342,   236,   285,    36,    35,   291,   284,   287,   292,   295,
      42,   284,   286,   244,   312,   244,   312,   219,   237,   217,
     219,   237,   220,   261,   273,   277,   295,   220,   277,   299,
     108,   108,   108,   321,   231,   291,    42,   240,   307,   244,
     310,   311,   312,   295,   108,   240,   306,   240,   306,   217,
     114,   277,   339,   340,   340,   238,   238,   316,   217,   217,
     238,   238,   276,   217,   217,   261,   261,   276,   298,   240,
     319,   118,   119,   219,   221,   218,   218,   222,   104,   274,
     217,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   237,   275,   261,   227,   228,   229,   223,   224,   116,
     117,   120,   121,   230,   231,   122,   123,   232,   233,   234,
     124,   125,   235,   222,   238,   238,   240,   316,   317,   318,
     244,   284,   284,   287,   292,   219,   236,   237,   219,   237,
     237,   220,   277,   239,   274,   313,   277,   313,   220,   220,
     222,   231,   231,   222,   222,   219,   236,   222,   238,   239,
     240,   240,   218,   222,   218,   105,   315,   323,   334,   276,
     238,   276,   276,   290,   326,   218,   218,   322,   249,   276,
     244,   246,   257,   258,   259,   260,   274,   274,   261,   261,
     261,   263,   263,   264,   264,   265,   265,   265,   265,   266,
     266,   267,   268,   269,   270,   271,   276,   274,   284,   277,
     244,   277,   220,   313,   313,   237,   220,   313,   314,   220,
     111,   111,   111,   277,   244,   311,   240,   306,   220,   339,
     220,   217,   326,   335,   336,   218,   218,   244,   218,   261,
     240,   220,   217,   218,   218,   222,   104,   274,   236,   220,
     237,   313,   222,   240,   231,   231,   231,   220,   240,   276,
     238,   218,   316,   325,   239,   328,   237,   317,   320,   321,
     274,   274,   236,   313,   313,   236,   218,   276,   320,    11,
      17,    18,   240,   329,   330,   331,   332,   313,   244,   244,
     238,   316,   276,   236,   316,   329,   316,   240,   331,   236
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (&yylloc, state, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, &yylloc, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, &yylloc, scanner)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, Location, state); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, state)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
  YYUSE (state);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp, state)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, state);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, struct _mesa_glsl_parse_state *state)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule, state)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
    struct _mesa_glsl_parse_state *state;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       , state);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule, state); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp, state)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (state);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (struct _mesa_glsl_parse_state *state);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (struct _mesa_glsl_parse_state *state)
#else
int
yyparse (state)
    struct _mesa_glsl_parse_state *state;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Location data for the lookahead symbol.  */
YYLTYPE yylloc;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.
       `yyls': related to locations.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;

#if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 1;
#endif

/* User initialization code.  */

/* Line 1590 of yacc.c  */
#line 50 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
{
   yylloc.first_line = 1;
   yylloc.first_column = 1;
   yylloc.last_line = 1;
   yylloc.last_column = 1;
   yylloc.source_file = 0;
}

/* Line 1590 of yacc.c  */
#line 3290 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
  yylsp[0] = yylloc;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);

	yyls = yyls1;
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
	YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;
  *++yylsp = yylloc;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1806 of yacc.c  */
#line 278 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   _mesa_glsl_initialize_types(state);
	}
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 282 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   delete state->symbols;
	   state->symbols = new(ralloc_parent(state)) glsl_symbol_table;
	   _mesa_glsl_initialize_types(state);
	}
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 297 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   /* FINISHME: The NULL test is required because pragmas are set to
		* FINISHME: NULL. (See production rule for external_declaration.)
		*/
	   if ((yyvsp[(1) - (1)].node) != NULL)
		  state->translation_unit.push_tail(& (yyvsp[(1) - (1)].node)->link);
	}
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 305 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   /* FINISHME: The NULL test is required because pragmas are set to
		* FINISHME: NULL. (See production rule for external_declaration.)
		*/
	   if ((yyvsp[(2) - (2)].node) != NULL)
		  state->translation_unit.push_tail(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 321 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_identifier, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.identifier = (yyvsp[(1) - (1)].identifier);
	}
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 328 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_int_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.int_constant = (yyvsp[(1) - (1)].n);
	}
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 335 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(state->bGenerateES ? ast_int_constant : ast_uint_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   if (state->bGenerateES)
	   {
		   (yyval.expression)->primary_expression.int_constant = (yyvsp[(1) - (1)].n);
	   }
	   else
	   {
		   (yyval.expression)->primary_expression.uint_constant = (yyvsp[(1) - (1)].n);
	   }
	}
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 349 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_float_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.float_constant = (yyvsp[(1) - (1)].real);
	}
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 356 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_bool_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.bool_constant = (yyvsp[(1) - (1)].n);
	}
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 363 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(2) - (3)].expression);
	}
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 371 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_array_index, (yyvsp[(1) - (4)].expression), (yyvsp[(3) - (4)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 377 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (1)].expression);
	}
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 381 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_field_selection, (yyvsp[(1) - (3)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.identifier = (yyvsp[(3) - (3)].identifier);
	}
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 388 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_post_inc, (yyvsp[(1) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 394 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_post_dec, (yyvsp[(1) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 412 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_field_selection, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 431 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (2)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(2) - (2)].expression)->link);
	}
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 437 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 453 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_function_expression((yyvsp[(1) - (1)].type_specifier));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 459 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (1)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 466 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (1)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 486 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (2)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(2) - (2)].expression)->link);
	}
    break;

  case 42:

/* Line 1806 of yacc.c  */
#line 492 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 504 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (2)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 45:

/* Line 1806 of yacc.c  */
#line 516 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_pre_inc, (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 522 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_pre_dec, (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 47:

/* Line 1806 of yacc.c  */
#line 528 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression((yyvsp[(1) - (2)].n), (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 48:

/* Line 1806 of yacc.c  */
#line 534 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_expression(ast_type_cast, (yyvsp[(4) - (4)].expression), NULL, NULL);
		(yyval.expression)->primary_expression.type_specifier = (yyvsp[(2) - (4)].type_specifier);
		(yyval.expression)->set_location(yylloc);
	}
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 544 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_plus; }
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 545 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_neg; }
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 546 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_logic_not; }
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 547 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_bit_not; }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 553 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_mul, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 55:

/* Line 1806 of yacc.c  */
#line 559 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_div, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 565 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_mod, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 575 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_add, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 59:

/* Line 1806 of yacc.c  */
#line 581 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_sub, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 591 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_lshift, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 62:

/* Line 1806 of yacc.c  */
#line 597 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_rshift, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 607 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_less, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 613 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_greater, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 619 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_lequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 625 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_gequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 635 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_equal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 70:

/* Line 1806 of yacc.c  */
#line 641 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_nequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 651 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_and, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 74:

/* Line 1806 of yacc.c  */
#line 661 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_xor, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 76:

/* Line 1806 of yacc.c  */
#line 671 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_or, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 78:

/* Line 1806 of yacc.c  */
#line 681 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_and, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 80:

/* Line 1806 of yacc.c  */
#line 691 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_or, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 82:

/* Line 1806 of yacc.c  */
#line 701 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_conditional, (yyvsp[(1) - (5)].expression), (yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 84:

/* Line 1806 of yacc.c  */
#line 711 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression((yyvsp[(2) - (3)].n), (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 85:

/* Line 1806 of yacc.c  */
#line 719 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_assign; }
    break;

  case 86:

/* Line 1806 of yacc.c  */
#line 720 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_mul_assign; }
    break;

  case 87:

/* Line 1806 of yacc.c  */
#line 721 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_div_assign; }
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 722 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_mod_assign; }
    break;

  case 89:

/* Line 1806 of yacc.c  */
#line 723 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_add_assign; }
    break;

  case 90:

/* Line 1806 of yacc.c  */
#line 724 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_sub_assign; }
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 725 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_ls_assign; }
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 726 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_rs_assign; }
    break;

  case 93:

/* Line 1806 of yacc.c  */
#line 727 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_and_assign; }
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 728 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_xor_assign; }
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 729 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_or_assign; }
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 734 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (1)].expression);
	}
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 738 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   if ((yyvsp[(1) - (3)].expression)->oper != ast_sequence) {
		  (yyval.expression) = new(ctx) ast_expression(ast_sequence, NULL, NULL, NULL);
		  (yyval.expression)->set_location(yylloc);
		  (yyval.expression)->expressions.push_tail(& (yyvsp[(1) - (3)].expression)->link);
	   } else {
		  (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   }

	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 99:

/* Line 1806 of yacc.c  */
#line 758 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   state->symbols->pop_scope();
	   (yyval.function) = (yyvsp[(1) - (2)].function);
	}
    break;

  case 100:

/* Line 1806 of yacc.c  */
#line 766 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(1) - (1)].function);
	}
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 770 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (yyvsp[(1) - (2)].declarator_list);
	}
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 777 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	}
    break;

  case 103:

/* Line 1806 of yacc.c  */
#line 780 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.function) = (yyvsp[(1) - (4)].function);
		(yyval.function)->return_semantic = (yyvsp[(4) - (4)].identifier);
	}
    break;

  case 106:

/* Line 1806 of yacc.c  */
#line 793 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.function) = (yyvsp[(1) - (2)].function);
	   (yyval.function)->parameters.push_tail(& (yyvsp[(2) - (2)].parameter_declarator)->link);
	}
    break;

  case 107:

/* Line 1806 of yacc.c  */
#line 798 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.function) = (yyvsp[(1) - (3)].function);
	   (yyval.function)->parameters.push_tail(& (yyvsp[(3) - (3)].parameter_declarator)->link);
	}
    break;

  case 108:

/* Line 1806 of yacc.c  */
#line 806 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function) = new(ctx) ast_function();
	   (yyval.function)->set_location(yylloc);
	   (yyval.function)->return_type = (yyvsp[(1) - (3)].fully_specified_type);
	   (yyval.function)->identifier = (yyvsp[(2) - (3)].identifier);

	   state->symbols->add_function(new(state) ir_function((yyvsp[(2) - (3)].identifier)));
	   state->symbols->push_scope();
	}
    break;

  case 109:

/* Line 1806 of yacc.c  */
#line 817 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function) = new(ctx) ast_function();
	   (yyval.function)->set_location(yylloc);
	   (yyval.function)->return_type = (yyvsp[(2) - (4)].fully_specified_type);
	   (yyval.function)->identifier = (yyvsp[(3) - (4)].identifier);

	   state->symbols->add_function(new(state) ir_function((yyvsp[(3) - (4)].identifier)));
	   state->symbols->push_scope();
	}
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 831 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (2)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (2)].identifier);
	}
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 841 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
		(yyval.parameter_declarator)->set_location(yylloc);
		(yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
		(yyval.parameter_declarator)->type->set_location(yylloc);
		(yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (4)].type_specifier);
		(yyval.parameter_declarator)->identifier = (yyvsp[(2) - (4)].identifier);
		(yyval.parameter_declarator)->default_value = (yyvsp[(4) - (4)].expression);
	}
    break;

  case 112:

/* Line 1806 of yacc.c  */
#line 852 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
		(yyval.parameter_declarator)->set_location(yylloc);
		(yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
		(yyval.parameter_declarator)->type->set_location(yylloc);
		(yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (4)].type_specifier);
		(yyval.parameter_declarator)->identifier = (yyvsp[(2) - (4)].identifier);
		(yyval.parameter_declarator)->semantic = (yyvsp[(4) - (4)].identifier);
	}
    break;

  case 113:

/* Line 1806 of yacc.c  */
#line 863 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (2)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (2)].declaration)->identifier;
	   (yyval.parameter_declarator)->is_array = true;
	   (yyval.parameter_declarator)->array_size = (yyvsp[(2) - (2)].declaration)->array_size;
	}
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 875 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (7)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (7)].identifier);
	   (yyval.parameter_declarator)->is_array = true;
	   (yyval.parameter_declarator)->array_size = (yyvsp[(4) - (7)].expression);
	   (yyval.parameter_declarator)->semantic = (yyvsp[(7) - (7)].identifier);
	}
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 891 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(3) - (3)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	}
    break;

  case 116:

/* Line 1806 of yacc.c  */
#line 898 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(3) - (3)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	}
    break;

  case 117:

/* Line 1806 of yacc.c  */
#line 905 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (4)].type_qualifier).flags.i |= (yyvsp[(2) - (4)].type_qualifier).flags.i;
	   (yyvsp[(1) - (4)].type_qualifier).flags.i |= (yyvsp[(3) - (4)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(4) - (4)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (4)].type_qualifier);
	}
    break;

  case 118:

/* Line 1806 of yacc.c  */
#line 913 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.parameter_declarator) = (yyvsp[(2) - (2)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 119:

/* Line 1806 of yacc.c  */
#line 918 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.parameter_declarator) = (yyvsp[(2) - (2)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 121:

/* Line 1806 of yacc.c  */
#line 924 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(3) - (3)].type_specifier);
	}
    break;

  case 122:

/* Line 1806 of yacc.c  */
#line 935 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(2) - (2)].type_specifier);
	}
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 947 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 124:

/* Line 1806 of yacc.c  */
#line 952 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 957 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 963 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.in = 1;
		(yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 969 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.in = 1;
		(yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 128:

/* Line 1806 of yacc.c  */
#line 975 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_point = 1;
	}
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 980 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_line = 1;
	}
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 985 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_triangle = 1;
	}
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 990 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_lineadj = 1;
	}
    break;

  case 132:

/* Line 1806 of yacc.c  */
#line 995 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_triangleadj = 1;
	}
    break;

  case 135:

/* Line 1806 of yacc.c  */
#line 1008 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (3)].identifier), false, NULL, NULL);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (3)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (3)].identifier), ir_var_auto));
	}
    break;

  case 136:

/* Line 1806 of yacc.c  */
#line 1018 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (5)].identifier), true, NULL, NULL);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].identifier), ir_var_auto));
	}
    break;

  case 137:

/* Line 1806 of yacc.c  */
#line 1028 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(3) - (3)].declaration);

	   (yyval.declarator_list) = (yyvsp[(1) - (3)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (3)].declaration)->identifier, ir_var_auto));
	}
    break;

  case 138:

/* Line 1806 of yacc.c  */
#line 1037 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (7)].identifier), true, NULL, (yyvsp[(7) - (7)].expression));
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (7)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (7)].identifier), ir_var_auto));
	}
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 1047 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(3) - (5)].declaration);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].declaration)->identifier, ir_var_auto));
	}
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 1057 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (5)].identifier), false, NULL, (yyvsp[(5) - (5)].expression));
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].identifier), ir_var_auto));
	}
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 1071 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   /* Empty declaration list is valid. */
	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (1)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	}
    break;

  case 142:

/* Line 1806 of yacc.c  */
#line 1078 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (2)].identifier), false, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (2)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 143:

/* Line 1806 of yacc.c  */
#line 1087 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), true, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 1096 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(2) - (2)].declaration);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (2)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 145:

/* Line 1806 of yacc.c  */
#line 1105 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (6)].identifier), true, NULL, (yyvsp[(6) - (6)].expression));

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (6)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 146:

/* Line 1806 of yacc.c  */
#line 1114 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(2) - (4)].declaration);
	   decl->initializer = (yyvsp[(4) - (4)].expression);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 147:

/* Line 1806 of yacc.c  */
#line 1124 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), false, NULL, (yyvsp[(4) - (4)].expression));

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 148:

/* Line 1806 of yacc.c  */
#line 1133 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (2)].identifier), false, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list(NULL);
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->invariant = true;

	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 149:

/* Line 1806 of yacc.c  */
#line 1147 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
	   (yyval.fully_specified_type)->set_location(yylloc);
	   (yyval.fully_specified_type)->specifier = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 150:

/* Line 1806 of yacc.c  */
#line 1154 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
	   (yyval.fully_specified_type)->set_location(yylloc);
	   (yyval.fully_specified_type)->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.fully_specified_type)->specifier = (yyvsp[(2) - (2)].type_specifier);
	}
    break;

  case 151:

/* Line 1806 of yacc.c  */
#line 1165 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.smooth = 1;
	}
    break;

  case 152:

/* Line 1806 of yacc.c  */
#line 1170 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.flat = 1;
	}
    break;

  case 153:

/* Line 1806 of yacc.c  */
#line 1175 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.noperspective = 1;
	}
    break;

  case 155:

/* Line 1806 of yacc.c  */
#line 1184 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 156:

/* Line 1806 of yacc.c  */
#line 1189 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 157:

/* Line 1806 of yacc.c  */
#line 1194 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 158:

/* Line 1806 of yacc.c  */
#line 1199 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.constant = 1;
	}
    break;

  case 159:

/* Line 1806 of yacc.c  */
#line 1204 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.uniform = 1;
	}
    break;

  case 162:

/* Line 1806 of yacc.c  */
#line 1214 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.type_qualifier).flags.i |= (yyvsp[(2) - (2)].type_qualifier).flags.i;
	}
    break;

  case 163:

/* Line 1806 of yacc.c  */
#line 1219 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 164:

/* Line 1806 of yacc.c  */
#line 1224 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(2) - (3)].type_qualifier);
	   (yyval.type_qualifier).flags.i |= (yyvsp[(3) - (3)].type_qualifier).flags.i;
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 165:

/* Line 1806 of yacc.c  */
#line 1230 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 166:

/* Line 1806 of yacc.c  */
#line 1238 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.constant = 1;
	}
    break;

  case 167:

/* Line 1806 of yacc.c  */
#line 1243 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.varying = 1;
	}
    break;

  case 168:

/* Line 1806 of yacc.c  */
#line 1248 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1;
	   (yyval.type_qualifier).flags.q.varying = 1;
	}
    break;

  case 169:

/* Line 1806 of yacc.c  */
#line 1254 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 170:

/* Line 1806 of yacc.c  */
#line 1259 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 171:

/* Line 1806 of yacc.c  */
#line 1264 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1; (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 172:

/* Line 1806 of yacc.c  */
#line 1269 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1; (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 173:

/* Line 1806 of yacc.c  */
#line 1274 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.uniform = 1;
	}
    break;

  case 174:

/* Line 1806 of yacc.c  */
#line 1279 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.row_major = 1;
	}
    break;

  case 175:

/* Line 1806 of yacc.c  */
#line 1284 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.column_major = 1;
	}
    break;

  case 176:

/* Line 1806 of yacc.c  */
#line 1289 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 177:

/* Line 1806 of yacc.c  */
#line 1294 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.constant = 1;
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 178:

/* Line 1806 of yacc.c  */
#line 1300 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.constant = 1;
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 179:

/* Line 1806 of yacc.c  */
#line 1306 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.coherent = 1;
	}
    break;

  case 180:

/* Line 1806 of yacc.c  */
#line 1311 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.shared = 1;
	}
    break;

  case 181:

/* Line 1806 of yacc.c  */
#line 1319 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 184:

/* Line 1806 of yacc.c  */
#line 1331 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (3)].type_specifier);
	   (yyval.type_specifier)->is_array = true;
	   (yyval.type_specifier)->is_unsized_array++;
	}
    break;

  case 185:

/* Line 1806 of yacc.c  */
#line 1337 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (3)].type_specifier);
	   (yyval.type_specifier)->is_unsized_array++;
	}
    break;

  case 186:

/* Line 1806 of yacc.c  */
#line 1342 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (4)].type_specifier);
	   (yyval.type_specifier)->is_array = true;
	   (yyval.type_specifier)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 187:

/* Line 1806 of yacc.c  */
#line 1348 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (4)].type_specifier);
	   (yyvsp[(3) - (4)].expression)->link.next = &((yyval.type_specifier)->array_size->link);
	   (yyval.type_specifier)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 188:

/* Line 1806 of yacc.c  */
#line 1357 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 189:

/* Line 1806 of yacc.c  */
#line 1363 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier),"vec4");
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 190:

/* Line 1806 of yacc.c  */
#line 1369 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->texture_ms_num_samples = (yyvsp[(5) - (6)].n);
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 191:

/* Line 1806 of yacc.c  */
#line 1376 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (4)].identifier),(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 192:

/* Line 1806 of yacc.c  */
#line 1382 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("RWStructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 193:

/* Line 1806 of yacc.c  */
#line 1388 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (4)].identifier),(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 194:

/* Line 1806 of yacc.c  */
#line 1394 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
		(yyval.type_specifier)->patch_size = (yyvsp[(5) - (6)].n);
	}
    break;

  case 195:

/* Line 1806 of yacc.c  */
#line 1401 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
		(yyval.type_specifier)->patch_size = (yyvsp[(5) - (6)].n);
	}
    break;

  case 196:

/* Line 1806 of yacc.c  */
#line 1408 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].struct_specifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 197:

/* Line 1806 of yacc.c  */
#line 1414 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 198:

/* Line 1806 of yacc.c  */
#line 1422 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "void"; }
    break;

  case 199:

/* Line 1806 of yacc.c  */
#line 1423 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "float"; }
    break;

  case 200:

/* Line 1806 of yacc.c  */
#line 1424 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half"; }
    break;

  case 201:

/* Line 1806 of yacc.c  */
#line 1425 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "int"; }
    break;

  case 202:

/* Line 1806 of yacc.c  */
#line 1426 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uint"; }
    break;

  case 203:

/* Line 1806 of yacc.c  */
#line 1427 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bool"; }
    break;

  case 204:

/* Line 1806 of yacc.c  */
#line 1428 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec2"; }
    break;

  case 205:

/* Line 1806 of yacc.c  */
#line 1429 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec3"; }
    break;

  case 206:

/* Line 1806 of yacc.c  */
#line 1430 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec4"; }
    break;

  case 207:

/* Line 1806 of yacc.c  */
#line 1431 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2"; }
    break;

  case 208:

/* Line 1806 of yacc.c  */
#line 1432 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3"; }
    break;

  case 209:

/* Line 1806 of yacc.c  */
#line 1433 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4"; }
    break;

  case 210:

/* Line 1806 of yacc.c  */
#line 1434 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec2"; }
    break;

  case 211:

/* Line 1806 of yacc.c  */
#line 1435 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec3"; }
    break;

  case 212:

/* Line 1806 of yacc.c  */
#line 1436 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec4"; }
    break;

  case 213:

/* Line 1806 of yacc.c  */
#line 1437 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec2"; }
    break;

  case 214:

/* Line 1806 of yacc.c  */
#line 1438 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec3"; }
    break;

  case 215:

/* Line 1806 of yacc.c  */
#line 1439 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec4"; }
    break;

  case 216:

/* Line 1806 of yacc.c  */
#line 1440 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec2"; }
    break;

  case 217:

/* Line 1806 of yacc.c  */
#line 1441 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec3"; }
    break;

  case 218:

/* Line 1806 of yacc.c  */
#line 1442 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec4"; }
    break;

  case 219:

/* Line 1806 of yacc.c  */
#line 1443 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2"; }
    break;

  case 220:

/* Line 1806 of yacc.c  */
#line 1444 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2x3"; }
    break;

  case 221:

/* Line 1806 of yacc.c  */
#line 1445 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2x4"; }
    break;

  case 222:

/* Line 1806 of yacc.c  */
#line 1446 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3x2"; }
    break;

  case 223:

/* Line 1806 of yacc.c  */
#line 1447 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3"; }
    break;

  case 224:

/* Line 1806 of yacc.c  */
#line 1448 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3x4"; }
    break;

  case 225:

/* Line 1806 of yacc.c  */
#line 1449 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4x2"; }
    break;

  case 226:

/* Line 1806 of yacc.c  */
#line 1450 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4x3"; }
    break;

  case 227:

/* Line 1806 of yacc.c  */
#line 1451 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4"; }
    break;

  case 228:

/* Line 1806 of yacc.c  */
#line 1452 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x2"; }
    break;

  case 229:

/* Line 1806 of yacc.c  */
#line 1453 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x3"; }
    break;

  case 230:

/* Line 1806 of yacc.c  */
#line 1454 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x4"; }
    break;

  case 231:

/* Line 1806 of yacc.c  */
#line 1455 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x2"; }
    break;

  case 232:

/* Line 1806 of yacc.c  */
#line 1456 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x3"; }
    break;

  case 233:

/* Line 1806 of yacc.c  */
#line 1457 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x4"; }
    break;

  case 234:

/* Line 1806 of yacc.c  */
#line 1458 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x2"; }
    break;

  case 235:

/* Line 1806 of yacc.c  */
#line 1459 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x3"; }
    break;

  case 236:

/* Line 1806 of yacc.c  */
#line 1460 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x4"; }
    break;

  case 237:

/* Line 1806 of yacc.c  */
#line 1461 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "samplerState"; }
    break;

  case 238:

/* Line 1806 of yacc.c  */
#line 1462 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "samplerComparisonState"; }
    break;

  case 239:

/* Line 1806 of yacc.c  */
#line 1497 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Buffer"; }
    break;

  case 240:

/* Line 1806 of yacc.c  */
#line 1498 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture1D"; }
    break;

  case 241:

/* Line 1806 of yacc.c  */
#line 1499 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture1DArray"; }
    break;

  case 242:

/* Line 1806 of yacc.c  */
#line 1500 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2D"; }
    break;

  case 243:

/* Line 1806 of yacc.c  */
#line 1501 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DArray"; }
    break;

  case 244:

/* Line 1806 of yacc.c  */
#line 1502 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DMS"; }
    break;

  case 245:

/* Line 1806 of yacc.c  */
#line 1503 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DMSArray"; }
    break;

  case 246:

/* Line 1806 of yacc.c  */
#line 1504 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture3D"; }
    break;

  case 247:

/* Line 1806 of yacc.c  */
#line 1505 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TextureCube"; }
    break;

  case 248:

/* Line 1806 of yacc.c  */
#line 1506 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TextureCubeArray"; }
    break;

  case 249:

/* Line 1806 of yacc.c  */
#line 1507 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWBuffer"; }
    break;

  case 250:

/* Line 1806 of yacc.c  */
#line 1508 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWByteAddressBuffer"; }
    break;

  case 251:

/* Line 1806 of yacc.c  */
#line 1509 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture1D"; }
    break;

  case 252:

/* Line 1806 of yacc.c  */
#line 1510 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture1DArray"; }
    break;

  case 253:

/* Line 1806 of yacc.c  */
#line 1511 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture2D"; }
    break;

  case 254:

/* Line 1806 of yacc.c  */
#line 1512 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture2DArray"; }
    break;

  case 255:

/* Line 1806 of yacc.c  */
#line 1513 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture3D"; }
    break;

  case 256:

/* Line 1806 of yacc.c  */
#line 1517 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "PointStream"; }
    break;

  case 257:

/* Line 1806 of yacc.c  */
#line 1518 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "LineStream"; }
    break;

  case 258:

/* Line 1806 of yacc.c  */
#line 1519 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TriangleStream"; }
    break;

  case 259:

/* Line 1806 of yacc.c  */
#line 1523 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "InputPatch"; }
    break;

  case 260:

/* Line 1806 of yacc.c  */
#line 1527 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "OutputPatch"; }
    break;

  case 261:

/* Line 1806 of yacc.c  */
#line 1532 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (5)].identifier), (yyvsp[(4) - (5)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	   state->symbols->add_type((yyvsp[(2) - (5)].identifier), glsl_type::void_type);
	}
    break;

  case 262:

/* Line 1806 of yacc.c  */
#line 1539 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (7)].identifier), (yyvsp[(4) - (7)].identifier), (yyvsp[(6) - (7)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	   state->symbols->add_type((yyvsp[(2) - (7)].identifier), glsl_type::void_type);
	}
    break;

  case 263:

/* Line 1806 of yacc.c  */
#line 1546 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier(NULL, (yyvsp[(3) - (4)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	}
    break;

  case 264:

/* Line 1806 of yacc.c  */
#line 1552 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (4)].identifier),NULL);
		(yyval.struct_specifier)->set_location(yylloc);
		state->symbols->add_type((yyvsp[(2) - (4)].identifier), glsl_type::void_type);
	}
    break;

  case 265:

/* Line 1806 of yacc.c  */
#line 1559 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (6)].identifier), (yyvsp[(4) - (6)].identifier), NULL);
		(yyval.struct_specifier)->set_location(yylloc);
		state->symbols->add_type((yyvsp[(2) - (6)].identifier), glsl_type::void_type);
	}
    break;

  case 266:

/* Line 1806 of yacc.c  */
#line 1566 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier(NULL,NULL);
		(yyval.struct_specifier)->set_location(yylloc);
	}
    break;

  case 267:

/* Line 1806 of yacc.c  */
#line 1575 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_cbuffer_declaration((yyvsp[(2) - (5)].identifier), (yyvsp[(4) - (5)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 268:

/* Line 1806 of yacc.c  */
#line 1581 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		/* Do nothing! */
		(yyval.node) = NULL;
	}
    break;

  case 269:

/* Line 1806 of yacc.c  */
#line 1589 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].declarator_list);
	   (yyvsp[(1) - (1)].declarator_list)->link.self_link();
	}
    break;

  case 270:

/* Line 1806 of yacc.c  */
#line 1594 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (2)].node);
	   (yyval.node)->link.insert_before(& (yyvsp[(2) - (2)].declarator_list)->link);
	}
    break;

  case 271:

/* Line 1806 of yacc.c  */
#line 1602 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (3)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_degenerate_list_at_head(& (yyvsp[(2) - (3)].declaration)->link);
	}
    break;

  case 272:

/* Line 1806 of yacc.c  */
#line 1612 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
		(yyval.fully_specified_type)->set_location(yylloc);
		(yyval.fully_specified_type)->specifier = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 273:

/* Line 1806 of yacc.c  */
#line 1619 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
		(yyval.fully_specified_type)->set_location(yylloc);
		(yyval.fully_specified_type)->specifier = (yyvsp[(2) - (2)].type_specifier);
		(yyval.fully_specified_type)->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 275:

/* Line 1806 of yacc.c  */
#line 1631 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 276:

/* Line 1806 of yacc.c  */
#line 1636 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 277:

/* Line 1806 of yacc.c  */
#line 1641 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 278:

/* Line 1806 of yacc.c  */
#line 1649 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.declaration) = (yyvsp[(1) - (1)].declaration);
	   (yyvsp[(1) - (1)].declaration)->link.self_link();
	}
    break;

  case 279:

/* Line 1806 of yacc.c  */
#line 1654 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.declaration) = (yyvsp[(1) - (3)].declaration);
	   (yyval.declaration)->link.insert_before(& (yyvsp[(3) - (3)].declaration)->link);
	}
    break;

  case 280:

/* Line 1806 of yacc.c  */
#line 1662 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (1)].identifier), false, NULL, NULL);
	   (yyval.declaration)->set_location(yylloc);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(1) - (1)].identifier), ir_var_auto));
	}
    break;

  case 281:

/* Line 1806 of yacc.c  */
#line 1669 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = (yyvsp[(1) - (1)].declaration);
	}
    break;

  case 282:

/* Line 1806 of yacc.c  */
#line 1674 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (3)].identifier), false, NULL, NULL);
		(yyval.declaration)->set_location(yylloc);
		(yyval.declaration)->semantic = (yyvsp[(3) - (3)].identifier);
		state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(1) - (3)].identifier), ir_var_auto));
	}
    break;

  case 283:

/* Line 1806 of yacc.c  */
#line 1682 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (6)].identifier), true, (yyvsp[(3) - (6)].expression), NULL);
	   (yyval.declaration)->set_location(yylloc);
	   (yyval.declaration)->semantic = (yyvsp[(6) - (6)].identifier);
	}
    break;

  case 284:

/* Line 1806 of yacc.c  */
#line 1692 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (4)].identifier), true, (yyvsp[(3) - (4)].expression), NULL);
	   (yyval.declaration)->set_location(yylloc);
	}
    break;

  case 285:

/* Line 1806 of yacc.c  */
#line 1698 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = (yyvsp[(1) - (4)].declaration);
	   (yyvsp[(3) - (4)].expression)->link.next = &((yyval.declaration)->array_size->link);
	   (yyval.declaration)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 287:

/* Line 1806 of yacc.c  */
#line 1708 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_initializer_list_expression();
		(yyval.expression)->expressions.push_degenerate_list_at_head(& (yyvsp[(2) - (3)].expression)->link);
	}
    break;

  case 288:

/* Line 1806 of yacc.c  */
#line 1717 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (1)].expression);
		(yyval.expression)->link.self_link();
	}
    break;

  case 289:

/* Line 1806 of yacc.c  */
#line 1722 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (3)].expression);
		(yyval.expression)->link.insert_before(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 290:

/* Line 1806 of yacc.c  */
#line 1727 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (2)].expression);
	}
    break;

  case 292:

/* Line 1806 of yacc.c  */
#line 1739 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].compound_statement); }
    break;

  case 294:

/* Line 1806 of yacc.c  */
#line 1742 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (ast_node *) (yyvsp[(2) - (2)].compound_statement);
		(yyval.node)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (2)].attribute)->link);
	}
    break;

  case 295:

/* Line 1806 of yacc.c  */
#line 1747 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(2) - (2)].node);
		(yyval.node)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (2)].attribute)->link);
	}
    break;

  case 302:

/* Line 1806 of yacc.c  */
#line 1764 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(true, NULL);
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 303:

/* Line 1806 of yacc.c  */
#line 1770 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   state->symbols->push_scope();
	}
    break;

  case 304:

/* Line 1806 of yacc.c  */
#line 1774 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(true, (yyvsp[(3) - (4)].node));
	   (yyval.compound_statement)->set_location(yylloc);
	   state->symbols->pop_scope();
	}
    break;

  case 305:

/* Line 1806 of yacc.c  */
#line 1783 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].compound_statement); }
    break;

  case 307:

/* Line 1806 of yacc.c  */
#line 1789 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(false, NULL);
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 308:

/* Line 1806 of yacc.c  */
#line 1795 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(false, (yyvsp[(2) - (3)].node));
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 309:

/* Line 1806 of yacc.c  */
#line 1804 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   if ((yyvsp[(1) - (1)].node) == NULL) {
		  _mesa_glsl_error(& (yylsp[(1) - (1)]), state, "<nil> statement\n");
		  check((yyvsp[(1) - (1)].node) != NULL);
	   }

	   (yyval.node) = (yyvsp[(1) - (1)].node);
	   (yyval.node)->link.self_link();
	}
    break;

  case 310:

/* Line 1806 of yacc.c  */
#line 1814 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   if ((yyvsp[(2) - (2)].node) == NULL) {
		  _mesa_glsl_error(& (yylsp[(2) - (2)]), state, "<nil> statement\n");
		  check((yyvsp[(2) - (2)].node) != NULL);
	   }
	   (yyval.node) = (yyvsp[(1) - (2)].node);
	   (yyval.node)->link.insert_before(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 311:

/* Line 1806 of yacc.c  */
#line 1826 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_expression_statement(NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 312:

/* Line 1806 of yacc.c  */
#line 1832 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_expression_statement((yyvsp[(1) - (2)].expression));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 313:

/* Line 1806 of yacc.c  */
#line 1841 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = new(state) ast_selection_statement((yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].selection_rest_statement).then_statement,
						   (yyvsp[(5) - (5)].selection_rest_statement).else_statement);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 314:

/* Line 1806 of yacc.c  */
#line 1850 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.selection_rest_statement).then_statement = (yyvsp[(1) - (3)].node);
	   (yyval.selection_rest_statement).else_statement = (yyvsp[(3) - (3)].node);
	}
    break;

  case 315:

/* Line 1806 of yacc.c  */
#line 1855 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.selection_rest_statement).then_statement = (yyvsp[(1) - (1)].node);
	   (yyval.selection_rest_statement).else_statement = NULL;
	}
    break;

  case 316:

/* Line 1806 of yacc.c  */
#line 1863 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].expression);
	}
    break;

  case 317:

/* Line 1806 of yacc.c  */
#line 1867 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), false, NULL, (yyvsp[(4) - (4)].expression));
	   ast_declarator_list *declarator = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   decl->set_location(yylloc);
	   declarator->set_location(yylloc);

	   declarator->declarations.push_tail(&decl->link);
	   (yyval.node) = declarator;
	}
    break;

  case 318:

/* Line 1806 of yacc.c  */
#line 1885 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = new(state) ast_switch_statement((yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].switch_body));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 319:

/* Line 1806 of yacc.c  */
#line 1893 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.switch_body) = new(state) ast_switch_body(NULL);
	   (yyval.switch_body)->set_location(yylloc);
	}
    break;

  case 320:

/* Line 1806 of yacc.c  */
#line 1898 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.switch_body) = new(state) ast_switch_body((yyvsp[(2) - (3)].case_statement_list));
	   (yyval.switch_body)->set_location(yylloc);
	}
    break;

  case 321:

/* Line 1806 of yacc.c  */
#line 1906 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label) = new(state) ast_case_label((yyvsp[(2) - (3)].expression));
	   (yyval.case_label)->set_location(yylloc);
	}
    break;

  case 322:

/* Line 1806 of yacc.c  */
#line 1911 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label) = new(state) ast_case_label(NULL);
	   (yyval.case_label)->set_location(yylloc);
	}
    break;

  case 323:

/* Line 1806 of yacc.c  */
#line 1919 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_label_list *labels = new(state) ast_case_label_list();

	   labels->labels.push_tail(& (yyvsp[(1) - (1)].case_label)->link);
	   (yyval.case_label_list) = labels;
	   (yyval.case_label_list)->set_location(yylloc);
	}
    break;

  case 324:

/* Line 1806 of yacc.c  */
#line 1927 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label_list) = (yyvsp[(1) - (2)].case_label_list);
	   (yyval.case_label_list)->labels.push_tail(& (yyvsp[(2) - (2)].case_label)->link);
	}
    break;

  case 325:

/* Line 1806 of yacc.c  */
#line 1935 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_statement *stmts = new(state) ast_case_statement((yyvsp[(1) - (2)].case_label_list));
	   stmts->set_location(yylloc);

	   stmts->stmts.push_tail(& (yyvsp[(2) - (2)].node)->link);
	   (yyval.case_statement) = stmts;
	}
    break;

  case 326:

/* Line 1806 of yacc.c  */
#line 1943 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_statement) = (yyvsp[(1) - (2)].case_statement);
	   (yyval.case_statement)->stmts.push_tail(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 327:

/* Line 1806 of yacc.c  */
#line 1951 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_statement_list *cases= new(state) ast_case_statement_list();
	   cases->set_location(yylloc);

	   cases->cases.push_tail(& (yyvsp[(1) - (1)].case_statement)->link);
	   (yyval.case_statement_list) = cases;
	}
    break;

  case 328:

/* Line 1806 of yacc.c  */
#line 1959 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_statement_list) = (yyvsp[(1) - (2)].case_statement_list);
	   (yyval.case_statement_list)->cases.push_tail(& (yyvsp[(2) - (2)].case_statement)->link);
	}
    break;

  case 329:

/* Line 1806 of yacc.c  */
#line 1967 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_while,
							NULL, (yyvsp[(3) - (5)].node), NULL, (yyvsp[(5) - (5)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 330:

/* Line 1806 of yacc.c  */
#line 1974 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_do_while,
							NULL, (yyvsp[(5) - (7)].expression), NULL, (yyvsp[(2) - (7)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 331:

/* Line 1806 of yacc.c  */
#line 1981 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_for,
							(yyvsp[(3) - (6)].node), (yyvsp[(4) - (6)].for_rest_statement).cond, (yyvsp[(4) - (6)].for_rest_statement).rest, (yyvsp[(6) - (6)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 335:

/* Line 1806 of yacc.c  */
#line 1997 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = NULL;
	}
    break;

  case 336:

/* Line 1806 of yacc.c  */
#line 2004 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.for_rest_statement).cond = (yyvsp[(1) - (2)].node);
	   (yyval.for_rest_statement).rest = NULL;
	}
    break;

  case 337:

/* Line 1806 of yacc.c  */
#line 2009 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.for_rest_statement).cond = (yyvsp[(1) - (3)].node);
	   (yyval.for_rest_statement).rest = (yyvsp[(3) - (3)].expression);
	}
    break;

  case 338:

/* Line 1806 of yacc.c  */
#line 2018 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_continue, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 339:

/* Line 1806 of yacc.c  */
#line 2024 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_break, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 340:

/* Line 1806 of yacc.c  */
#line 2030 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_return, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 341:

/* Line 1806 of yacc.c  */
#line 2036 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_return, (yyvsp[(2) - (3)].expression));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 342:

/* Line 1806 of yacc.c  */
#line 2042 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_discard, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 343:

/* Line 1806 of yacc.c  */
#line 2050 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (yyvsp[(1) - (1)].function_definition); }
    break;

  case 344:

/* Line 1806 of yacc.c  */
#line 2051 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 345:

/* Line 1806 of yacc.c  */
#line 2056 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute_arg) = new(ctx) ast_attribute_arg( (yyvsp[(1) - (1)].expression) );
		(yyval.attribute_arg)->link.self_link();
	}
    break;

  case 346:

/* Line 1806 of yacc.c  */
#line 2062 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute_arg) = new(ctx) ast_attribute_arg( (yyvsp[(1) - (1)].string_literal) );
		(yyval.attribute_arg)->link.self_link();
	}
    break;

  case 347:

/* Line 1806 of yacc.c  */
#line 2071 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute_arg) = (yyvsp[(1) - (1)].attribute_arg);
	}
    break;

  case 348:

/* Line 1806 of yacc.c  */
#line 2075 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute_arg) = (yyvsp[(1) - (3)].attribute_arg);
		(yyval.attribute_arg)->link.insert_before( & (yyvsp[(3) - (3)].attribute_arg)->link);
	}
    break;

  case 349:

/* Line 1806 of yacc.c  */
#line 2083 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (3)].identifier) );
		(yyval.attribute)->link.self_link();
	}
    break;

  case 350:

/* Line 1806 of yacc.c  */
#line 2089 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (3)].identifier) );
		(yyval.attribute)->link.self_link();
	}
    break;

  case 351:

/* Line 1806 of yacc.c  */
#line 2095 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (6)].identifier) );
		(yyval.attribute)->link.self_link();
		(yyval.attribute)->arguments.push_degenerate_list_at_head( & (yyvsp[(4) - (6)].attribute_arg)->link );
	}
    break;

  case 352:

/* Line 1806 of yacc.c  */
#line 2102 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (6)].identifier) );
		(yyval.attribute)->link.self_link();
		(yyval.attribute)->arguments.push_degenerate_list_at_head( & (yyvsp[(4) - (6)].attribute_arg)->link );
	}
    break;

  case 353:

/* Line 1806 of yacc.c  */
#line 2112 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute) = (yyvsp[(1) - (2)].attribute);
		(yyval.attribute)->link.insert_before( & (yyvsp[(2) - (2)].attribute)->link);
	}
    break;

  case 354:

/* Line 1806 of yacc.c  */
#line 2117 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute) = (yyvsp[(1) - (1)].attribute);
	}
    break;

  case 355:

/* Line 1806 of yacc.c  */
#line 2124 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function_definition) = new(ctx) ast_function_definition();
	   (yyval.function_definition)->set_location(yylloc);
	   (yyval.function_definition)->prototype = (yyvsp[(1) - (2)].function);
	   (yyval.function_definition)->body = (yyvsp[(2) - (2)].compound_statement);

	   state->symbols->pop_scope();
	}
    break;

  case 356:

/* Line 1806 of yacc.c  */
#line 2134 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function_definition) = new(ctx) ast_function_definition();
	   (yyval.function_definition)->set_location(yylloc);
	   (yyval.function_definition)->prototype = (yyvsp[(2) - (3)].function);
	   (yyval.function_definition)->body = (yyvsp[(3) - (3)].compound_statement);
	   (yyval.function_definition)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (3)].attribute)->link);

	   state->symbols->pop_scope();
	}
    break;

  case 357:

/* Line 1806 of yacc.c  */
#line 2148 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(1) - (1)].function);
	}
    break;

  case 358:

/* Line 1806 of yacc.c  */
#line 2152 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		if ((yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.is_static == 0 && (yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.shared == 0)
		{
			(yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.uniform = 1;
		}
		(yyval.node) = (yyvsp[(1) - (2)].declarator_list);
	}
    break;

  case 359:

/* Line 1806 of yacc.c  */
#line 2160 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (yyvsp[(1) - (1)].node);
	}
    break;



/* Line 1806 of yacc.c  */
#line 6554 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (&yylloc, state, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (&yylloc, state, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, &yylloc, state);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  yyerror_range[1] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yylsp, state);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, state, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, state);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp, state);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



