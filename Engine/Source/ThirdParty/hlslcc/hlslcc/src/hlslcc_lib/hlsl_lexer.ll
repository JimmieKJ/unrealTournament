%{
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

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
#include "strtod.h"
#include "ast.h"
#include "glsl_parser_extras.h"
#include "hlsl_parser.h"

static int classify_identifier(struct _mesa_glsl_parse_state *, const char *);

#ifdef _MSC_VER
#define YY_NO_UNISTD_H
#endif

#define YY_USER_ACTION						\
   do {								\
	  yylloc->source_file = yyextra->current_source_file; \
	  yylloc->first_column = yycolumn + 1;			\
	  yylloc->first_line = yylineno + 1;			\
	  yycolumn += yyleng;					\
   } while(0);

#define YY_USER_INIT yylineno = 0; yycolumn = 0;

/* A macro for handling reserved words and keywords across language versions.
 *
 * Certain words start out as identifiers, become reserved words in
 * later language revisions, and finally become language keywords.
 *
 * For example, consider the following lexer rule:
 * samplerBuffer       KEYWORD(130, 140, SAMPLERBUFFER)
 *
 * This means that "samplerBuffer" will be treated as:
 * - a keyword (SAMPLERBUFFER token)         ...in GLSL >= 1.40
 * - a reserved word - error                 ...in GLSL >= 1.30
 * - an identifier                           ...in GLSL <  1.30
 */
#define KEYWORD(reserved_version, allowed_version, token)		\
   do {									\
	  if (yyextra->language_version >= allowed_version) {		\
	 return token;							\
	  } else if (yyextra->language_version >= reserved_version) {	\
	 _mesa_glsl_error(yylloc, yyextra,				\
			  "Illegal use of reserved word '%s'", yytext);	\
	 return ERROR_TOK;						\
	  } else {								\
	 yylval->identifier = strdup(yytext);				\
	 return classify_identifier(yyextra, yytext);			\
	  }									\
   } while (0)

/* The ES macro can be used in KEYWORD checks:
 *
 *    word      KEYWORD(110 || ES, 400, TOKEN)
 * ...means the word is reserved in GLSL ES 1.00, while
 *
 *    word      KEYWORD(110, 130 || ES, TOKEN)
 * ...means the word is a legal keyword in GLSL ES 1.00.
 */
#define ES 1

static int
literal_integer(char *text, int len, struct _mesa_glsl_parse_state *state,
		YYSTYPE *lval, YYLTYPE *lloc, int base)
{
   bool is_uint = (text[len - 1] == 'u' ||
		   text[len - 1] == 'U');
   const char *digits = text;

   /* Skip "0x" */
   if (base == 16)
	  digits += 2;

#ifdef _MSC_VER
   unsigned __int64 value = _strtoui64(digits, NULL, base);
#else
   unsigned long long value = strtoull(digits, NULL, base);
#endif

   lval->n = (int)value;

   if (value > UINT_MAX) {
	  /* Note that signed 0xffffffff is valid, not out of range! */
	  if (state->language_version >= 130) {
	 _mesa_glsl_error(lloc, state,
			  "Literal value '%s' out of range", text);
	  } else {
	 _mesa_glsl_warning(lloc, state,
				"Literal value '%s' out of range", text);
	  }
   } else if (base == 10 && !is_uint && (unsigned)value > (unsigned)INT_MAX + 1) {
	  /* Tries to catch unintentionally providing a negative value.
	   * Note that -2147483648 is parsed as -(2147483648), so we don't
	   * want to warn for INT_MAX.
	   */
	  _mesa_glsl_warning(lloc, state,
			 "Signed literal value '%s' is interpreted as %d",
			 text, lval->n);
   }
   return is_uint ? UINTCONSTANT : INTCONSTANT;
}

#define LITERAL_INTEGER(base) \
   literal_integer(yytext, yyleng, yyextra, yylval, yylloc, base)

%}

%option bison-bridge bison-locations reentrant noyywrap
%option nounput noyy_top_state
%option never-interactive
%option prefix="_mesa_hlsl_"
%option extra-type="struct _mesa_glsl_parse_state *"

%x PP PRAGMA

