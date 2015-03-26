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
     POINT_TOK = 346,
     LINE_TOK = 347,
     TRIANGLE_TOK = 348,
     LINEADJ_TOK = 349,
     TRIANGLEADJ_TOK = 350,
     POINTSTREAM = 351,
     LINESTREAM = 352,
     TRIANGLESTREAM = 353,
     INPUTPATCH = 354,
     OUTPUTPATCH = 355,
     STRUCT = 356,
     VOID_TOK = 357,
     WHILE = 358,
     CBUFFER = 359,
     IDENTIFIER = 360,
     TYPE_IDENTIFIER = 361,
     NEW_IDENTIFIER = 362,
     FLOATCONSTANT = 363,
     INTCONSTANT = 364,
     UINTCONSTANT = 365,
     BOOLCONSTANT = 366,
     STRINGCONSTANT = 367,
     FIELD_SELECTION = 368,
     LEFT_OP = 369,
     RIGHT_OP = 370,
     INC_OP = 371,
     DEC_OP = 372,
     LE_OP = 373,
     GE_OP = 374,
     EQ_OP = 375,
     NE_OP = 376,
     AND_OP = 377,
     OR_OP = 378,
     MUL_ASSIGN = 379,
     DIV_ASSIGN = 380,
     ADD_ASSIGN = 381,
     MOD_ASSIGN = 382,
     LEFT_ASSIGN = 383,
     RIGHT_ASSIGN = 384,
     AND_ASSIGN = 385,
     XOR_ASSIGN = 386,
     OR_ASSIGN = 387,
     SUB_ASSIGN = 388,
     INVARIANT = 389,
     VERSION_TOK = 390,
     EXTENSION = 391,
     LINE = 392,
     COLON = 393,
     EOL = 394,
     INTERFACE = 395,
     OUTPUT = 396,
     PRAGMA_DEBUG_ON = 397,
     PRAGMA_DEBUG_OFF = 398,
     PRAGMA_OPTIMIZE_ON = 399,
     PRAGMA_OPTIMIZE_OFF = 400,
     PRAGMA_INVARIANT_ALL = 401,
     LAYOUT_TOK = 402,
     ASM = 403,
     CLASS = 404,
     UNION = 405,
     ENUM = 406,
     TYPEDEF = 407,
     TEMPLATE = 408,
     THIS = 409,
     PACKED_TOK = 410,
     GOTO = 411,
     INLINE_TOK = 412,
     NOINLINE = 413,
     VOLATILE = 414,
     PUBLIC_TOK = 415,
     STATIC = 416,
     EXTERN = 417,
     EXTERNAL = 418,
     LONG_TOK = 419,
     SHORT_TOK = 420,
     DOUBLE_TOK = 421,
     HALF = 422,
     FIXED_TOK = 423,
     UNSIGNED = 424,
     DVEC2 = 425,
     DVEC3 = 426,
     DVEC4 = 427,
     FVEC2 = 428,
     FVEC3 = 429,
     FVEC4 = 430,
     SAMPLER2DRECT = 431,
     SAMPLER3DRECT = 432,
     SAMPLER2DRECTSHADOW = 433,
     SIZEOF = 434,
     CAST = 435,
     NAMESPACE = 436,
     USING = 437,
     ERROR_TOK = 438,
     COMMON = 439,
     PARTITION = 440,
     ACTIVE = 441,
     SAMPLERBUFFER = 442,
     FILTER = 443,
     IMAGE1D = 444,
     IMAGE2D = 445,
     IMAGE3D = 446,
     IMAGECUBE = 447,
     IMAGE1DARRAY = 448,
     IMAGE2DARRAY = 449,
     IIMAGE1D = 450,
     IIMAGE2D = 451,
     IIMAGE3D = 452,
     IIMAGECUBE = 453,
     IIMAGE1DARRAY = 454,
     IIMAGE2DARRAY = 455,
     UIMAGE1D = 456,
     UIMAGE2D = 457,
     UIMAGE3D = 458,
     UIMAGECUBE = 459,
     UIMAGE1DARRAY = 460,
     UIMAGE2DARRAY = 461,
     IMAGE1DSHADOW = 462,
     IMAGE2DSHADOW = 463,
     IMAGEBUFFER = 464,
     IIMAGEBUFFER = 465,
     UIMAGEBUFFER = 466,
     IMAGE1DARRAYSHADOW = 467,
     IMAGE2DARRAYSHADOW = 468,
     ROW_MAJOR = 469,
     COLUMN_MAJOR = 470
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
#line 306 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.h"
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



