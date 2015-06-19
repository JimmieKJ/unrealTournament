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

/* Line 2068 of yacc.c  */
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



/* Line 2068 of yacc.c  */
#line 307 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.h"
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