DEC_INT		[1-9][0-9]*
HEX_INT		0[xX][0-9a-fA-F]+
OCT_INT		0[0-7]*
INT		({DEC_INT}|{HEX_INT}|{OCT_INT})
SPC		[ \t]*
SPCP		[ \t]+
HASH		^{SPC}#{SPC}
%%

[ \r\t]+		;

	/* Preprocessor tokens. */ 
^[ \t]*#[ \t]*$			;
{HASH}line{SPCP}{INT}{SPCP}\".*\"{SPC}$ {
				   /* Eat characters until the first digit is
					* encountered
					*/
				   char *ptr = yytext;
				   while (!isdigit(*ptr))
					  ptr++;

				   yylineno = strtol(ptr, &ptr, 0) - 2;

				   while (*ptr != '\"')
					   ptr++;
				   char *filename = ++ptr;
				   while (*ptr != '\"')
					   ptr++;

				   struct _mesa_glsl_parse_state *state = yyextra;
				   void *ctx = state;
				   state->current_source_file = ralloc_strndup(ctx, filename, ptr - filename);
				}
{HASH}line{SPCP}{INT}{SPC}$	{
				   /* Eat characters until the first digit is
					* encountered
					*/
				   char *ptr = yytext;
				   while (!isdigit(*ptr))
					  ptr++;

				   yylineno = strtol(ptr, &ptr, 0) - 2;
				   yyextra->current_source_file = 0;
				}
^{SPC}#{SPC}pragma{SPCP}debug{SPC}\({SPC}on{SPC}\) {
				  BEGIN PP;
				  return PRAGMA_DEBUG_ON;
				}
^{SPC}#{SPC}pragma{SPCP}debug{SPC}\({SPC}off{SPC}\) {
				  BEGIN PP;
				  return PRAGMA_DEBUG_OFF;
				}
^{SPC}#{SPC}pragma{SPCP}optimize{SPC}\({SPC}on{SPC}\) {
				  BEGIN PP;
				  return PRAGMA_OPTIMIZE_ON;
				}
^{SPC}#{SPC}pragma{SPCP}optimize{SPC}\({SPC}off{SPC}\) {
				  BEGIN PP;
				  return PRAGMA_OPTIMIZE_OFF;
				}
^{SPC}#{SPC}pragma{SPCP}STDGL{SPCP}invariant{SPC}\({SPC}all{SPC}\) {
				  BEGIN PP;
				  return PRAGMA_INVARIANT_ALL;
				}
^{SPC}#{SPC}pragma{SPCP}	{ BEGIN PRAGMA; }

<PRAGMA>\n			{ BEGIN 0; yylineno++; yycolumn = 0; }
<PRAGMA>.			{ }

<PP>\/\/[^\n]*			{ }
<PP>[ \t\r]*			{ }
<PP>:				return COLON;
<PP>[_a-zA-Z][_a-zA-Z0-9]*	{
				   yylval->identifier = strdup(yytext);
				   return IDENTIFIER;
				}
<PP>[1-9][0-9]*			{
					yylval->n = strtol(yytext, NULL, 10);
					return INTCONSTANT;
				}
<PP>\n				{ BEGIN 0; yylineno++; yycolumn = 0; return EOL; }

\n		{ yylineno++; yycolumn = 0; }

const		return CONST_TOK;
bool		return BOOL_TOK;
float		return FLOAT_TOK;
float1		return FLOAT_TOK;
half		return HALF_TOK;
int			return INT_TOK;
uint		KEYWORD(130, 130, UINT_TOK);

break		return BREAK;
continue	return CONTINUE;
do		return DO;
while		return WHILE;
else		return ELSE;
for		return FOR;
if		return IF;
discard		return DISCARD;
return		return RETURN;

bool2		return BVEC2;
bool3		return BVEC3;
bool4		return BVEC4;
int2		return IVEC2;
int3		return IVEC3;
int4		return IVEC4;
uint2		KEYWORD(130, 130, UVEC2);
uint3		KEYWORD(130, 130, UVEC3);
uint4		KEYWORD(130, 130, UVEC4);
float2		return VEC2;
float3		return VEC3;
float4		return VEC4;
float2x2	KEYWORD(120, 120, MAT2X2);
float2x3	KEYWORD(120, 120, MAT2X3);
float2x4	KEYWORD(120, 120, MAT2X4);
float3x2	KEYWORD(120, 120, MAT3X2);
float3x3	KEYWORD(120, 120, MAT3X3);
float3x4	KEYWORD(120, 120, MAT3X4);
float4x2	KEYWORD(120, 120, MAT4X2);
float4x3	KEYWORD(120, 120, MAT4X3);
float4x4	KEYWORD(120, 120, MAT4X4);

point			return POINT_TOK;
line			return LINE_TOK;
triangle		return TRIANGLE_TOK;
lineadj			return LINEADJ_TOK;
triangleadj		return TRIANGLEADJ_TOK;
PointStream		return POINTSTREAM;
LineStream		return LINESTREAM;
TriangleStream	return TRIANGLESTREAM;

InputPatch		return INPUTPATCH;
OutputPatch		return OUTPUTPATCH;

in		return IN_TOK;
out		return OUT_TOK;
inout		return INOUT_TOK;
uniform		return UNIFORM;
varying		return VARYING;
centroid	KEYWORD(120, 120, CENTROID);
invariant	KEYWORD(120 || ES, 120 || ES, INVARIANT);
nointerpolation KEYWORD(130 || ES, 130, NOINTERPOLATION);
linear KEYWORD(130, 130, LINEAR);
noperspective	KEYWORD(130, 130, NOPERSPECTIVE);
globallycoherent	KEYWORD(100, 310, GLOBALLYCOHERENT);
groupshared		KEYWORD(100, 310, SHARED);

row_major		return ROW_MAJOR;
column_major	return COLUMN_MAJOR;

Buffer				return BUFFER;
Texture1D			return TEXTURE1D;
Texture1DArray		return TEXTURE1D_ARRAY;
Texture2D			return TEXTURE2D;
Texture2DArray		return TEXTURE2D_ARRAY;
Texture2DMS			return TEXTURE2DMS;
Texture2DMSArray	return TEXTURE2DMS_ARRAY;
Texture3D			return TEXTURE3D;
TextureCube			return TEXTURECUBE;
TextureCubeArray	KEYWORD(100, 310, TEXTURECUBE_ARRAY);
SamplerState		return SAMPLERSTATE;
SamplerComparisonState return SAMPLERSTATE_CMP;

RWBuffer			KEYWORD(100, 310, RWBUFFER);
RWStructuredBuffer	KEYWORD(100, 430, RWSTRUCTUREDBUFFER);
RWByteAddressBuffer	KEYWORD(100, 430, RWBYTEADDRESSBUFFER);
RWTexture1D			KEYWORD(100, 310, RWTEXTURE1D);
RWTexture1DArray	KEYWORD(100, 310, RWTEXTURE1D_ARRAY);
RWTexture2D			KEYWORD(100, 310, RWTEXTURE2D);
RWTexture2DArray	KEYWORD(100, 310, RWTEXTURE2D_ARRAY);
RWTexture3D			KEYWORD(100, 310, RWTEXTURE3D);

struct		return STRUCT;
cbuffer		return CBUFFER;
void		return VOID_TOK;

\+\+		return INC_OP;
--		return DEC_OP;
\<=		return LE_OP;
>=		return GE_OP;
==		return EQ_OP;
!=		return NE_OP;
&&		return AND_OP;
\|\|		return OR_OP;
"<<"		return LEFT_OP;
">>"		return RIGHT_OP;

\*=		return MUL_ASSIGN;
\/=		return DIV_ASSIGN;
\+=		return ADD_ASSIGN;
\%=		return MOD_ASSIGN;
\<\<=		return LEFT_ASSIGN;
>>=		return RIGHT_ASSIGN;
&=		return AND_ASSIGN;
"^="		return XOR_ASSIGN;
\|=		return OR_ASSIGN;
-=		return SUB_ASSIGN;

[1-9][0-9]*[uU]?	{
				return LITERAL_INTEGER(10);
			}
0[xX][0-9a-fA-F]+[uU]?	{
				return LITERAL_INTEGER(16);
			}
0[0-7]*[uU]?		{
				return LITERAL_INTEGER(8);
			}

\"[^"\n]*\"	{
				struct _mesa_glsl_parse_state *state = yyextra;
				void *ctx = state;	
				yylval->string_literal = ralloc_strdup(ctx, yytext);
				return STRINGCONSTANT;
			}

[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?[fF]?	{
				yylval->real = glsl_strtod(yytext, NULL);
				return FLOATCONSTANT;
			}
\.[0-9]+([eE][+-]?[0-9]+)?[fF]?		{
				yylval->real = glsl_strtod(yytext, NULL);
				return FLOATCONSTANT;
			}
[0-9]+\.([eE][+-]?[0-9]+)?[fF]?		{
				yylval->real = glsl_strtod(yytext, NULL);
				return FLOATCONSTANT;
			}
[0-9]+[eE][+-]?[0-9]+[fF]?		{
				yylval->real = glsl_strtod(yytext, NULL);
				return FLOATCONSTANT;
			}
[0-9]+[fF]		{
				yylval->real = glsl_strtod(yytext, NULL);
				return FLOATCONSTANT;
			}

true			{
				yylval->n = 1;
				return BOOLCONSTANT;
			}
false			{
				yylval->n = 0;
				return BOOLCONSTANT;
			}


	/* Reserved words in GLSL 1.10. */
asm		KEYWORD(110 || ES, 999, ASM);
class		KEYWORD(110 || ES, 999, CLASS);
union		KEYWORD(110 || ES, 999, UNION);
enum		KEYWORD(110 || ES, 999, ENUM);
typedef		KEYWORD(110 || ES, 999, TYPEDEF);
template	KEYWORD(110 || ES, 999, TEMPLATE);
this		KEYWORD(110 || ES, 999, THIS);
packed		KEYWORD(110 || ES, 999, PACKED_TOK);
goto		KEYWORD(110 || ES, 999, GOTO);
switch		KEYWORD(110 || ES, 130, SWITCH);
default		KEYWORD(110 || ES, 130, DEFAULT);
inline		return INLINE_TOK;
noinline	KEYWORD(110 || ES, 999, NOINLINE);
volatile	KEYWORD(110 || ES, 999, VOLATILE);
public		KEYWORD(110 || ES, 999, PUBLIC_TOK);
static		return STATIC;
extern		KEYWORD(110 || ES, 999, EXTERN);
external	KEYWORD(110 || ES, 999, EXTERNAL);
interface	KEYWORD(110 || ES, 999, INTERFACE);
long		KEYWORD(110 || ES, 999, LONG_TOK);
short		KEYWORD(110 || ES, 999, SHORT_TOK);
double		KEYWORD(110 || ES, 400, DOUBLE_TOK);
fixed		KEYWORD(110 || ES, 110, FIXED_TOK);
unsigned	KEYWORD(110 || ES, 999, UNSIGNED);
half2		KEYWORD(110 || ES, 150, HVEC2);
half3		KEYWORD(110 || ES, 150, HVEC3);
half4		KEYWORD(110 || ES, 150, HVEC4);
half2x2		KEYWORD(120, 120, HMAT2X2);
half2x3		KEYWORD(120, 120, HMAT2X3);
half2x4		KEYWORD(120, 120, HMAT2X4);
half3x2		KEYWORD(120, 120, HMAT3X2);
half3x3		KEYWORD(120, 120, HMAT3X3);
half3x4		KEYWORD(120, 120, HMAT3X4);
half4x2		KEYWORD(120, 120, HMAT4X2);
half4x3		KEYWORD(120, 120, HMAT4X3);
half4x4		KEYWORD(120, 120, HMAT4X4);
double2		KEYWORD(110 || ES, 400, DVEC2);
double3		KEYWORD(110 || ES, 400, DVEC3);
double4		KEYWORD(110 || ES, 400, DVEC4);
fixed2		KEYWORD(110 || ES, 110, FVEC2);
fixed3		KEYWORD(110 || ES, 110, FVEC3);
fixed4		KEYWORD(110 || ES, 110, FVEC4);
fixed2x2	KEYWORD(110 || ES, 110, FMAT2X2);
fixed2x3	KEYWORD(110 || ES, 110, FMAT2X3);
fixed2x4	KEYWORD(110 || ES, 110, FMAT2X4);
fixed3x2	KEYWORD(110 || ES, 110, FMAT3X2);
fixed3x3	KEYWORD(110 || ES, 110, FMAT3X3);
fixed3x4	KEYWORD(110 || ES, 110, FMAT3X4);
fixed4x2	KEYWORD(110 || ES, 110, FMAT4X2);
fixed4x3	KEYWORD(110 || ES, 110, FMAT4X3);
fixed4x4	KEYWORD(110 || ES, 110, FMAT4X4);
sampler2DRect		return SAMPLER2DRECT;
sampler3DRect		KEYWORD(110 || ES, 999, SAMPLER3DRECT);
sampler2DRectShadow	return SAMPLER2DRECTSHADOW;
sizeof		KEYWORD(110 || ES, 999, SIZEOF);
cast		KEYWORD(110 || ES, 999, CAST);
namespace	KEYWORD(110 || ES, 999, NAMESPACE);
using		KEYWORD(110 || ES, 999, USING);

	/* Additional reserved words in GLSL 1.30. */
case		KEYWORD(130, 130, CASE);
common		KEYWORD(130, 999, COMMON);
partition	KEYWORD(130, 999, PARTITION);
active		KEYWORD(130, 999, ACTIVE);
samplerBuffer	KEYWORD(130, 140, SAMPLERBUFFER);
filter		KEYWORD(130, 999, FILTER);
image1D		KEYWORD(130, 999, IMAGE1D);
image2D		KEYWORD(130, 999, IMAGE2D);
image3D		KEYWORD(130, 999, IMAGE3D);
imageCube	KEYWORD(130, 999, IMAGECUBE);
iimage1D	KEYWORD(130, 999, IIMAGE1D);
iimage2D	KEYWORD(130, 999, IIMAGE2D);
iimage3D	KEYWORD(130, 999, IIMAGE3D);
iimageCube	KEYWORD(130, 999, IIMAGECUBE);
uimage1D	KEYWORD(130, 999, UIMAGE1D);
uimage2D	KEYWORD(130, 999, UIMAGE2D);
uimage3D	KEYWORD(130, 999, UIMAGE3D);
uimageCube	KEYWORD(130, 999, UIMAGECUBE);
image1DArray	KEYWORD(130, 999, IMAGE1DARRAY);
image2DArray	KEYWORD(130, 999, IMAGE2DARRAY);
iimage1DArray	KEYWORD(130, 999, IIMAGE1DARRAY);
iimage2DArray	KEYWORD(130, 999, IIMAGE2DARRAY);
uimage1DArray	KEYWORD(130, 999, UIMAGE1DARRAY);
uimage2DArray	KEYWORD(130, 999, UIMAGE2DARRAY);
image1DShadow	KEYWORD(130, 999, IMAGE1DSHADOW);
image2DShadow	KEYWORD(130, 999, IMAGE2DSHADOW);
image1DArrayShadow KEYWORD(130, 999, IMAGE1DARRAYSHADOW);
image2DArrayShadow KEYWORD(130, 999, IMAGE2DARRAYSHADOW);
imageBuffer	KEYWORD(130, 999, IMAGEBUFFER);
iimageBuffer	KEYWORD(130, 999, IIMAGEBUFFER);
uimageBuffer	KEYWORD(130, 999, UIMAGEBUFFER);

[_a-zA-Z][_a-zA-Z0-9]*	{
				struct _mesa_glsl_parse_state *state = yyextra;
				void *ctx = state;	
				yylval->identifier = ralloc_strdup(ctx, yytext);
				return classify_identifier(state, yytext);
			}

.			{ return yytext[0]; }

%%

int
classify_identifier(struct _mesa_glsl_parse_state *state, const char *name)
{
   if (state->symbols->get_variable(name) || state->symbols->get_function(name))
	  return IDENTIFIER;
   else if (state->symbols->get_type(name))
	  return TYPE_IDENTIFIER;
   else
	  return NEW_IDENTIFIER;
}

void
_mesa_hlsl_lexer_ctor(struct _mesa_glsl_parse_state *state, const char *string)
{
   yylex_init_extra(state, & state->scanner);
   yy_scan_string(string, state->scanner);
}

void
_mesa_hlsl_lexer_dtor(struct _mesa_glsl_parse_state *state)
{
   yylex_destroy(state->scanner);
}
