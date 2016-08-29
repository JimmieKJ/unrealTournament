// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "MetalShaderFormat.h"
#include "hlslcc.h"
#include "hlslcc_private.h"
#include "MetalBackend.h"
#include "MetalUtils.h"
#include "compiler.h"

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include "glsl_parser_extras.h"
PRAGMA_POP

#include "hash_table.h"
#include "ir_rvalue_visitor.h"
#include "PackUniformBuffers.h"
#include "IRDump.h"
#include "OptValueNumbering.h"
#include "ir_optimization.h"
#include "MetalUtils.h"

#if !PLATFORM_WINDOWS
#define _strdup strdup
#endif

#define GROUP_MEMORY_BARRIER						"GroupMemoryBarrier"
#define GROUP_MEMORY_BARRIER_WITH_GROUP_SYNC		"GroupMemoryBarrierWithGroupSync"
#define DEVICE_MEMORY_BARRIER					"DeviceMemoryBarrier"
#define DEVICE_MEMORY_BARRIER_WITH_GROUP_SYNC	"DeviceMemoryBarrierWithGroupSync"
#define ALL_MEMORY_BARRIER						"AllMemoryBarrier"
#define ALL_MEMORY_BARRIER_WITH_GROUP_SYNC		"AllMemoryBarrierWithGroupSync"

/**
 * This table must match the ir_expression_operation enum.
 */
static const char * const MetalExpressionTable[ir_opcode_count][4] =
	{
	{ "(~", ")", "", "" }, // ir_unop_bit_not,
	{ "not(", ")", "", "!" }, // ir_unop_logic_not,
	{ "(-", ")", "", "" }, // ir_unop_neg,
	{ "fabs(", ")", "", "" }, // ir_unop_abs,
	{ "sign(", ")", "", "" }, // ir_unop_sign,
	{ "(1.0/(", "))", "", "" }, // ir_unop_rcp,
	{ "rsqrt(", ")", "", "" }, // ir_unop_rsq,
	{ "sqrt(", ")", "", "" }, // ir_unop_sqrt,
	{ "exp(", ")", "", "" }, // ir_unop_exp,      /**< Log base e on gentype */
	{ "log(", ")", "", "" }, // ir_unop_log,	     /**< Natural log on gentype */
	{ "exp2(", ")", "", "" }, // ir_unop_exp2,
	{ "log2(", ")", "", "" }, // ir_unop_log2,
	{ "int(", ")", "", "" }, // ir_unop_f2i,      /**< Float-to-integer conversion. */
	{ "float(", ")", "", "" }, // ir_unop_i2f,      /**< Integer-to-float conversion. */
	{ "bool(", ")", "", "" }, // ir_unop_f2b,      /**< Float-to-boolean conversion */
	{ "float(", ")", "", "" }, // ir_unop_b2f,      /**< Boolean-to-float conversion */
	{ "bool(", ")", "", "" }, // ir_unop_i2b,      /**< int-to-boolean conversion */
	{ "int(", ")", "", "" }, // ir_unop_b2i,      /**< Boolean-to-int conversion */
	{ "uint(", ")", "", "" }, // ir_unop_b2u,
	{ "bool(", ")", "", "" }, // ir_unop_u2b,
	{ "uint(", ")", "", "" }, // ir_unop_f2u,
	{ "float(", ")", "", "" }, // ir_unop_u2f,      /**< Unsigned-to-float conversion. */
	{ "uint(", ")", "", "" }, // ir_unop_i2u,      /**< Integer-to-unsigned conversion. */
	{ "int(", ")", "", "" }, // ir_unop_u2i,      /**< Unsigned-to-integer conversion. */
	{ "int(", ")", "", "" }, // ir_unop_h2i,
	{ "half(", ")", "", "" }, // ir_unop_i2h,
	{ "float(", ")", "", "" }, // ir_unop_h2f,
	{ "half(", ")", "", "" }, // ir_unop_f2h,
	{ "bool(", ")", "", "" }, // ir_unop_h2b,
	{ "float(", ")", "", "" }, // ir_unop_b2h,
	{ "uint(", ")", "", "" }, // ir_unop_h2u,
	{ "uint(", ")", "", "" }, // ir_unop_u2h,
	{ "transpose(", ")", "", "" }, // ir_unop_transpose
	{ "any(", ")", "", "" }, // ir_unop_any,
	{ "all(", ")", "", "" }, // ir_unop_all,

	/**
	* \name Unary floating-point rounding operations.
	*/
	/*@{*/
	{ "trunc(", ")", "", "" }, // ir_unop_trunc,
	{ "ceil(", ")", "", "" }, // ir_unop_ceil,
	{ "floor(", ")", "", "" }, // ir_unop_floor,
	{ "fract(", ")", "", "" }, // ir_unop_fract,
	{ "round(", ")", "", "" }, // ir_unop_round,
	/*@}*/

	/**
	* \name Trigonometric operations.
	*/
	/*@{*/
	{ "sin(", ")", "", "" }, // ir_unop_sin,
	{ "cos(", ")", "", "" }, // ir_unop_cos,
	{ "tan(", ")", "", "" }, // ir_unop_tan,
	{ "asin(", ")", "", "" }, // ir_unop_asin,
	{ "acos(", ")", "", "" }, // ir_unop_acos,
	{ "atan(", ")", "", "" }, // ir_unop_atan,
	{ "sinh(", ")", "", "" }, // ir_unop_sinh,
	{ "cosh(", ")", "", "" }, // ir_unop_cosh,
	{ "tanh(", ")", "", "" }, // ir_unop_tanh,
	/*@}*/

	/**
	* \name Normalize.
	*/
	/*@{*/
	{ "normalize(", ")", "", "" }, // ir_unop_normalize,
	/*@}*/

	/**
	* \name Partial derivatives.
	*/
	/*@{*/
	{ "dfdx(", ")", "", "" }, // ir_unop_dFdx,
	{ "dfdy(", ")", "", "" }, // ir_unop_dFdy,
	/*@}*/

	{ "isnan(", ")", "", "" }, // ir_unop_isnan,
	{ "isinf(", ")", "", "" }, // ir_unop_isinf,

	{ "floatBitsToUint(", ")", "", "" }, // ir_unop_fasu,
	{ "floatBitsToInt(", ")", "", "" }, // ir_unop_fasi,
	{ "intBitsToFloat(", ")", "", "" }, // ir_unop_iasf,
	{ "uintBitsToFloat(", ")", "", "" }, // ir_unop_uasf,

	{ "bitfieldReverse(", ")", "", "" }, // ir_unop_bitreverse,
	{ "bitCount(", ")", "", "" }, // ir_unop_bitcount,
	{ "findMSB(", ")", "", "" }, // ir_unop_msb,
	{ "findLSB(", ")", "", "" }, // ir_unop_lsb,

	{ "ERROR_NO_NOISE_FUNCS(", ")", "", "" }, // ir_unop_noise,

	{ "(", "+", ")", "" }, // ir_binop_add,
	{ "(", "-", ")", "" }, // ir_binop_sub,
	{ "(", "*", ")", "" }, // ir_binop_mul,
	{ "(", "/", ")", "" }, // ir_binop_div,

	/**
	* Takes one of two combinations of arguments:
	*
	* - mod(vecN, vecN)
	* - mod(vecN, float)
	*
	* Does not take integer types.
	*/
	{ "fmod(", ",", ")", "%" }, // ir_binop_mod,
	{ "modf(", ",", ")", "" }, // ir_binop_modf,

	{ "step(", ",", ")", "" }, // ir_binop_step,

	/**
	* \name Binary comparison operators which return a boolean vector.
	* The type of both operands must be equal.
	*/
	/*@{*/
	{ "(", "<", ")", "<" }, // ir_binop_less,
	{ "(", ">", ")", ">" }, // ir_binop_greater,
	{ "(", "<=", ")", "<=" }, // ir_binop_lequal,
	{ "(", ">=", ")", ">=" }, // ir_binop_gequal,
	{ "(", "==", ")", "==" }, // ir_binop_equal,
	{ "(", "!=", ")", "!=" }, // ir_binop_nequal,
	/**
	* Returns single boolean for whether all components of operands[0]
	* equal the components of operands[1].
	*/
	{ "(", "==", ")", "" }, // ir_binop_all_equal,
	/**
	* Returns single boolean for whether any component of operands[0]
	* is not equal to the corresponding component of operands[1].
	*/
	{ "(", "!=", ")", "" }, // ir_binop_any_nequal,
	/*@}*/

	/**
	* \name Bit-wise binary operations.
	*/
	/*@{*/
	{ "(", "<<", ")", "" }, // ir_binop_lshift,
	{ "(", ">>", ")", "" }, // ir_binop_rshift,
	{ "(", "&", ")", "" }, // ir_binop_bit_and,
	{ "(", "^", ")", "" }, // ir_binop_bit_xor,
	{ "(", "|", ")", "" }, // ir_binop_bit_or,
	/*@}*/

	{ "bool%d(uint%d(", ")*uint%d(", "))", "&&" }, // ir_binop_logic_and,
	{ "bool%d(abs(int%d(", ")+int%d(", ")))", "^^" }, // ir_binop_logic_xor,
	{ "bool%d(uint%d(", ")+uint%d(", "))", "||" }, // ir_binop_logic_or,

	{ "dot(", ",", ")", "" }, // ir_binop_dot,
	{ "cross(", ",", ")", "" }, // ir_binop_cross,
	{ "fmin(", ",", ")", "" }, // ir_binop_min,
	{ "fmax(", ",", ")", "" }, // ir_binop_max,
	{ "atan2(", ",", ")", "" },
	{ "pow(", ",", ")", "" }, // ir_binop_pow,

	{ "mix(", ",", ",", ")" }, // ir_ternop_lerp,
	{ "smoothstep(", ",", ",", ")" }, // ir_ternop_smoothstep,
	{ "clamp(", ",", ",", ")" }, // ir_ternop_clamp,

	{ "ERROR_QUADOP_VECTOR(", ",", ")" }, // ir_quadop_vector,
};

static_assert((sizeof(MetalExpressionTable) / sizeof(MetalExpressionTable[0])) == ir_opcode_count, "Metal Expression Table Size Mismatch");

struct SDMARange
{
	unsigned SourceCB;
	unsigned SourceOffset;
	unsigned Size;
	unsigned DestCBIndex;
	unsigned DestCBPrecision;
	unsigned DestOffset;

	bool operator <(SDMARange const & Other) const
	{
		if (SourceCB == Other.SourceCB)
		{
			return SourceOffset < Other.SourceOffset;
		}

		return SourceCB < Other.SourceCB;
	}
};
typedef std::list<SDMARange> TDMARangeList;
typedef std::map<unsigned, TDMARangeList> TCBDMARangeMap;


static void InsertRange( TCBDMARangeMap& CBAllRanges, unsigned SourceCB, unsigned SourceOffset, unsigned Size, unsigned DestCBIndex, unsigned DestCBPrecision, unsigned DestOffset ) 
{
	check(SourceCB < (1 << 12));
	check(DestCBIndex < (1 << 12));
	check(DestCBPrecision < (1 << 8));
	unsigned SourceDestCBKey = (SourceCB << 20) | (DestCBIndex << 8) | DestCBPrecision;
	SDMARange Range = { SourceCB, SourceOffset, Size, DestCBIndex, DestCBPrecision, DestOffset };

	TDMARangeList& CBRanges = CBAllRanges[SourceDestCBKey];
//printf("* InsertRange: %08x\t%u:%u - %u:%c:%u:%u\n", SourceDestCBKey, SourceCB, SourceOffset, DestCBIndex, DestCBPrecision, DestOffset, Size);
	if (CBRanges.empty())
	{
		CBRanges.push_back(Range);
	}
	else
	{
		TDMARangeList::iterator Prev = CBRanges.end();
		bool bAdded = false;
		for (auto Iter = CBRanges.begin(); Iter != CBRanges.end(); ++Iter)
		{
			if (SourceOffset + Size <= Iter->SourceOffset)
			{
				if (Prev == CBRanges.end())
				{
					CBRanges.push_front(Range);
				}
				else
				{
					CBRanges.insert(Iter, Range);
				}

				bAdded = true;
				break;
			}

			Prev = Iter;
		}

		if (!bAdded)
		{
			CBRanges.push_back(Range);
		}

		if (CBRanges.size() > 1)
		{
			// Try to merge ranges
			bool bDirty = false;
			do
			{
				bDirty = false;
				TDMARangeList NewCBRanges;
				for (auto Iter = CBRanges.begin(); Iter != CBRanges.end(); ++Iter)
				{
					if (Iter == CBRanges.begin())
					{
						Prev = CBRanges.begin();
					}
					else
					{
						if (Prev->SourceOffset + Prev->Size == Iter->SourceOffset && Prev->DestOffset + Prev->Size == Iter->DestOffset)
						{
							SDMARange Merged = *Prev;
							Merged.Size = Prev->Size + Iter->Size;
							NewCBRanges.pop_back();
							NewCBRanges.push_back(Merged);
							++Iter;
							NewCBRanges.insert(NewCBRanges.end(), Iter, CBRanges.end());
							bDirty = true;
							break;
						}
					}

					NewCBRanges.push_back(*Iter);
					Prev = Iter;
				}

				CBRanges.swap(NewCBRanges);
			}
			while (bDirty);
		}
	}
}

static TDMARangeList SortRanges( TCBDMARangeMap& CBRanges ) 
{
	TDMARangeList Sorted;
	for (auto Iter = CBRanges.begin(); Iter != CBRanges.end(); ++Iter)
	{
		Sorted.insert(Sorted.end(), Iter->second.begin(), Iter->second.end());
	}

	Sorted.sort();

	return Sorted;
}

static void DumpSortedRanges(TDMARangeList& SortedRanges)
{
	printf("**********************************\n");
	for (auto i = SortedRanges.begin(); i != SortedRanges.end(); ++i )
	{
		auto o = *i;
		printf("\t%u:%u - %u:%c:%u:%u\n", o.SourceCB, o.SourceOffset, o.DestCBIndex, o.DestCBPrecision, o.DestOffset, o.Size);
	}
}

/**
 * IR visitor used to generate Metal. Based on ir_print_visitor.
 */
class FGenerateMetalVisitor : public ir_visitor
{
	FMetalCodeBackend* Backend;
	_mesa_glsl_parse_state* ParseState;

	/** Track which multi-dimensional arrays are used. */
	struct md_array_entry : public exec_node
	{
		const glsl_type* type;
	};

public:
	/** External variables. */
	exec_list input_variables;
protected:
	exec_list output_variables;
	exec_list uniform_variables;
	exec_list sampler_variables;
	exec_list image_variables;

	/** Attribute [numthreads(X,Y,Z)] */
	int32 NumThreadsX;
	int32 NumThreadsY;
	int32 NumThreadsZ;

	/** Track global instructions. */
	struct global_ir : public exec_node
	{
		ir_instruction* ir;
		explicit global_ir(ir_instruction* in_ir) : ir(in_ir) {}
	};

	/** Global instructions. */
	exec_list global_instructions;

	/** A mapping from ir_variable * -> unique printable names. */
	hash_table *printable_names;
	/** Structures required by the code. */
	hash_table *used_structures;
	/** Uniform block variables required by the code. */
	hash_table *used_uniform_blocks;
	/** Multi-dimensional arrays required by the code. */

	// Code generation flags
	_mesa_glsl_parser_targets Frequency;

	FBuffers& Buffers;

	/** Memory context within which to make allocations. */
	void *mem_ctx;
	/** Buffer to which GLSL source is being generated. */
	char** buffer;
	/** Indentation level. */
	int indentation;
	/** Scope depth. */
	int scope_depth;
	// Expression Depth
	int ExpressionDepth;
	/** The number of temporary variables declared in the current scope. */
	int temp_id;
	/** The number of global variables declared. */
	int global_id;
	/** Whether a semicolon must be printed before the next EOL. */
	bool needs_semicolon;
	bool IsMain;
	/**
	 * Whether uint literals should be printed as int literals. This is a hack
	 * because glCompileShader crashes on Mac OS X with code like this:
	 * foo = bar[0u];
	 */
	bool should_print_uint_literals_as_ints;
	/** number of loops in the generated code */
	int loop_count;

	// Only one stage_in is allowed
	bool bStageInEmitted;

	// Are we in the middle of a function signature?
	//bool bIsFunctionSig = false;

	// Use packed_ prefix when printing out structs
	bool bUsePacked;

	// Do we need to add #include <compute_shaders>
	bool bNeedsComputeInclude;

	/**
	 * Fetch/generate a unique name for ir_variable.
	 *
	 * GLSL IR permits multiple ir_variables to share the same name.  This works
	 * fine until we try to print it, when we really need a unique one.
	 */
	const char *unique_name(ir_variable *var)
	{
		if (var->mode == ir_var_temporary || var->mode == ir_var_auto)
		{
			/* Do we already have a name for this variable? */
			const char *name = (const char *) hash_table_find(this->printable_names, var);
			if (name == NULL)
			{
				bool bIsGlobal = (scope_depth == 0 && var->mode != ir_var_temporary);
				const char* prefix = "g";
				if (!bIsGlobal)
				{
					if (var->type->is_matrix())
					{
						prefix = "m";
					}
					else if (var->type->is_vector())
					{
						prefix = "v";
					}
					else
					{
						switch (var->type->base_type)
						{
						case GLSL_TYPE_BOOL: prefix = "b"; break;
						case GLSL_TYPE_UINT: prefix = "u"; break;
						case GLSL_TYPE_INT: prefix = "i"; break;
						case GLSL_TYPE_HALF: prefix = "h"; break;
						case GLSL_TYPE_FLOAT: prefix = "f"; break;
						default: prefix = "t"; break;
						}
					}
				}
				int var_id = bIsGlobal ? global_id++ : temp_id++;
				name = ralloc_asprintf(mem_ctx, "%s%d", prefix, var_id);
				hash_table_insert(this->printable_names, (void *)name, var);
			}
			return name;
		}

		/* If there's no conflict, just use the original name */
		return var->name;
	}

	/**
	 * Add tabs/spaces for the current indentation level.
	 */
	void indent()
	{
		for (int i = 0; i < indentation; i++)
		{
			ralloc_asprintf_append(buffer, "\t");
		}
	}

	/**
	 * Print the base type, e.g. vec3.
	 */
	void print_base_type(const glsl_type *t)
	{
		if (t->base_type == GLSL_TYPE_ARRAY)
		{
			bool bPrevPacked = bUsePacked;
			if (t->element_type()->is_vector() && t->element_type()->vector_elements == 3)
			{
				bUsePacked = false;
			}
			print_base_type(t->fields.array);
			bUsePacked = bPrevPacked;
		}
		else if (t->base_type == GLSL_TYPE_INPUTPATCH)
		{
			ralloc_asprintf_append(buffer, "GLSL_TYPE_INPUTPATCH");
			check(0);
			ralloc_asprintf_append(buffer, "/* %s */ ", t->name);
			print_base_type(t->inner_type);
		}
		else if (t->base_type == GLSL_TYPE_OUTPUTPATCH)
		{
			ralloc_asprintf_append(buffer, "GLSL_TYPE_OUTPUTPATCH");
			check(0);
			ralloc_asprintf_append(buffer, "/* %s */ ", t->name);
			print_base_type(t->inner_type);
		}
		else if ((t->base_type == GLSL_TYPE_STRUCT)
				&& (strncmp("gl_", t->name, 3) != 0))
		{
			ralloc_asprintf_append(buffer, "%s", t->name);
		}
		else if (t->base_type == GLSL_TYPE_SAMPLER && t->sampler_buffer) 
		{
			// Typed buffer read
			check(t->inner_type);
			print_base_type(t->inner_type);
		}
		else if (t->base_type == GLSL_TYPE_IMAGE)
		{
			// Do nothing...
		}
		else
		{
			if (t->base_type == GLSL_TYPE_SAMPLER)
			{
				bool bDone = false;
				if (t->sampler_dimensionality == GLSL_SAMPLER_DIM_2D && t->sampler_array)
				{
					ralloc_asprintf_append(buffer, t->sampler_shadow ? "depth2d_array" : "texture2d_array");
					bDone = true;
				}
				else if (t->sampler_dimensionality == GLSL_SAMPLER_DIM_CUBE && t->sampler_array)
				{
					ralloc_asprintf_append(buffer, "texturecube_array");
					bDone = true;
				}
				else if (t->sampler_dimensionality == GLSL_SAMPLER_DIM_2D && t->sampler_ms)
				{
					ralloc_asprintf_append(buffer, t->sampler_shadow ? "depth2d_ms" : "texture2d_ms");
					bDone = true;
				}
				else if (t->HlslName)
				{
					if (!strcmp(t->HlslName, "texture2d") && t->sampler_shadow)
					{
						ralloc_asprintf_append(buffer, "depth2d");
						bDone = true;
					}
					else if (!strcmp(t->HlslName, "texturecube") && t->sampler_shadow)
					{
						ralloc_asprintf_append(buffer, "depthcube");
						bDone = true;
					}
				}

				if (!bDone)
				{
					ralloc_asprintf_append(buffer, "%s", t->HlslName ? t->HlslName : "UnsupportedSamplerType");
				}
			}
			else
			{
				check(t->HlslName);
				if (bUsePacked && t->is_vector() && t->vector_elements < 4)
				{
					ralloc_asprintf_append(buffer, "packed_%s", t->HlslName);
				}
				else
				{
					ralloc_asprintf_append(buffer, "%s", t->HlslName);
				}
			}
		}
	}

	/**
	 * Print the portion of the type that appears before a variable declaration.
	 */
	void print_type_pre(const glsl_type *t)
	{
		print_base_type(t);
	}

	/**
	 * Print the portion of the type that appears after a variable declaration.
	 */
	void print_type_post(const glsl_type *t)
	{
		if (t->base_type == GLSL_TYPE_ARRAY)
		{
			do 
			{
				ralloc_asprintf_append(buffer, "[%u]", t->length);
				t = t->element_type();
			}
			while (t && t->base_type == GLSL_TYPE_ARRAY);
		}
		else if (t->base_type == GLSL_TYPE_INPUTPATCH || t->base_type == GLSL_TYPE_OUTPUTPATCH)
		{
			ralloc_asprintf_append(buffer, "[%u] /* %s */", t->patch_length, t->name);
		}
	}

	/**
	 * Print a full variable declaration.
	 */
	void print_type_full(const glsl_type *t)
	{
		print_type_pre(t);
		print_type_post(t);
	}

	/**
	 * Visit a single instruction. Appends a semicolon and EOL if needed.
	 */
	void do_visit(ir_instruction* ir)
	{
		needs_semicolon = true;
		ir->accept(this);
		if (needs_semicolon)
		{
			ralloc_asprintf_append(buffer, ";\n");
		}
	}

	/**
	* \name Visit methods
	*
	* As typical for the visitor pattern, there must be one \c visit method for
	* each concrete subclass of \c ir_instruction.  Virtual base classes within
	* the hierarchy should not have \c visit methods.
	*/

	virtual void visit(ir_rvalue *rvalue) override
	{
		check(0 && "ir_rvalue not handled for GLSL export.");
	}

	void print_zero_initialiser(const glsl_type * type)
	{
		check(type->base_type != GLSL_TYPE_STRUCT);
		{
			if (type->base_type != GLSL_TYPE_ARRAY)
			{
				ir_constant* zero = ir_constant::zero(mem_ctx, type);
				if (zero)
				{
					zero->accept(this);
				}
			}
			else
			{
				ralloc_asprintf_append(buffer, "{");
				
				for (uint32 i = 0; i < type->length; i++)
				{
					if (i > 0)
					{
						ralloc_asprintf_append(buffer, ", ");
					}
					print_zero_initialiser(type->element_type());
				}
				
				ralloc_asprintf_append(buffer, "}");
			}
		}
	}

	virtual void visit(ir_variable *var) override
	{
		// Check for an initialized const variable
		// If var is read-only and initialized, set it up as an initialized const
		bool constInit = false;
		if (var->has_initializer && var->read_only && (var->constant_initializer || var->constant_value))
		{
			ralloc_asprintf_append( buffer, "const ");
			constInit = true;
		}

		if (scope_depth == 0)
		{
			check(0);
		}

		if (scope_depth == 0 && var->mode == ir_var_temporary)
		{
			check(0);
		}
		else
		{
			if (scope_depth == 0 &&
			   ((var->mode == ir_var_in) || (var->mode == ir_var_out)) && 
			   var->is_interface_block)
			{
				check(0);
			}
			else if (var->type->is_image())
			{
				auto* PtrType = var->type->is_array() ? var->type->element_type() : var->type;
				check(!PtrType->is_array() && PtrType->inner_type);

				// Buffer
				int BufferIndex = Buffers.GetIndex(var, Backend->bIsDesktop);
				check(BufferIndex >= 0);
				if (var->type->sampler_buffer)
				{
                    check(BufferIndex <= 30);
					ralloc_asprintf_append(
						buffer,
						"device "
						);
					if (Buffers.AtomicVariables.find(var) != Buffers.AtomicVariables.end())
					{
						ralloc_asprintf_append(buffer, "atomic_");
					}
					print_type_pre(PtrType->inner_type);
					ralloc_asprintf_append(buffer, " *%s", unique_name(var));
					print_type_post(PtrType->inner_type);
					ralloc_asprintf_append(
						buffer,
						" [[ buffer(%d) ]]", BufferIndex
						);
				}
				else
				{
					check(PtrType->inner_type->is_numeric());
					auto* Found = strstr(PtrType->name, "image");
					check(Found);
					Found += 5;	//strlen("image")
					char Temp[16];
					char* TempPtr = Temp;
					do
					{
						*TempPtr++ = tolower(*Found);
						++Found;
					}
					while (*Found);
					*TempPtr = 0;
					ralloc_asprintf_append(buffer, "texture%s<", Temp);
					// UAVs require type per channel, not including # of channels
					print_type_pre(PtrType->inner_type->get_scalar_type());
					ralloc_asprintf_append(buffer, ", access::write> %s", unique_name(var));
					ralloc_asprintf_append(
						buffer,
						" [[ texture(%d) ]]", BufferIndex
						);
				}
			}
			else
			{
				if (IsMain && var->type->base_type == GLSL_TYPE_STRUCT && (var->mode == ir_var_in || var->mode == ir_var_out || var->mode == ir_var_uniform))
				{
					if (hash_table_find(used_structures, var->type) == NULL)
					{
						hash_table_insert(used_structures, (void*)var->type, var->type);
					}
				}

				if (IsMain && var->mode == ir_var_uniform)
				{
					auto* PtrType = var->type->is_array() ? var->type->element_type() : var->type;
					check(!PtrType->is_array());
					if (var->type->is_sampler())
					{
						if (var->type->sampler_buffer)
						{
							// Buffer
							int BufferIndex = Buffers.GetIndex(var, Backend->bIsDesktop);
							check(BufferIndex >= 0 && BufferIndex <= 30);
							ralloc_asprintf_append(
								buffer,
								"const device "
								);
							print_type_pre(PtrType);
							ralloc_asprintf_append(buffer, " *%s", unique_name(var));
							print_type_post(PtrType);
							ralloc_asprintf_append(
								buffer,
								" [[ buffer(%d) ]]", BufferIndex
								);
						}
						else
						{
							// Regular textures
							auto* Entry = ParseState->FindPackedSamplerEntry(var->name);
							check(Entry);
							//@todo-rco: SamplerStates
							auto SamplerStateFound = ParseState->TextureToSamplerMap.find(Entry->Name.c_str());
							if (SamplerStateFound != ParseState->TextureToSamplerMap.end())
							{
								auto& SamplerStates = SamplerStateFound->second;
								for (auto& SamplerState : SamplerStates)
								{
									bool bAdded = false;
									int32 SamplerStateIndex = Buffers.GetUniqueSamplerStateIndex(SamplerState, true, bAdded);
									if (bAdded)
									{
										ralloc_asprintf_append(
											buffer,
											"sampler s%d [[ sampler(%d) ]], ", SamplerStateIndex, SamplerStateIndex);
									}
								}
							}

							print_type_pre(PtrType);
							const char* InnerType = "float";
							if (PtrType->inner_type)
							{
								if (PtrType->base_type == GLSL_TYPE_SAMPLER && PtrType->sampler_shadow)
								{
									//#todo-rco: Currently force to float...
								}
								else
								{
									switch (PtrType->inner_type->base_type)
									{
									case GLSL_TYPE_HALF:
										InnerType = "half";
										break;
									case GLSL_TYPE_INT:
										InnerType = "int";
										break;
									case GLSL_TYPE_UINT:
										InnerType = "uint";
										break;
									default:
										break;
									}
								}
							}
							ralloc_asprintf_append(
								buffer,
								"<%s> %s", InnerType, unique_name(var));
							print_type_post(PtrType);
							ralloc_asprintf_append(
								buffer,
								" [[ texture(%u) ]]", Entry->offset
								);
						}
					}
					else
					{
						int BufferIndex = Buffers.GetIndex(var, Backend->bIsDesktop);
						bool bNeedsPointer = (var->semantic && (strlen(var->semantic) == 1));
						check(BufferIndex >= 0 && BufferIndex <= 30);
						ralloc_asprintf_append(
							buffer,
							"constant "
							);
						print_type_pre(PtrType);
						ralloc_asprintf_append(buffer, " %s%s", bNeedsPointer ? "*" : "&", unique_name(var));
						print_type_post(PtrType);
						ralloc_asprintf_append(
							buffer,
							" [[ buffer(%d) ]]", BufferIndex
							);
					}
				}
				else if (IsMain && var->mode == ir_var_in)
				{
					if (!strcmp(var->name, "gl_FrontFacing"))
					{
						check(var->type->is_boolean());
						print_type_pre(var->type);
						ralloc_asprintf_append(buffer, " %s", unique_name(var));
						print_type_post(var->type);
						ralloc_asprintf_append(
							buffer,
							" [[ front_facing ]]"
							);
					}
					else if (var->semantic && !strncmp(var->semantic, "[[ color(", 9))
					{
						check(var->type->is_vector() && var->type->vector_elements == 4);
						print_type_pre(var->type);
						ralloc_asprintf_append(buffer, " %s", unique_name(var));
						print_type_post(var->type);
						ralloc_asprintf_append(
							buffer,
							" %s",
							var->semantic
							);
					}
					else if(var->semantic && !strncmp(var->semantic,"[[", 2))
					{
						check(!var->type->is_record());
						print_type_pre(var->type);
						ralloc_asprintf_append(buffer," %s",unique_name(var));
						print_type_post(var->type);
						ralloc_asprintf_append(
							buffer,
							" %s",
							var->semantic
							);
					}
					else
					{
						check(var->type->is_record());
						check(!bStageInEmitted);
						print_type_pre(var->type);
						ralloc_asprintf_append(buffer, " %s", unique_name(var));
						print_type_post(var->type);
						ralloc_asprintf_append(
							buffer,
							" [[ stage_in ]]"
							);
						bStageInEmitted = true;
					}
				}
				else if (IsMain && var->mode == ir_var_out)
				{
					auto* PtrType = var->type->is_array() ? var->type->element_type() : var->type;
					check(!PtrType->is_array());
					print_type_pre(PtrType);
					ralloc_asprintf_append(buffer, " %s", unique_name(var));
					print_type_post(PtrType);
				}
				else
				{
					if (var->mode == ir_var_shared)
					{
						ralloc_asprintf_append(buffer, "threadgroup ");
					}

					if (Buffers.AtomicVariables.find(var) != Buffers.AtomicVariables.end())
					{
						ralloc_asprintf_append(buffer, "atomic_");
					}

					print_type_pre(var->type);
					ralloc_asprintf_append(buffer, " %s", unique_name(var));
					print_type_post(var->type);
				}
			}
		}

		// Add the initializer if we need it
		if (constInit)
		{
			ralloc_asprintf_append(buffer, " = ");
			if (var->constant_initializer)
			{
				var->constant_initializer->accept(this);
			}
			else
			{
				var->constant_value->accept(this);
			}
		}
		else if ((Backend && Backend->bZeroInitialise) && (var->type->base_type != GLSL_TYPE_STRUCT) && (var->mode == ir_var_auto || var->mode == ir_var_temporary || var->mode == ir_var_shared) && (Buffers.AtomicVariables.find(var) == Buffers.AtomicVariables.end()))
		{
			// @todo UE-34355 temporary workaround for 10.12 shader compiler error - really all arrays should be zero'd but only threadgroup shared initialisation works on the Beta drivers.
			if (var->type->base_type != GLSL_TYPE_ARRAY || var->mode == ir_var_shared)
			{
				ralloc_asprintf_append(buffer, " = ");
				print_zero_initialiser(var->type);
			}
		}
	}

	virtual void visit(ir_function_signature *sig) override
	{
		// Reset temporary id count.
		temp_id = 0;
		bool bPrintComma = false;
		scope_depth++;
		IsMain = sig->is_main;

		print_type_full(sig->return_type);
		ralloc_asprintf_append(buffer, " %s(", sig->function_name());
		
        if (sig->is_main && Backend && Backend->bBoundsChecks)
		{
            bool bInsertSideTable = false;
            foreach_iter(exec_list_iterator, iter, sig->parameters)
            {
                ir_variable *const inst = (ir_variable *) iter.get();
                bInsertSideTable |= ((inst->type->is_image() || inst->type->sampler_buffer) && inst->used);
            }
            if (bInsertSideTable)
            {
                ir_variable* BufferSizes = new(ParseState)ir_variable(glsl_type::uint_type, "BufferSizes", ir_var_uniform);
                BufferSizes->semantic = "u";
                Buffers.Buffers.Add(BufferSizes);
                sig->parameters.push_head(BufferSizes);
            }
		}

		foreach_iter(exec_list_iterator, iter, sig->parameters)
		{
			ir_variable *const inst = (ir_variable *) iter.get();
			if (bPrintComma)
			{
				ralloc_asprintf_append(buffer, ",\n");
				++indentation;
				indent();
				--indentation;
			}
			inst->accept(this);
			bPrintComma = true;
		}
		ralloc_asprintf_append(buffer, ")\n");

		indent();
		ralloc_asprintf_append(buffer, "{\n");

		if (sig->is_main && !global_instructions.is_empty())
		{
			indentation++;
			foreach_iter(exec_list_iterator, iter, global_instructions)
			{
				global_ir* gir = (global_ir*)iter.get();
				indent();
				do_visit(gir->ir);
			}
			indentation--;
		}

		// Copy the global attributes
		if (sig->is_main)
		{
			NumThreadsX = sig->wg_size_x;
			NumThreadsY = sig->wg_size_y;
			NumThreadsZ = sig->wg_size_z;
		}

		indentation++;
		foreach_iter(exec_list_iterator, iter, sig->body)
		{
			ir_instruction *const inst = (ir_instruction *) iter.get();
			indent();
			do_visit(inst);
		}
		indentation--;
		indent();
		ralloc_asprintf_append(buffer, "}\n");
		needs_semicolon = false;
		IsMain = false;
		scope_depth--;
	}

	virtual void visit(ir_function *func) override
	{
		foreach_iter(exec_list_iterator, iter, *func)
		{
			ir_function_signature *const sig = (ir_function_signature *) iter.get();
			if (sig->is_defined && !sig->is_builtin)
			{
				indent();
				if (sig->is_main)
				{
					switch (Frequency)
					{
					case vertex_shader:
						ralloc_asprintf_append(buffer, "vertex ");
						break;
					case fragment_shader:
						ralloc_asprintf_append(buffer, "fragment ");
						break;
					case compute_shader:
						ralloc_asprintf_append(buffer, "kernel ");
						break;
					default:
						check(0);
						break;
					}
				}

				sig->accept(this);
			}
		}
		needs_semicolon = false;
	}

	virtual void visit(ir_expression *expr) override
	{
		check(scope_depth > 0);
		++ExpressionDepth;

		int numOps = expr->get_num_operands();
		ir_expression_operation op = expr->operation;

		if (op == ir_unop_rcp)
		{
			check(numOps == 1);

			FCustomStdString Type = expr->type->name;
			Type = FixVecPrefix(Type);

			ralloc_asprintf_append(buffer, "(%s(1.0) / ", Type.c_str());
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else if (op >= ir_unop_fasu && op <= ir_unop_uasf)
		{
			ralloc_asprintf_append(buffer, "as_type<");
			print_type_full(expr->type);
			ralloc_asprintf_append(buffer, ">(");
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else if (numOps == 1 && op >= ir_unop_first_conversion && op <= ir_unop_last_conversion)
		{
			FCustomStdString Type = expr->type->name;
			Type = FixVecPrefix(Type);

			ralloc_asprintf_append(buffer, "%s(", Type.c_str());
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else if (expr->type->is_scalar() &&
			((numOps == 1 && op == ir_unop_logic_not) ||
			 (numOps == 2 && op >= ir_binop_first_comparison && op <= ir_binop_last_comparison) ||
			 (numOps == 2 && op >= ir_binop_first_logic && op <= ir_binop_last_logic)))
		{
			const char* op_str = MetalExpressionTable[op][3];
			ralloc_asprintf_append(buffer, "%s%s",
				(numOps == 1) ? op_str : "",
				(ExpressionDepth > 1) ? "(" : "");
			expr->operands[0]->accept(this);
			if (numOps == 2)
			{
				ralloc_asprintf_append(buffer, "%s", op_str);
				expr->operands[1]->accept(this);
			}
			ralloc_asprintf_append(buffer, (ExpressionDepth > 1) ? ")" : "");
		}
		else if (expr->type->is_vector() && numOps == 2 &&
			op >= ir_binop_first_logic && op <= ir_binop_last_logic)
		{
			ralloc_asprintf_append(buffer, MetalExpressionTable[op][0], expr->type->vector_elements, expr->type->vector_elements);
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, MetalExpressionTable[op][1], expr->type->vector_elements);
			expr->operands[1]->accept(this);
			ralloc_asprintf_append(buffer, MetalExpressionTable[op][2]);
		}
		else if (op == ir_binop_mod && !expr->type->is_float())
		{
			ralloc_asprintf_append(buffer, "((");
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ")%%(");
			expr->operands[1]->accept(this);
			ralloc_asprintf_append(buffer, "))");
		}
		else if (op == ir_binop_mul && expr->type->is_matrix()
			&& expr->operands[0]->type->is_matrix()
			&& expr->operands[1]->type->is_matrix())
		{
			ralloc_asprintf_append(buffer, "ERRROR_MulMatrix()");
			check(0);
		}
		else if ((op == ir_ternop_clamp || op == ir_unop_sqrt || op == ir_unop_rsq) && expr->type->base_type == GLSL_TYPE_FLOAT)
		{
			ralloc_asprintf_append(buffer, "precise::%s", MetalExpressionTable[op][0]);
			for (int i = 0; i < numOps; ++i)
			{
				expr->operands[i]->accept(this);
				ralloc_asprintf_append(buffer, MetalExpressionTable[op][i+1]);
			}
		}
		else if (numOps == 2 && (op == ir_binop_max || op == ir_binop_min))
		{
			// Convert fmax/fmin to max/min when dealing with integers
			auto* OpString = MetalExpressionTable[op][0];
			check(OpString[0] == 'f');
			
			if(expr->type->is_integer())
			{
				OpString = (OpString + 1);
			}
			else if(expr->type->base_type == GLSL_TYPE_FLOAT)
			{
				ralloc_asprintf_append(buffer, "precise::");
			}
			
			ralloc_asprintf_append(buffer, OpString);
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, MetalExpressionTable[op][1]);
			expr->operands[1]->accept(this);
			ralloc_asprintf_append(buffer, MetalExpressionTable[op][2]);
		}
		else if (numOps == 2 && op == ir_binop_dot)
		{
			auto* OpString = MetalExpressionTable[op][0];
			
			if (expr->operands[0]->type->is_scalar() && expr->operands[1]->type->is_scalar())
			{
				ralloc_asprintf_append(buffer, "(");
				expr->operands[0]->accept(this);
				ralloc_asprintf_append(buffer, "*");
				expr->operands[1]->accept(this);
				ralloc_asprintf_append(buffer, ")");
			}
			else
			{
				ralloc_asprintf_append(buffer, OpString);
				expr->operands[0]->accept(this);
				ralloc_asprintf_append(buffer, MetalExpressionTable[op][1]);
				expr->operands[1]->accept(this);
				ralloc_asprintf_append(buffer, MetalExpressionTable[op][2]);
			}
		}
		else if (op == ir_unop_lsb && numOps == 1)
		{
			ralloc_asprintf_append(buffer, "ctz(");
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else if (op == ir_unop_msb && numOps == 1)
		{
			ralloc_asprintf_append(buffer, "clz(");
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else if (op == ir_unop_bitcount && numOps == 1)
		{
			ralloc_asprintf_append(buffer, "popcount(");
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else if (numOps < 4)
		{
			ralloc_asprintf_append(buffer, MetalExpressionTable[op][0]);
			for (int i = 0; i < numOps; ++i)
			{
				expr->operands[i]->accept(this);
				ralloc_asprintf_append(buffer, MetalExpressionTable[op][i+1]);
			}
		}

		--ExpressionDepth;
	}

	virtual void visit(ir_texture *tex) override
	{
		check(scope_depth > 0);
		bool bNeedsClosingParenthesis = true;
		if (tex->op == ir_txs)
		{
			ralloc_asprintf_append(buffer, "int2((int)");
		}

		if (tex->op != ir_txf)
		{
			tex->sampler->accept(this);
		}
		switch (tex->op)
		{
		case ir_tex:
		case ir_txl:
		case ir_txb:
		case ir_txd:
		{
			ralloc_asprintf_append(buffer, tex->shadow_comparitor ? ".sample_compare(" : ".sample(");
			auto* Texture = tex->sampler->variable_referenced();
			check(Texture);
			auto* Entry = ParseState->FindPackedSamplerEntry(Texture->name);
			bool bDummy;
			int32 SamplerStateIndex = Buffers.GetUniqueSamplerStateIndex(tex->SamplerStateName, false, bDummy);
			check(SamplerStateIndex != INDEX_NONE);
			ralloc_asprintf_append(buffer, "s%d, ", SamplerStateIndex);
			if (tex->sampler->type->sampler_array)
			{
				// Need to split the coordinate
				ralloc_asprintf_append(buffer, "(");
				tex->coordinate->accept(this);
				
				char const* CoordSwizzle = "";
				char const* IndexSwizzle = "y";
				switch(tex->sampler->type->sampler_dimensionality)
				{
					case GLSL_SAMPLER_DIM_1D:
					{
						break;
					}
					case GLSL_SAMPLER_DIM_2D:
					case GLSL_SAMPLER_DIM_RECT:
					{
						CoordSwizzle = "y";
						IndexSwizzle = "z";
						break;
					}
					case GLSL_SAMPLER_DIM_3D:
					case GLSL_SAMPLER_DIM_CUBE:
					{
						CoordSwizzle = "yz";
						IndexSwizzle = "w";
						break;
					}
					case GLSL_SAMPLER_DIM_BUF:
					case GLSL_SAMPLER_DIM_EXTERNAL:
					default:
					{
						check(0);
						break;
					}
				}
				
				ralloc_asprintf_append(buffer, ").x%s, (uint)(", CoordSwizzle);
				tex->coordinate->accept(this);
				ralloc_asprintf_append(buffer, ").%s", IndexSwizzle);
			}
			else
			{
				tex->coordinate->accept(this);
			}
			
			if (tex->shadow_comparitor)
			{
				ralloc_asprintf_append(buffer, ", ");
				tex->shadow_comparitor->accept(this);
			}

			if (tex->op == ir_txl && (!tex->shadow_comparitor || !tex->lod_info.lod->is_zero()))
			{
				ralloc_asprintf_append(buffer, ", level(");
				tex->lod_info.lod->accept(this);
				ralloc_asprintf_append(buffer, ")");
			}
			else if (tex->op == ir_txb)
			{
				ralloc_asprintf_append(buffer, ", bias(");
				tex->lod_info.lod->accept(this);
				ralloc_asprintf_append(buffer, ")");
			}
			else if (tex->op == ir_txd)
			{
				char const* GradientType = "";
				switch(tex->sampler->type->sampler_dimensionality)
				{
					case GLSL_SAMPLER_DIM_2D:
					case GLSL_SAMPLER_DIM_RECT:
					{
						GradientType = "gradient2d";
						break;
					}
					case GLSL_SAMPLER_DIM_3D:
					{
						GradientType = "gradient3d";
						break;
					}
					case GLSL_SAMPLER_DIM_CUBE:
					{
						GradientType = "gradientcube";
						break;
					}
					case GLSL_SAMPLER_DIM_1D:
					case GLSL_SAMPLER_DIM_BUF:
					case GLSL_SAMPLER_DIM_EXTERNAL:
					default:
					{
						check(0);
						break;
					}
				}
				ralloc_asprintf_append(buffer, ", %s(", GradientType);
				tex->lod_info.grad.dPdx->accept(this);
				ralloc_asprintf_append(buffer, ",");
				tex->lod_info.grad.dPdy->accept(this);
				ralloc_asprintf_append(buffer, ")");
			}

			if (tex->offset)
			{
				ralloc_asprintf_append(buffer, ", ");
				tex->offset->accept(this);
			}
		}
			break;

		case ir_txf:
		{
			check(tex->sampler->type);
			if (tex->sampler->type->is_sampler() && tex->sampler->type->sampler_buffer)
			{
				auto* Texture = tex->sampler->variable_referenced();
				int Index = Buffers.GetIndex(Texture, Backend->bIsDesktop);
				check(Index >= 0 && Index <= 30);
				
				ralloc_asprintf_append(buffer, "(");
				tex->sampler->accept(this);
				if (Backend && Backend->bBoundsChecks)
				{
					ralloc_asprintf_append(buffer, "[");
					ralloc_asprintf_append(buffer, "min(");
					tex->coordinate->accept(this);
					ralloc_asprintf_append(buffer, ",");
					
					ralloc_asprintf_append(buffer, "((BufferSizes[%d] / sizeof(", Index);
					print_type_pre(Texture->type->inner_type);
					ralloc_asprintf_append(buffer, ")) - 1))]");
					
					if (Buffers.AtomicVariables.find(Texture) == Buffers.AtomicVariables.end())
					{
						ralloc_asprintf_append(buffer, " * int(");
						tex->coordinate->accept(this);
						ralloc_asprintf_append(buffer, " < (BufferSizes[%d] / sizeof(", Index);
						print_type_pre(Texture->type->inner_type);
						ralloc_asprintf_append(buffer, ")))");
					}
				}
				else
				{
					ralloc_asprintf_append(buffer, "[");
					tex->coordinate->accept(this);
					ralloc_asprintf_append(buffer, "]");
				}
				
				ralloc_asprintf_append(buffer, ")");
				
				bNeedsClosingParenthesis = false;
			}
			else
			{
				tex->sampler->accept(this);
				ralloc_asprintf_append(buffer, ".read(");
				tex->coordinate->accept(this);
				
				if (tex->sampler->type->sampler_ms)
				{
					ralloc_asprintf_append(buffer, ",");
					tex->lod_info.sample_index->accept(this);
				}
			}
		}
			break;

		case ir_txg:
		{
			//Tv gather(sampler s, float2 coord, int2 offset = int2(0)) const
			//Tv gather_compare(sampler s, float2 coord, float compare_value, int2 offset = int2(0)) const
			if (tex->shadow_comparitor)
			{
				ralloc_asprintf_append(buffer, ".gather_compare(");
			}
			else
			{
				ralloc_asprintf_append(buffer, ".gather(");
			}
			// Sampler
			auto* Texture = tex->sampler->variable_referenced();
			check(Texture);
			bool bDummy;
			auto* Entry = ParseState->FindPackedSamplerEntry(Texture->name);
			int32 SamplerStateIndex = Buffers.GetUniqueSamplerStateIndex(tex->SamplerStateName, false, bDummy);
			check(SamplerStateIndex != INDEX_NONE);
			ralloc_asprintf_append(buffer, "s%d, ", SamplerStateIndex);
			// Coord
			tex->coordinate->accept(this);

			if (tex->shadow_comparitor)
			{
				tex->shadow_comparitor->accept(this);
				ralloc_asprintf_append(buffer, ", ");
			}

			if (tex->offset)
			{
				ralloc_asprintf_append(buffer, ", ");
				tex->offset->accept(this);
			}
		}
			break;

		case ir_txs:
		{
			// Convert from:
			//	HLSL
			//		int w, h;
			//		T.GetDimensions({lod, }w, h);
			// GLSL
			//		ivec2 Temp;
			//		Temp = textureSize(T{, lod});
			// Metal
			//		int2 Temp = int2((int)T.get_width({lod}), (int)T.get_height({lod}));
			ralloc_asprintf_append(buffer, ".get_width(");
			if (tex->lod_info.lod)
			{
				tex->lod_info.lod->accept(this);
			}
			ralloc_asprintf_append(buffer, "), (int)");
			tex->sampler->accept(this);
			ralloc_asprintf_append(buffer, ".get_height(");
			if (tex->lod_info.lod)
			{
				tex->lod_info.lod->accept(this);
			}
			ralloc_asprintf_append(buffer, ")");
		}
			break;

		case ir_txm:
		{
			// Convert from:
			//	HLSL
			//		uint w, h, d;
			//		T.GetDimensions({lod, }w, h, d);
			// Metal
			//		uint2 Temp = T.get_num_mip_levels();
			ralloc_asprintf_append(buffer, ".get_num_mip_levels()");
		}
			break;

		default:
			ralloc_asprintf_append(buffer, "UNKNOWN TEXOP %d!", tex->op);
			check(0);
			break;
		}

		if (bNeedsClosingParenthesis)
		{
			ralloc_asprintf_append(buffer, ")");
		}
	}

	virtual void visit(ir_swizzle *swizzle) override
	{
		check(scope_depth > 0);

		const unsigned mask[4] =
		{
			swizzle->mask.x,
			swizzle->mask.y,
			swizzle->mask.z,
			swizzle->mask.w,
		};

		if (swizzle->val->type->is_scalar())
		{
			// Scalar -> Vector swizzles must use the constructor syntax.
			if (swizzle->type->is_scalar() == false)
			{
				print_type_full(swizzle->type);
				ralloc_asprintf_append(buffer, "(");
				swizzle->val->accept(this);
				ralloc_asprintf_append(buffer, ")");
			}
		}
		else
		{
			const bool is_constant = swizzle->val->as_constant() != nullptr;
			if (is_constant)
			{
				ralloc_asprintf_append(buffer, "(");
			}
			swizzle->val->accept(this);
			if (is_constant)
			{
				ralloc_asprintf_append(buffer, ")");
			}
			ralloc_asprintf_append(buffer, ".");
			for (unsigned i = 0; i < swizzle->mask.num_components; i++)
			{
				ralloc_asprintf_append(buffer, "%c", "xyzw"[mask[i]]);
			}
		}
	}

	virtual void visit(ir_dereference_variable *deref) override
	{
		check(scope_depth > 0);

		ir_variable* var = deref->variable_referenced();

		ralloc_asprintf_append(buffer, unique_name(var));

		if (var->type->base_type == GLSL_TYPE_STRUCT)
		{
			if (hash_table_find(used_structures, var->type) == NULL)
			{
				hash_table_insert(used_structures, (void*)var->type, var->type);
			}
		}

		if (var->type->base_type == GLSL_TYPE_ARRAY && var->type->fields.array->base_type == GLSL_TYPE_STRUCT)
		{
			if (hash_table_find(used_structures, var->type->fields.array) == NULL)
			{
				hash_table_insert(used_structures, (void*)var->type->fields.array, var->type->fields.array);
			}
		}

		if ((var->type->base_type == GLSL_TYPE_INPUTPATCH || var->type->base_type == GLSL_TYPE_OUTPUTPATCH) && var->type->inner_type->base_type == GLSL_TYPE_STRUCT)
		{
			if (hash_table_find(used_structures, var->type->inner_type) == NULL)
			{
				hash_table_insert(used_structures, (void*)var->type->inner_type, var->type->inner_type);
			}
		}

		if (var->mode == ir_var_uniform && var->semantic != NULL)
		{
			if (hash_table_find(used_uniform_blocks, var->semantic) == NULL)
			{
				hash_table_insert(used_uniform_blocks, (void*)var->semantic, var->semantic);
			}
		}
	}

	virtual void visit(ir_dereference_array *deref) override
	{
		check(scope_depth > 0);

		deref->array->accept(this);

		// Make extra sure crappy Mac OS X compiler won't have any reason to crash
		bool enforceInt = false;

		if (deref->array_index->type->base_type == GLSL_TYPE_UINT)
		{
			if (deref->array_index->ir_type == ir_type_constant)
			{
				should_print_uint_literals_as_ints = true;
			}
			else
			{
				enforceInt = true;
			}
		}

		if (enforceInt)
		{
			ralloc_asprintf_append(buffer, "[int(");
		}
		else
		{
			ralloc_asprintf_append(buffer, "[");
		}

		deref->array_index->accept(this);
		should_print_uint_literals_as_ints = false;

		if (enforceInt)
		{
			ralloc_asprintf_append(buffer, ")]");
		}
		else
		{
			ralloc_asprintf_append(buffer, "]");
		}
	}

	void print_image_op( ir_dereference_image *deref, ir_rvalue *src)
	{
		const int dst_elements = deref->type->vector_elements;
		const int src_elements = (src) ? src->type->vector_elements : 1;
		
		check( 1 <= dst_elements && dst_elements <= 4);
		check( 1 <= src_elements && src_elements <= 4);

		if ( deref->op == ir_image_access)
		{
			bool bIsRWTexture = !deref->image->type->sampler_buffer;
			if (src == nullptr)
			{
				if (bIsRWTexture)
				{
					deref->image->accept(this);
					ralloc_asprintf_append(buffer, ".read(");
					deref->image_index->accept(this);
					ralloc_asprintf_append(buffer, ")");
				}
				else
				{
					auto* Texture = deref->image->variable_referenced();
					int Index = Buffers.GetIndex(Texture, Backend->bIsDesktop);
					check(Index >= 0 && Index <= 30);
					
					ralloc_asprintf_append(buffer, "(");
					deref->image->accept(this);
					
					if (Backend && Backend->bBoundsChecks)
					{
						ralloc_asprintf_append(buffer, "[");
						ralloc_asprintf_append(buffer, "min(");
						deref->image_index->accept(this);
						ralloc_asprintf_append(buffer, ",");
						
						ralloc_asprintf_append(buffer, "((BufferSizes[%d] / sizeof(", Index);
						print_type_pre(Texture->type->inner_type);
						ralloc_asprintf_append(buffer, ")) - 1))]"/*.%s, swizzle[dst_elements - 1]*/);
						
						// Can't flush to zero for a structured buffer...
						if (!Texture->type->inner_type->is_record() && Buffers.AtomicVariables.find(Texture) == Buffers.AtomicVariables.end())
						{
							ralloc_asprintf_append(buffer, " * int(");
							deref->image_index->accept(this);
							ralloc_asprintf_append(buffer, " < (BufferSizes[%d] / sizeof(", Index);
							print_type_pre(Texture->type->inner_type);
							ralloc_asprintf_append(buffer, ")))");
						}
					}
					else
					{
						ralloc_asprintf_append(buffer, "[");
						deref->image_index->accept(this);
						ralloc_asprintf_append(buffer, "]"/*.%s, swizzle[dst_elements - 1]*/);
					}
					
					ralloc_asprintf_append(buffer, ")");
				}
			}
			else
			{
				deref->image->accept(this);
				if (bIsRWTexture)
				{
					ralloc_asprintf_append(buffer, ".write(");
					// @todo Zebra: Below is a terrible hack - the input to write is always vec<T, 4>,
					// 				but the type T comes from the texture type.  
					if(src_elements == 1)
					{
						switch(deref->type->base_type)
						{
							case GLSL_TYPE_UINT:
								ralloc_asprintf_append(buffer, "uint4(");
								break;
							case GLSL_TYPE_INT:
								ralloc_asprintf_append(buffer, "int4(");
								break;
							case GLSL_TYPE_HALF:
								ralloc_asprintf_append(buffer, "half4(");
								break;
							case GLSL_TYPE_FLOAT:
							default:
								ralloc_asprintf_append(buffer, "float4(");
								break;
						}
						src->accept(this);
						ralloc_asprintf_append(buffer, ")");
					}
					else
					{
						switch(deref->type->base_type)
						{
							case GLSL_TYPE_UINT:
								ralloc_asprintf_append(buffer, "(uint4)(");
								break;
							case GLSL_TYPE_INT:
								ralloc_asprintf_append(buffer, "(int4)(");
								break;
							case GLSL_TYPE_HALF:
								ralloc_asprintf_append(buffer, "(half4)(");
								break;
							case GLSL_TYPE_FLOAT:
							default:
								ralloc_asprintf_append(buffer, "(float4)(");
								break;
						}
						src->accept(this);
						switch (src_elements)
						{
							case 3:
								ralloc_asprintf_append(buffer, ").xyzx");
								break;
							case 2:
								ralloc_asprintf_append(buffer, ").xyxy");
								break;
							default:
								ralloc_asprintf_append(buffer, ")");
								break;
						}
					}

					//#todo-rco: Add language spec to know if indices need to be uint
					ralloc_asprintf_append(buffer, ",(uint");
					switch (deref->image_index->type->vector_elements)
					{
					case 4:
					case 3:
					case 2:
						ralloc_asprintf_append(buffer, "%u", deref->image_index->type->vector_elements);
						//fallthrough
					case 1:
						ralloc_asprintf_append(buffer, ")(");
						break;
					}
					deref->image_index->accept(this);
					ralloc_asprintf_append(buffer, "))");
				}
				else
				{
					if (Backend && Backend->bBoundsChecks)
					{
						ralloc_asprintf_append(buffer, "[");
						ralloc_asprintf_append(buffer, "min(");
						deref->image_index->accept(this);
						ralloc_asprintf_append(buffer, ",");
						
						auto* Texture = deref->image->variable_referenced();
						int Index = Buffers.GetIndex(Texture, Backend->bIsDesktop);
						check(Index >= 0 && Index <= 30);
						
						ralloc_asprintf_append(buffer, "(BufferSizes[%d] / sizeof(", Index);
						print_type_pre(Texture->type->inner_type);
						ralloc_asprintf_append(buffer, ")))] = ");
					}
					else
					{
						ralloc_asprintf_append(buffer, "[");
						deref->image_index->accept(this);
						ralloc_asprintf_append(buffer, "] = ");
					}
					src->accept(this);
					ralloc_asprintf_append(buffer, ""/*".%s", expand[src_elements - 1]*/);
				}
			}
		}
		else if ( deref->op == ir_image_dimensions)
		{
			ralloc_asprintf_append( buffer, "imageSize( " );
			deref->image->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else
		{
			check( !"Unknown image operation");
		}
	}

	virtual void visit(ir_dereference_image *deref) override
	{
		check(scope_depth > 0);

		print_image_op( deref, NULL);
		
	}

	virtual void visit(ir_dereference_record *deref) override
	{
		check(scope_depth > 0);

		deref->record->accept(this);
		ralloc_asprintf_append(buffer, ".%s", deref->field);
	}

	virtual void visit(ir_assignment *assign) override
	{
		if (scope_depth == 0)
		{
			global_instructions.push_tail(new(mem_ctx) global_ir(assign));
			needs_semicolon = false;
			return;
		}

		// constant variables with initializers are statically assigned
		ir_variable *var = assign->lhs->variable_referenced();
		if (var->has_initializer && var->read_only && (var->constant_initializer || var->constant_value))
		{
			//This will leave a blank line with a semi-colon
			return;
		}

		if (assign->condition)
		{
			ralloc_asprintf_append(buffer, "if(");
			assign->condition->accept(this);
			ralloc_asprintf_append(buffer, ") { ");
		}

		if (assign->lhs->as_dereference_image() != NULL)
		{
			/** EHart - should the write mask be checked here? */
			print_image_op( assign->lhs->as_dereference_image(), assign->rhs);
		}
		else
		{
			char mask[6];
			unsigned j = 1;
			if (assign->lhs->type->is_scalar() == false ||
				assign->write_mask != 0x1)
			{
				for (unsigned i = 0; i < 4; i++)
				{
					if ((assign->write_mask & (1 << i)) != 0)
					{
						mask[j] = "xyzw"[i];
						j++;
					}
				}
			}
			mask[j] = '\0';

			mask[0] = (j == 1) ? '\0' : '.';

			assign->lhs->accept(this);
			ralloc_asprintf_append(buffer, "%s = ", mask);

			// Hack: Need to add additional cast from packed types
			bool bNeedToAcceptRHS = true;
			if (auto* Expr = assign->rhs->as_expression())
			{
				if (Expr->operation == ir_unop_f2h)
				{
					auto* Var = Expr->operands[0]->variable_referenced();
					if (Var && Var->mode == ir_var_uniform && Var->type->HlslName && !strcmp(Var->type->HlslName, "__PACKED__"))
					{
						ralloc_asprintf_append(buffer, "%s(%s(", Expr->type->name, FixVecPrefix(PromoteHalfToFloatType(ParseState, Expr->type)->name).c_str());
						Expr->operands[0]->accept(this);
						ralloc_asprintf_append(buffer, "))");
						bNeedToAcceptRHS = false;
					}
				}
			}

			if (bNeedToAcceptRHS)
			{
				assign->rhs->accept(this);
			}
		}

		if (assign->condition)
		{
			ralloc_asprintf_append(buffer, "%s }", needs_semicolon ? ";" : "");
		}
	}

	void print_constant(ir_constant *constant, int index)
	{
		if (constant->type->is_float())
		{
			if (constant->is_component_finite(index))
			{
				float value = constant->value.f[index];
				const char *format = (fabsf(fmodf(value,1.0f)) < 1.e-8f) ? "%.1f" : "%.8f";
				ralloc_asprintf_append(buffer, format, value);
			}
			else
			{
				switch (constant->value.u[index])
				{
					case 0x7f800000u:
						ralloc_asprintf_append(buffer, "(1.0/0.0)");
						break;

					case 0xffc00000u:
						ralloc_asprintf_append(buffer, "(0.0/0.0)");
						break;

					case 0xff800000u:
						ralloc_asprintf_append(buffer, "(-1.0/0.0)");
						break;

					default:
						ralloc_asprintf_append(buffer, "UnknownNonFinite_0x%08x", constant->value.u[index]);
						check(0);
				}
			}
		}
		else if (constant->type->base_type == GLSL_TYPE_INT)
		{
			ralloc_asprintf_append(buffer, "%d", constant->value.i[index]);
		}
		else if (constant->type->base_type == GLSL_TYPE_UINT)
		{
			ralloc_asprintf_append(buffer, "%u%s",
				constant->value.u[index],
				should_print_uint_literals_as_ints ? "" : "u"
				);
		}
		else if (constant->type->base_type == GLSL_TYPE_BOOL)
		{
			ralloc_asprintf_append(buffer, "%s", constant->value.b[index] ? "true" : "false");
		}
	}

	virtual void visit(ir_constant *constant) override
	{
		if (constant->type == glsl_type::float_type
			|| constant->type == glsl_type::int_type
			|| constant->type == glsl_type::uint_type)
		{
			print_constant(constant, 0);
		}
		else if (constant->type->is_record())
		{
			print_type_full(constant->type);
			ralloc_asprintf_append(buffer, "(");
			ir_constant* value = (ir_constant*)constant->components.get_head();
			if (value)
			{
				value->accept(this);
			}
			for (uint32 i = 1; i < constant->type->length; i++)
			{
				check(value);
				value = (ir_constant*)value->next;
				if (value)
				{
					ralloc_asprintf_append(buffer, ",");
					value->accept(this);
				}
			}
			ralloc_asprintf_append(buffer, ")");
		}
		else if (constant->type->is_array())
		{
			print_type_full(constant->type);
			ralloc_asprintf_append(buffer, "(");
			constant->get_array_element(0)->accept(this);
			for (uint32 i = 1; i < constant->type->length; ++i)
			{
				ralloc_asprintf_append(buffer, ",");
				constant->get_array_element(i)->accept(this);
			}
			ralloc_asprintf_append(buffer, ")");
		}
		else
		{
			if (constant->type->is_matrix())
			{
				// Need to print row by row
				print_type_full(constant->type);
				ralloc_asprintf_append(buffer, "(");
				const auto* RowType = constant->type->column_type();
				uint32 Component = 0;
				for (uint32 Index = 0; Index < constant->type->matrix_columns; ++Index)
				{
					if (Index > 0)
					{
						ralloc_asprintf_append(buffer, ",");
					}
					print_type_full(RowType);
					ralloc_asprintf_append(buffer, "(");
					for (uint32 VecIndex = 0; VecIndex < RowType->vector_elements; ++VecIndex)
					{
						if (VecIndex > 0)
						{
							ralloc_asprintf_append(buffer, ",");
						}
						print_constant(constant, Component);
						++Component;
					}
					ralloc_asprintf_append(buffer, ")");
				}
				check(Component == constant->type->components());
				ralloc_asprintf_append(buffer, ")");
			}
			else
			{
				print_type_full(constant->type);
				ralloc_asprintf_append(buffer, "(");
				print_constant(constant, 0);
				int num_components = constant->type->components();
				for (int i = 1; i < num_components; ++i)
				{
					ralloc_asprintf_append(buffer, ",");
					print_constant(constant, i);
				}
				ralloc_asprintf_append(buffer, ")");
			}
		}
	}

	virtual void visit(ir_call *call) override
	{
		if (scope_depth == 0)
		{
			global_instructions.push_tail(new(mem_ctx) global_ir(call));
			needs_semicolon = false;
			return;
		}

		if (call->return_deref)
		{
			call->return_deref->accept(this);
			ralloc_asprintf_append(buffer, " = ");
		}
		else
		{
			//@todo-rco: Fix this properly
			if (!strcmp(call->callee_name(), GROUP_MEMORY_BARRIER) || !strcmp(call->callee_name(), GROUP_MEMORY_BARRIER_WITH_GROUP_SYNC))
			{
				bNeedsComputeInclude = true;
				ralloc_asprintf_append(buffer, "threadgroup_barrier(mem_flags::mem_threadgroup)");
				return;
			}
			else if (!strcmp(call->callee_name(), DEVICE_MEMORY_BARRIER) || !strcmp(call->callee_name(), DEVICE_MEMORY_BARRIER_WITH_GROUP_SYNC))
			{
				bNeedsComputeInclude = true;
				ralloc_asprintf_append(buffer, "threadgroup_barrier(mem_flags::mem_device)");
				return;
			}
			else if (!strcmp(call->callee_name(), ALL_MEMORY_BARRIER) || !strcmp(call->callee_name(), ALL_MEMORY_BARRIER_WITH_GROUP_SYNC))
			{
				bNeedsComputeInclude = true;
				ralloc_asprintf_append(buffer, "threadgroup_barrier(mem_flags::mem_device_and_threadgroup)");
				return;
			}
		}

        if (call->return_deref && call->return_deref->type && call->return_deref->type->is_scalar())
        {
            if (!strcmp(call->callee_name(), "length"))
            {
                bool bIsVector = true;
                foreach_iter(exec_list_iterator, iter, *call)
                {
                    ir_instruction *const inst = (ir_instruction *) iter.get();
                    ir_rvalue* const val = inst->as_rvalue();
                    if (val && val->type->is_scalar())
                    {
                        bIsVector &= val->type->is_vector();
                    }
                }
                
                if (!bIsVector)
                {
                    ralloc_asprintf_append(buffer, "(");
                    foreach_iter(exec_list_iterator, iter, *call)
                    {
                        ir_instruction *const inst = (ir_instruction *) iter.get();
                        inst->accept(this);
                    }
                    ralloc_asprintf_append(buffer, ")");
                    return;
                }
            }
        }
        
		if (!strcmp(call->callee_name(), "packHalf2x16"))
		{
			ralloc_asprintf_append(buffer, "as_type<uint>(half2(");
		}
		else if (!strcmp(call->callee_name(), "unpackHalf2x16"))
		{
			if(call->return_deref && call->return_deref->type && call->return_deref->type->base_type == GLSL_TYPE_HALF)
			{
				ralloc_asprintf_append(buffer, "half2(as_type<half2>(");
			}
			else
			{
				ralloc_asprintf_append(buffer, "float2(as_type<half2>(");
			}
		}
		else
		{
			ralloc_asprintf_append(buffer, "%s(", call->callee_name());
		}
		bool bPrintComma = false;
		foreach_iter(exec_list_iterator, iter, *call)
		{
			ir_instruction *const inst = (ir_instruction *) iter.get();
			if (bPrintComma)
			{
				ralloc_asprintf_append(buffer, ",");
			}
			inst->accept(this);
			bPrintComma = true;
		}
		ralloc_asprintf_append(buffer, ")");
		
		if (!strcmp(call->callee_name(), "packHalf2x16") || !strcmp(call->callee_name(), "unpackHalf2x16"))
		{
			ralloc_asprintf_append(buffer, ")");
		}
	}

	virtual void visit(ir_return *ret) override
	{
		check(scope_depth > 0);

		ralloc_asprintf_append(buffer, "return ");
		ir_rvalue *const value = ret->get_value();
		if (value)
		{
			value->accept(this);
		}
	}

	virtual void visit(ir_discard *discard) override
	{
		check(scope_depth > 0);

		if (discard->condition)
		{
			ralloc_asprintf_append(buffer, "if (");
			discard->condition->accept(this);
			ralloc_asprintf_append(buffer, ") ");
		}
		ralloc_asprintf_append(buffer, "discard_fragment()");
	}

	bool try_conditional_move(ir_if *expr)
	{
		ir_dereference_variable *dest_deref = NULL;
		ir_rvalue *true_value = NULL;
		ir_rvalue *false_value = NULL;
		unsigned write_mask = 0;
		const glsl_type *assign_type = NULL;
		int num_inst;

		num_inst = 0;
		foreach_iter(exec_list_iterator, iter, expr->then_instructions)
		{
			if (num_inst > 0)
			{
				// multiple instructions? not a conditional move
				return false;
			}

			ir_instruction *const inst = (ir_instruction *) iter.get();
			ir_assignment *assignment = inst->as_assignment();
			if (assignment && (assignment->rhs->ir_type == ir_type_dereference_variable || assignment->rhs->ir_type == ir_type_constant))
			{
				dest_deref = assignment->lhs->as_dereference_variable();
				true_value = assignment->rhs;
				write_mask = assignment->write_mask;
			}
			num_inst++;
		}

		if (dest_deref == NULL || true_value == NULL)
		{
			return false;
		}

		num_inst = 0;
		foreach_iter(exec_list_iterator, iter, expr->else_instructions)
		{
			if (num_inst > 0)
			{
				// multiple instructions? not a conditional move
				return false;
			}

			ir_instruction *const inst = (ir_instruction *) iter.get();
			ir_assignment *assignment = inst->as_assignment();
			if (assignment && (assignment->rhs->ir_type == ir_type_dereference_variable || assignment->rhs->ir_type == ir_type_constant))
			{
				ir_dereference_variable *tmp_deref = assignment->lhs->as_dereference_variable();
				if (tmp_deref
					&& tmp_deref->var == dest_deref->var
					&& tmp_deref->type == dest_deref->type
					&& assignment->write_mask == write_mask)
				{
					false_value= assignment->rhs;
				}
			}
			num_inst++;
		}

		if (false_value == NULL)
			return false;

		char mask[6];
		unsigned j = 1;
		if (dest_deref->type->is_scalar() == false || write_mask != 0x1)
		{
			for (unsigned i = 0; i < 4; i++)
			{
				if ((write_mask & (1 << i)) != 0)
				{
					mask[j] = "xyzw"[i];
					j++;
				}
			}
		}
		mask[j] = '\0';
		mask[0] = (j == 1) ? '\0' : '.';

		dest_deref->accept(this);
		ralloc_asprintf_append(buffer, "%s = (", mask);
		expr->condition->accept(this);
		ralloc_asprintf_append(buffer, ")?(");
		true_value->accept(this);
		ralloc_asprintf_append(buffer, "):(");
		false_value->accept(this);
		ralloc_asprintf_append(buffer, ")");

		return true;
	}

	virtual void visit(ir_if *expr) override
	{
		check(scope_depth > 0);

		if (try_conditional_move(expr) == false)
		{
			ralloc_asprintf_append(buffer, "if (");
			expr->condition->accept(this);
			ralloc_asprintf_append(buffer, ")\n");
			indent();
			ralloc_asprintf_append(buffer, "{\n");

			indentation++;
			foreach_iter(exec_list_iterator, iter, expr->then_instructions)
			{
				ir_instruction *const inst = (ir_instruction *)iter.get();
				indent();
				do_visit(inst);
			}
			indentation--;

			indent();
			ralloc_asprintf_append(buffer, "}\n");

			if (!expr->else_instructions.is_empty())
			{
				indent();
				ralloc_asprintf_append(buffer, "else\n");
				indent();
				ralloc_asprintf_append(buffer, "{\n");

				indentation++;
				foreach_iter(exec_list_iterator, iter, expr->else_instructions)
				{
					ir_instruction *const inst = (ir_instruction *)iter.get();
					indent();
					do_visit(inst);
				}
				indentation--;

				indent();
				ralloc_asprintf_append(buffer, "}\n");
			}

			needs_semicolon = false;
		}
	}

	virtual void visit(ir_loop *loop) override
	{
		check(scope_depth > 0);

		if (loop->counter && loop->to)
		{
			// IR cmp operator is when to terminate loop; whereas GLSL for loop syntax
			// is while to continue the loop. Invert the meaning of operator when outputting.
			const char* termOp = NULL;
			switch (loop->cmp)
			{
				case ir_binop_less: termOp = ">="; break;
				case ir_binop_greater: termOp = "<="; break;
				case ir_binop_lequal: termOp = ">"; break;
				case ir_binop_gequal: termOp = "<"; break;
				case ir_binop_equal: termOp = "!="; break;
				case ir_binop_nequal: termOp = "=="; break;
				default: check(false);
			}
			ralloc_asprintf_append(buffer, "for (;%s%s", unique_name(loop->counter), termOp);
			loop->to->accept (this);
			ralloc_asprintf_append(buffer, ";)\n");
		}
		else
		{
#if 1
			ralloc_asprintf_append(buffer, "for (;;)\n");
#else
			ralloc_asprintf_append(buffer, "for ( int loop%d = 0; loop%d < 256; loop%d ++)\n", loop_count, loop_count, loop_count);
			loop_count++;
#endif
		}
		indent();
		ralloc_asprintf_append(buffer, "{\n");

		indentation++;
		foreach_iter(exec_list_iterator, iter, loop->body_instructions)
		{
			ir_instruction *const inst = (ir_instruction *) iter.get();
			indent();
			do_visit(inst);
		}
		indentation--;

		indent();
		ralloc_asprintf_append(buffer, "}\n");

		needs_semicolon = false;
	}

	virtual void visit(ir_loop_jump *jmp) override
	{
		check(scope_depth > 0);

		ralloc_asprintf_append(buffer, "%s",
			jmp->is_break() ? "break" : "continue");
	}

	virtual void visit(ir_atomic *ir) override
	{
		const char* SharedAtomicFunctions[ir_atomic_count] = 
		{
			"atomic_fetch_add_explicit",
			"atomic_fetch_and_explicit",
			"atomic_fetch_min_explicit",
			"atomic_fetch_max_explicit",
			"atomic_fetch_or_explicit",
			"atomic_fetch_xor_explicit",
			"atomic_exchange_explicit",
			"atomic_compare_exchange_weak_explicit",
			"atomic_load_explicit",
			"atomic_store_explicit",
		};
		static_assert(sizeof(SharedAtomicFunctions) / sizeof(SharedAtomicFunctions[0]) == ir_atomic_count, "Mismatched entries!");
/*
		const char *imageAtomicFunctions[] =
		{
			"imageAtomicAdd",
			"imageAtomicAnd",
			"imageAtomicMin",
			"imageAtomicMax",
			"imageAtomicOr",
			"imageAtomicXor",
			"imageAtomicExchange",
			"imageAtomicCompSwap"
		};
*/
		check(scope_depth > 0);
		const bool is_image = ir->memory_ref->as_dereference_image() != NULL;

		if (ir->lhs)
		{
			ir->lhs->accept(this);
			ralloc_asprintf_append(buffer, " = ");
		}
		//if (!is_image)
		{
			ralloc_asprintf_append(buffer, "%s(&", SharedAtomicFunctions[ir->operation]);
			ir->memory_ref->accept(this);
			if (ir->operands[0])
			{
				ralloc_asprintf_append(buffer, ", ");
				ir->operands[0]->accept(this);
			}
			if (ir->operands[1])
			{
				ralloc_asprintf_append(buffer, ", ");
				ir->operands[1]->accept(this);
			}
			ralloc_asprintf_append(buffer, ", memory_order_relaxed)");
		}
/*
		else
		{
			ir_dereference_image *image = ir->memory_ref->as_dereference_image();
			ralloc_asprintf_append(buffer, " = %s(",
				imageAtomicFunctions[ir->operation]);
			image->image->accept(this);
			ralloc_asprintf_append(buffer, ", ");
			image->image_index->accept(this);
			ralloc_asprintf_append(buffer, ", ");
			ir->operands[0]->accept(this);
			if (ir->operands[1])
			{
				ralloc_asprintf_append(buffer, ", ");
				ir->operands[1]->accept(this);
			}
			ralloc_asprintf_append(buffer, ", memory_order_relaxed)");
		}*/
	}

	/**
	 * Declare structs used by the code that has been generated.
	 */
	void declare_structs(_mesa_glsl_parse_state* state)
	{
		// If any variable in a uniform block is in use, the entire uniform block
		// must be present including structs that are not actually accessed.
		for (unsigned i = 0; i < state->num_uniform_blocks; i++)
		{
			const glsl_uniform_block* block = state->uniform_blocks[i];
			if (hash_table_find(used_uniform_blocks, block->name))
			{
				for (unsigned var_index = 0; var_index < block->num_vars; ++var_index)
				{
					const glsl_type* type = block->vars[var_index]->type;
					if (type->base_type == GLSL_TYPE_STRUCT &&
						hash_table_find(used_structures, type) == NULL)
					{
						hash_table_insert(used_structures, (void*)type, type);
					}
				}
			}
		}

		// If otherwise unused structure is a member of another, used structure, the unused structure is also, in fact, used
		{
			int added_structure_types;
			do
			{
				added_structure_types = 0;
				for (unsigned i = 0; i < state->num_user_structures; i++)
				{
					const glsl_type *const s = state->user_structures[i];

					if (hash_table_find(used_structures, s) == NULL)
					{
						continue;
					}

					for (unsigned j = 0; j < s->length; j++)
					{
						const glsl_type* type = s->fields.structure[j].type;

						if (type->base_type == GLSL_TYPE_STRUCT)
						{
							if (hash_table_find(used_structures, type) == NULL)
							{
								hash_table_insert(used_structures, (void*)type, type);
								++added_structure_types;
							}
						}
						else if (type->base_type == GLSL_TYPE_ARRAY && type->fields.array->base_type == GLSL_TYPE_STRUCT)
						{
							if (hash_table_find(used_structures, type->fields.array) == NULL)
							{
								hash_table_insert(used_structures, (void*)type->fields.array, type->fields.array);
							}
						}
						else if ((type->base_type == GLSL_TYPE_INPUTPATCH || type->base_type == GLSL_TYPE_OUTPUTPATCH) && type->inner_type->base_type == GLSL_TYPE_STRUCT)
						{
							if (hash_table_find(used_structures, type->inner_type) == NULL)
							{
								hash_table_insert(used_structures, (void*)type->inner_type, type->inner_type);
							}
						}
					}
				}
			}
			while( added_structure_types > 0 );
		}

		for (unsigned i = 0; i < state->num_user_structures; i++)
		{
			const glsl_type *const s = state->user_structures[i];

			if (hash_table_find(used_structures, s) == NULL)
			{
				continue;
			}

			if (s->HlslName && !strcmp(s->HlslName, "__PACKED__"))
			{
				bUsePacked = true;
			}

			ralloc_asprintf_append(buffer, "struct %s\n{\n", s->name);

			if (s->length == 0)
			{
				ralloc_asprintf_append(buffer, "\t float glsl_doesnt_like_empty_structs;\n");
			}
			else
			{
				for (unsigned j = 0; j < s->length; j++)
				{
					ralloc_asprintf_append(buffer, "\t");
					print_type_pre(s->fields.structure[j].type);
					ralloc_asprintf_append(buffer, " %s", s->fields.structure[j].name);
					print_type_post(s->fields.structure[j].type);
//@todo-rco
					if (s->fields.structure[j].semantic)
					{
						if (!strncmp(s->fields.structure[j].semantic, "ATTRIBUTE", 9))
						{
							ralloc_asprintf_append(buffer, " [[ attribute(%s) ]]", s->fields.structure[j].semantic + 9);
						}
						else if (!strncmp(s->fields.structure[j].semantic, "gl_FragDepth", 12) || !strcmp(s->fields.structure[j].semantic, "[[ depth(any) ]]"))
						{
							ralloc_asprintf_append(buffer, " [[ depth(any) ]]");
							output_variables.push_tail(new(state) extern_var(new(state)ir_variable(s->fields.structure[j].type, "FragDepth", ir_var_out)));
						}
						else if (!strcmp(s->fields.structure[j].semantic, "SV_RenderTargetArrayIndex"))
						{
							ralloc_asprintf_append(buffer, " [[ render_target_array_index ]]");
						}
						else if (!strcmp(s->fields.structure[j].semantic, "SV_Coverage") || !strcmp(s->fields.structure[j].semantic, "[[ sample_mask ]]"))
						{
							ralloc_asprintf_append(buffer, " [[ sample_mask ]]");
						}
						else if (!strncmp(s->fields.structure[j].semantic, "[[", 2))
						{
							ralloc_asprintf_append(buffer, " %s", s->fields.structure[j].semantic);
						}
						else
						{
							ralloc_asprintf_append(buffer, "[[ ERROR! ]]");
							check(0);
						}
					}
					ralloc_asprintf_append(buffer, ";\n");
				}
			}
			ralloc_asprintf_append(buffer, "};\n\n");
			bUsePacked = false;
		}

		unsigned num_used_blocks = 0;
		for (unsigned i = 0; i < state->num_uniform_blocks; i++)
		{
			const glsl_uniform_block* block = state->uniform_blocks[i];
			if (hash_table_find(used_uniform_blocks, block->name))
			{
				const char* block_name = block->name;
				check(0);
/*
				if (state->has_packed_uniforms)
				{
					block_name = ralloc_asprintf(mem_ctx, "%sb%u",
						glsl_variable_tag_from_parser_target(state->target),
						num_used_blocks
						);
				}
				ralloc_asprintf_append(buffer, "layout(std140) uniform %s\n{\n", block_name);

				for (unsigned var_index = 0; var_index < block->num_vars; ++var_index)
				{
					ir_variable* var = block->vars[var_index];
					ralloc_asprintf_append(buffer, "\t");
					print_type_pre(var->type);
					ralloc_asprintf_append(buffer, " %s", var->name);
					print_type_post(var->type);
					ralloc_asprintf_append(buffer, ";\n");
				}
				ralloc_asprintf_append(buffer, "};\n\n");
*/
				num_used_blocks++;
			}
		}
	}

	void PrintPackedSamplers(_mesa_glsl_parse_state::TUniformList& Samplers, TStringToSetMap& TextureToSamplerMap)
	{
		bool bPrintHeader = true;
		bool bNeedsComma = false;
		for (_mesa_glsl_parse_state::TUniformList::iterator Iter = Samplers.begin(); Iter != Samplers.end(); ++Iter)
		{
			glsl_packed_uniform& Sampler = *Iter;
			FCustomStdString SamplerStates("");
			TStringToSetMap::iterator IterFound = TextureToSamplerMap.find(Sampler.Name);
			if (IterFound != TextureToSamplerMap.end())
			{
				TStringSet& ListSamplerStates = IterFound->second;
				check(!ListSamplerStates.empty());
				for (TStringSet::iterator IterSS = ListSamplerStates.begin(); IterSS != ListSamplerStates.end(); ++IterSS)
				{
					if (IterSS == ListSamplerStates.begin())
					{
						SamplerStates += "[";
					}
					else
					{
						SamplerStates += ",";
					}
					SamplerStates += *IterSS;
				}

				SamplerStates += "]";
			}

			// Try to find SRV index
			uint32 Offset = Sampler.offset;
			for (int i = 0; i < Buffers.Buffers.Num(); ++i)
			{
				if (Buffers.Buffers[i])
				{
					auto* Var = Buffers.Buffers[i]->as_variable();
					if (!Var->semantic && Var->type->is_sampler() && Var->name && Sampler.CB_PackedSampler == Var->name && Var->type->sampler_buffer)
					{
						Offset = i;
						break;
					}
				}
			}


			ralloc_asprintf_append(
				buffer,
				"%s%s(%u:%u%s)",
				bNeedsComma ? "," : "",
				Sampler.Name.c_str(),
				Offset,
				Sampler.num_components,
				SamplerStates.c_str()
				);

			bNeedsComma = true;
		}
	}

	void PrintImages(_mesa_glsl_parse_state::TUniformList& Uniforms)
	{
		bool bPrintHeader = true;
		bool bNeedsComma = false;
		for (_mesa_glsl_parse_state::TUniformList::iterator Iter = Uniforms.begin(); Iter != Uniforms.end(); ++Iter)
		{
			glsl_packed_uniform& Uniform = *Iter;
			ANSICHAR Name[32];
			FCStringAnsi::Sprintf(Name, "%si%u", glsl_variable_tag_from_parser_target(Frequency), Uniform.offset);
			int32 Offset = Buffers.GetIndex(Name, Backend->bIsDesktop);
            check(Offset >= 0);
			ralloc_asprintf_append(
				buffer,
				"%s%s(%u:%u)",
				bNeedsComma ? "," : "",
				Uniform.Name.c_str(),
				Offset,
				Uniform.num_components
				);
			bNeedsComma = true;
		}
	}

	void PrintPackedGlobals(_mesa_glsl_parse_state* State)
	{
		//	@PackedGlobals: Global0(DestArrayType, DestOffset, SizeInFloats), Global1(DestArrayType, DestOffset, SizeInFloats), ...
		bool bNeedsHeader = true;
		bool bNeedsComma = false;
		for (auto ArrayIter = State->GlobalPackedArraysMap.begin(); ArrayIter != State->GlobalPackedArraysMap.end(); ++ArrayIter)
		{
			char ArrayType = ArrayIter->first;
			if (ArrayType != EArrayType_Image && ArrayType != EArrayType_Sampler)
			{
				_mesa_glsl_parse_state::TUniformList& Uniforms = ArrayIter->second;
				check(!Uniforms.empty());

				for (auto Iter = Uniforms.begin(); Iter != Uniforms.end(); ++Iter)
				{
					glsl_packed_uniform& Uniform = *Iter;
					if (!State->bFlattenUniformBuffers || Uniform.CB_PackedSampler.empty())
					{
						if (bNeedsHeader)
						{
							ralloc_asprintf_append(buffer, "// @PackedGlobals: ");
							bNeedsHeader = false;
						}

						ralloc_asprintf_append(
							buffer,
							"%s%s(%c:%u,%u)",
							bNeedsComma ? "," : "",
							Uniform.Name.c_str(),
							ArrayType,
							Uniform.offset,
							Uniform.num_components
							);
						bNeedsComma = true;
					}
				}
			}
		}

		if (!bNeedsHeader)
		{
			ralloc_asprintf_append(buffer, "\n");
		}
	}

	void PrintPackedUniformBuffers(_mesa_glsl_parse_state* State)
	{
		// @PackedUB: UniformBuffer0(SourceIndex0): Member0(SourceOffset,SizeInFloats),Member1(SourceOffset,SizeInFloats), ...
		// @PackedUB: UniformBuffer1(SourceIndex1): Member0(SourceOffset,SizeInFloats),Member1(SourceOffset,SizeInFloats), ...
		// ...

		// First find all used CBs (since we lost that info during flattening)
		TStringSet UsedCBs;
		for (auto IterCB = State->CBPackedArraysMap.begin(); IterCB != State->CBPackedArraysMap.end(); ++IterCB)
		{
			for (auto Iter = IterCB->second.begin(); Iter != IterCB->second.end(); ++Iter)
			{
				_mesa_glsl_parse_state::TUniformList& Uniforms = Iter->second;
				for (auto IterU = Uniforms.begin(); IterU != Uniforms.end(); ++IterU)
				{
					if (!IterU->CB_PackedSampler.empty())
					{
						check(IterCB->first == IterU->CB_PackedSampler);
						UsedCBs.insert(IterU->CB_PackedSampler);
					}
				}
			}
		}

		check(UsedCBs.size() == State->CBPackedArraysMap.size());

		// Now get the CB index based off source declaration order, and print an info line for each, while creating the mem copy list
		unsigned CBIndex = 0;
		TCBDMARangeMap CBRanges;
		for (unsigned i = 0; i < State->num_uniform_blocks; i++)
		{
			const glsl_uniform_block* block = State->uniform_blocks[i];
			if (UsedCBs.find(block->name) != UsedCBs.end())
			{
				bool bNeedsHeader = true;

				// Now the members for this CB
				bool bNeedsComma = false;
				auto IterPackedArrays = State->CBPackedArraysMap.find(block->name);
				check(IterPackedArrays != State->CBPackedArraysMap.end());
				for (auto Iter = IterPackedArrays->second.begin(); Iter != IterPackedArrays->second.end(); ++Iter)
				{
					char ArrayType = Iter->first;
					check(ArrayType != EArrayType_Image && ArrayType != EArrayType_Sampler);

					_mesa_glsl_parse_state::TUniformList& Uniforms = Iter->second;
					for (auto IterU = Uniforms.begin(); IterU != Uniforms.end(); ++IterU)
					{
						glsl_packed_uniform& Uniform = *IterU;
						if (Uniform.CB_PackedSampler == block->name)
						{
							if (bNeedsHeader)
							{
								ralloc_asprintf_append(buffer, "// @PackedUB: %s(%u): ",
									block->name,
									CBIndex);
								bNeedsHeader = false;
							}

							ralloc_asprintf_append(buffer, "%s%s(%u,%u)",
								bNeedsComma ? "," : "",
								Uniform.Name.c_str(),
								Uniform.OffsetIntoCBufferInFloats,
								Uniform.SizeInFloats);

							bNeedsComma = true;
							unsigned SourceOffset = Uniform.OffsetIntoCBufferInFloats;
							unsigned DestOffset = Uniform.offset;
							unsigned Size = Uniform.SizeInFloats;
							unsigned DestCBIndex = 0;
							unsigned DestCBPrecision = ArrayType;
							InsertRange(CBRanges, CBIndex, SourceOffset, Size, DestCBIndex, DestCBPrecision, DestOffset);
						}
					}
				}

				if (!bNeedsHeader)
				{
					ralloc_asprintf_append(buffer, "\n");
				}

				CBIndex++;
			}
		}

		//DumpSortedRanges(SortRanges(CBRanges));

		// @PackedUBCopies: SourceArray:SourceOffset-DestArray:DestOffset,SizeInFloats;SourceArray:SourceOffset-DestArray:DestOffset,SizeInFloats,...
		bool bFirst = true;
		for (auto Iter = CBRanges.begin(); Iter != CBRanges.end(); ++Iter)
		{
			TDMARangeList& List = Iter->second;
			for (auto IterList = List.begin(); IterList != List.end(); ++IterList)
			{
				if (bFirst)
				{
					ralloc_asprintf_append(buffer, "// @PackedUBGlobalCopies: ");
					bFirst = false;
				}
				else
				{
					ralloc_asprintf_append(buffer, ",");
				}

				check(IterList->DestCBIndex == 0);
				ralloc_asprintf_append(buffer, "%u:%u-%c:%u:%u", IterList->SourceCB, IterList->SourceOffset, IterList->DestCBPrecision, IterList->DestOffset, IterList->Size);
			}
		}

		if (!bFirst)
		{
			ralloc_asprintf_append(buffer, "\n");
		}
	}

	void PrintPackedUniforms(_mesa_glsl_parse_state* State)
	{
		PrintPackedGlobals(State);

		if (State->bFlattenUniformBuffers && !State->CBuffersOriginal.empty())
		{
			PrintPackedUniformBuffers(State);
		}
	}

	/**
	 * Print a list of external variables.
	 */
	void print_extern_vars(_mesa_glsl_parse_state* State, exec_list* extern_vars)
	{
		const char *type_str[] = { "u", "i", "f", "f", "b", "t", "?", "?", "?", "?", "s", "os", "im", "ip", "op" };
		const char *col_str[] = { "", "", "2x", "3x", "4x" };
		const char *row_str[] = { "", "1", "2", "3", "4" };

		check( sizeof(type_str)/sizeof(char*) == GLSL_TYPE_MAX);

		bool need_comma = false;
		foreach_iter(exec_list_iterator, iter, *extern_vars)
		{
			ir_variable* var = ((extern_var*)iter.get())->var;
			const glsl_type* type = var->type;
			if (!strcmp(var->name,"gl_in"))
			{
				// Ignore it, as we can't properly frame this information in current format, and it's not used anyway for geometry shaders
				continue;
			}
			if (!strncmp(var->name,"in_",3) || !strncmp(var->name,"out_",4))
			{
				if (type->is_record())
				{
					// This is the specific case for GLSL >= 150, as we generate a struct with a member for each interpolator (which we still want to count)
					if (type->length != 1)
					{
						_mesa_glsl_warning(State, "Found a complex structure as in/out, counting is not implemented yet...\n");
						continue;
					}

					type = type->fields.structure->type;
				}

				// In and out variables may be packed in structures, or array of structures.
				// But we're interested only in those that aren't, ie. inputs for vertex shader and outputs for pixel shader.
				if (type->is_array() || type->is_record())
				{
					continue;
				}
			}
			bool is_array = type->is_array();
			int array_size = is_array ? type->length : 0;
			if (is_array)
			{
				type = type->fields.array;
			}
			ralloc_asprintf_append(buffer, "%s%s%s%s",
				need_comma ? "," : "",
				type_str[type->base_type],
				col_str[type->matrix_columns],
				row_str[type->vector_elements]);
			if (is_array)
			{
				ralloc_asprintf_append(buffer, "[%u]", array_size);
			}
			ralloc_asprintf_append(buffer, ":%s", var->name);
			need_comma = true;
		}
	}

	/**
	 * Print the input/output signature for this shader.
	 */
	void print_signature(_mesa_glsl_parse_state *state)
	{
		if (!input_variables.is_empty())
		{
			ralloc_asprintf_append(buffer, "// @Inputs: ");
			print_extern_vars(state, &input_variables);
			ralloc_asprintf_append(buffer, "\n");
		}

		if (!output_variables.is_empty())
		{
			ralloc_asprintf_append(buffer, "// @Outputs: ");
			print_extern_vars(state, &output_variables);
			ralloc_asprintf_append(buffer, "\n");
		}
		if (state->num_uniform_blocks > 0 && !state->bFlattenUniformBuffers)
		{
			bool bFirst = true;
			for (int i = 0; i < Buffers.Buffers.Num(); ++i)
			{
				// Some entries might be null, if we used more packed than real UBs used
				if (Buffers.Buffers[i])
				{
					auto* Var = Buffers.Buffers[i]->as_variable();
					if (!Var->semantic && !Var->type->is_sampler() && !Var->type->is_image())
					{
						ralloc_asprintf_append(buffer, "%s%s(%d)",
							bFirst ? "// @UniformBlocks: " : ",",
							Var->name, i);
						bFirst = false;
					}
				}
			}
			if (!bFirst)
			{
				ralloc_asprintf_append(buffer, "\n");
			}
		}

		if (state->has_packed_uniforms)
		{
			PrintPackedUniforms(state);

			if (!state->GlobalPackedArraysMap[EArrayType_Sampler].empty())
			{
				ralloc_asprintf_append(buffer, "// @Samplers: ");
				PrintPackedSamplers(
					state->GlobalPackedArraysMap[EArrayType_Sampler],
					state->TextureToSamplerMap
					);
				ralloc_asprintf_append(buffer, "\n");
			}

			if (!state->GlobalPackedArraysMap[EArrayType_Image].empty())
			{
				ralloc_asprintf_append(buffer, "// @UAVs: ");
				PrintImages(state->GlobalPackedArraysMap[EArrayType_Image]);
				ralloc_asprintf_append(buffer, "\n");
			}
		}
		else
		{
			if (!uniform_variables.is_empty())
			{
				ralloc_asprintf_append(buffer, "// @Uniforms: ");
				print_extern_vars(state, &uniform_variables);
				ralloc_asprintf_append(buffer, "\n");
			}
			if (!sampler_variables.is_empty())
			{
				ralloc_asprintf_append(buffer, "// @Samplers: ");
				print_extern_vars(state, &sampler_variables);
				ralloc_asprintf_append(buffer, "\n");
			}
			if (!image_variables.is_empty())
			{
				ralloc_asprintf_append(buffer, "// @UAVs: ");
				print_extern_vars(state, &image_variables);
				ralloc_asprintf_append(buffer, "\n");
			}
		}

		if (Buffers.UniqueSamplerStates.Num() > 0)
		{
			ralloc_asprintf_append(buffer, "// @SamplerStates: ");
			for (int32 Index = 0; Index < Buffers.UniqueSamplerStates.Num(); ++Index)
			{
				ralloc_asprintf_append(buffer, "%s%d:%s", Index > 0 ? "," : "", Index, Buffers.UniqueSamplerStates[Index].c_str());
			}
			ralloc_asprintf_append(buffer, "\n");
		}

		if (Frequency == compute_shader)
		{
			ralloc_asprintf_append(buffer, "// @NumThreads: %d, %d, %d\n", this->NumThreadsX, this->NumThreadsY, this->NumThreadsZ);
		}
		
		for (int i = 0; i < Buffers.Buffers.Num(); ++i)
		{
			if (Buffers.Buffers[i])
			{
				auto* Var = Buffers.Buffers[i]->as_variable();
				if (!Var->type->is_sampler() && !Var->type->is_image() && Var->semantic && !strcmp(Var->semantic, "u") && Var->mode == ir_var_uniform && Var->name && !strcmp(Var->name, "BufferSizes"))
				{
					ralloc_asprintf_append(buffer, "// @SideTable: %s(%d)", Var->name, i);
				}
			}
		}
	}

public:

	/** Constructor. */
	FGenerateMetalVisitor(FMetalCodeBackend* InBackend, _mesa_glsl_parse_state* InParseState, _mesa_glsl_parser_targets InFrequency, FBuffers& InBuffers)
		: Backend(InBackend)
		, ParseState(InParseState)
		, Frequency(InFrequency)
		, Buffers(InBuffers)
		, buffer(0)
		, indentation(0)
		, scope_depth(0)
		, ExpressionDepth(0)
		, temp_id(0)
		, global_id(0)
		, needs_semicolon(false)
		, IsMain(false)
		, should_print_uint_literals_as_ints(false)
		, loop_count(0)
		, bStageInEmitted(false)
		, bUsePacked(false)
		, bNeedsComputeInclude(false)
	{
		printable_names = hash_table_ctor(32, hash_table_pointer_hash, hash_table_pointer_compare);
		used_structures = hash_table_ctor(128, hash_table_pointer_hash, hash_table_pointer_compare);
		used_uniform_blocks = hash_table_ctor(32, hash_table_string_hash, hash_table_string_compare);
	}

	/** Destructor. */
	virtual ~FGenerateMetalVisitor()
	{
		hash_table_dtor(printable_names);
		hash_table_dtor(used_structures);
		hash_table_dtor(used_uniform_blocks);
	}

	/**
	 * Executes the visitor on the provided ir.
	 * @returns the Metal source code generated.
	 */
	const char* run(exec_list* ir)
	{
		mem_ctx = ralloc_context(NULL);

		char* code_buffer = ralloc_asprintf(mem_ctx, "");
		buffer = &code_buffer;

		foreach_iter(exec_list_iterator, iter, *ir)
		{
			ir_instruction *inst = (ir_instruction *)iter.get();
			do_visit(inst);
		}
		buffer = 0;

		char* decl_buffer = ralloc_asprintf(mem_ctx, "");
		buffer = &decl_buffer;
		declare_structs(ParseState);
		buffer = 0;

		char* signature = ralloc_asprintf(mem_ctx, "");
		buffer = &signature;
		print_signature(ParseState);
		buffer = 0;

		char* full_buffer = ralloc_asprintf(
			ParseState,
			"// Compiled by HLSLCC\n%s\n#include <metal_stdlib>\n%s\nusing namespace metal;\n\n%s%s",
			signature,
			bNeedsComputeInclude ? "#include <metal_compute>" : "",
			decl_buffer,
			code_buffer
			);
		ralloc_free(mem_ctx);

		return full_buffer;
	}
};

char* FMetalCodeBackend::GenerateCode(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency)
{
	// We'll need this Buffers info for the [[buffer()]] index
	FBuffers Buffers;
	FGenerateMetalVisitor visitor(this, state, state->target, Buffers);

	// At this point, all inputs and outputs are global uniforms, no structures.

	// Promotes all inputs from half to float to avoid stage_in issues
	PromoteInputsAndOutputsGlobalHalfToFloat(ir, state, Frequency);

	// Move all inputs & outputs to structs for Metal
	PackInputsAndOutputs(ir, state, Frequency, visitor.input_variables);

	// ir_var_uniform instances be global, so move them as arguments to main
	MovePackedUniformsToMain(ir, state, Buffers);

	//@todo-rco: Do we need this here?
	ExpandArrayAssignments(ir, state);

	// Fix any special language extensions (FrameBufferFetchES2() intrinsic)
	FixIntrinsics(ir, state);

	// Remove half->float->half or float->half->float
	FixRedundantCasts(ir);

	if (!OptimizeAndValidate(ir, state))
	{
		return nullptr;
	}

	// Do not call Optimize() after this!
	{
		// Metal can't do implicit conversions between half<->float during math expressions
		BreakPrecisionChangesVisitor(ir, state);

		// Metal can't read from a packed_* type, which for us come from a constant buffer
		//@todo-rco: Might not work if accessing packed_half* m[N]!
		RemovePackedVarReferences(ir, state);

		FindAtomicVariables(ir, Buffers.AtomicVariables);

		bool bConvertUniformsToFloats = (HlslCompileFlags & HLSLCC_FlattenUniformBuffers) != HLSLCC_FlattenUniformBuffers;
		ConvertHalfToFloatUniformsAndSamples(ir, state, bConvertUniformsToFloats, true);

		Validate(ir, state);
	}
//IRDump(ir);
	// Generate the actual code string
	const char* code = visitor.run(ir);
	return _strdup(code);
}

struct FMetalCheckNonComputeRestrictionsVisitor : public ir_hierarchical_visitor
{
	_mesa_glsl_parse_state* ParseState;
	bool bErrors;
	FMetalCheckNonComputeRestrictionsVisitor(_mesa_glsl_parse_state* InParseState)
		: ParseState(InParseState)
		, bErrors(false)
	{
	}

	virtual ir_visitor_status visit(ir_variable* IR) override
	{
		if (IR->type && IR->type->is_image())
		{
			if (IR->name)
			{
				_mesa_glsl_error(ParseState, "Metal doesn't allow UAV '%s' on non-compute shader stages.", IR->name);
			}
			else
			{
				_mesa_glsl_error(ParseState, "Metal doesn't allow UAV on non-compute shader stages.");
			}
			bErrors = true;
			return visit_stop;
		}

		return visit_continue;
	}
}; 

struct FMetalCheckComputeRestrictionsVisitor : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* ParseState;
	bool bErrors;
	enum
	{
		Read = 1,
		Write = 2,
		ReadWrite = Read | Write,
	};
	TMap<ir_variable*, uint32> ImageRW;

	FMetalCheckComputeRestrictionsVisitor(_mesa_glsl_parse_state* InParseState)
		: ParseState(InParseState)
		, bErrors(false)
	{
	}

	virtual ir_visitor_status visit(ir_variable* IR) override
	{
		if (IR->type && IR->type->is_image())
		{
			ImageRW.Add(IR, 0);
		}

		return ir_rvalue_visitor::visit(IR);
	}

	void VerifyDeReference(ir_dereference* DeRef, bool bWrite)
	{
		auto* Var = DeRef->variable_referenced();
		if (Var && Var->type && Var->type->is_image() && !Var->type->sampler_buffer)
		{
			if (bWrite)
			{
				ImageRW[Var] |= Write;
			}
			else
			{
				ImageRW[Var] |= Read;
			}

			if (ImageRW[Var] == ReadWrite)
			{
				_mesa_glsl_error(ParseState, "Metal doesn't allow simultaneous read & write on RWTexture(s) %s%s%s", Var->name ? "(" : "", Var->name ? Var->name : "", Var->name ? ")" : "");
				bErrors = true;
			}
		}
	}

	ir_visitor_status visit_leave(ir_assignment *ir) override
	{
		auto ReturnValue = ir_rvalue_visitor::visit_leave(ir);
		if (ReturnValue != visit_stop)
		{
			VerifyDeReference(ir->lhs, true);
			if (bErrors)
			{
				return visit_stop;
			}
		}
		return ReturnValue;
	}

	virtual void handle_rvalue(ir_rvalue **rvalue) override
	{
		if (rvalue && *rvalue)
		{
			auto* DeRef = (*rvalue)->as_dereference();
			if (DeRef)
			{
				VerifyDeReference(DeRef, in_assignee);
			}
		}
	}
};


bool FMetalCodeBackend::ApplyAndVerifyPlatformRestrictions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency)
{
	if (Frequency == HSF_ComputeShader)
	{
		FMetalCheckComputeRestrictionsVisitor Visitor(ParseState);
		Visitor.run(Instructions);

		return !Visitor.bErrors;
	}
	else
	{
		FMetalCheckNonComputeRestrictionsVisitor Visitor(ParseState);
		Visitor.run(Instructions);

		return !Visitor.bErrors;
	}
}

bool FMetalCodeBackend::GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
	auto* EntryPointSig = FindEntryPointFunction(Instructions, ParseState, EntryPoint);
	if (!EntryPointSig)
	{
		_mesa_glsl_error(ParseState, "shader entry point '%s' not found", EntryPoint);
		return false;
	}

	exec_list DeclInstructions;
	exec_list PreCallInstructions;
	exec_list ArgInstructions;
	exec_list PostCallInstructions;

	ParseState->symbols->push_scope();

	// Find all system semantics and generate in/out globals
	foreach_iter(exec_list_iterator, Iter, EntryPointSig->parameters)
	{
		const ir_variable* Variable = (ir_variable*)Iter.get();
		if (Variable->semantic != NULL || Variable->type->is_record())
		{
			ir_dereference_variable* ArgVarDeref = NULL;
			switch (Variable->mode)
			{
			case ir_var_in:
				ArgVarDeref = MetalUtils::GenerateInput(
					Frequency, bIsDesktop,
					ParseState,
					Variable->semantic,
					Variable->type,
					&DeclInstructions,
					&PreCallInstructions
					);
				break;
			case ir_var_out:
				ArgVarDeref = MetalUtils::GenerateOutput(
					Frequency, bIsDesktop,
					ParseState,
					Variable->semantic,
					Variable->type,
					&DeclInstructions,
					&PreCallInstructions,
					&PostCallInstructions
					);
				break;
			default:
			   _mesa_glsl_error(
				   ParseState,
				   "entry point parameter '%s' must be an input or output",
				   Variable->name
				   );
			}
			
			ArgInstructions.push_tail(ArgVarDeref);
		}
	}


	// The function's return value should have an output semantic if it's not void.
	ir_dereference_variable* EntryPointReturn = nullptr;
	if (!EntryPointSig->return_type->is_void())
	{
		EntryPointReturn = MetalUtils::GenerateOutput(Frequency, bIsDesktop, ParseState, EntryPointSig->return_semantic, EntryPointSig->return_type, &DeclInstructions, &PreCallInstructions, &PostCallInstructions);
	}

	ParseState->symbols->pop_scope();

	// Generate the Main() function signature
	ir_function_signature* MainSig = new(ParseState) ir_function_signature(glsl_type::void_type);
	MainSig->is_defined = true;
	MainSig->is_main = true;
	MainSig->body.append_list(&PreCallInstructions);
	// Call the original EntryPoint
	MainSig->body.push_tail(new(ParseState) ir_call(EntryPointSig, EntryPointReturn, &ArgInstructions));
	MainSig->body.append_list(&PostCallInstructions);
	MainSig->wg_size_x = EntryPointSig->wg_size_x;
	MainSig->wg_size_y = EntryPointSig->wg_size_y;
	MainSig->wg_size_z = EntryPointSig->wg_size_z;

	// Generate the Main() function
	auto* MainFunction = new(ParseState)ir_function("Main");
	MainFunction->add_signature(MainSig);
	// Adds uniforms as globals
	Instructions->append_list(&DeclInstructions);
	Instructions->push_tail(MainFunction);

	// Now that we have a proper Main(), move global setup to Main().
	MoveGlobalInstructionsToMain(Instructions);
//IRDump(Instructions);
	return true;
}

FMetalCodeBackend::FMetalCodeBackend(unsigned int InHlslCompileFlags, EHlslCompileTarget InTarget, bool bInDesktop, bool bInZeroInitialise, bool bInBoundsChecks) :
	FCodeBackend(InHlslCompileFlags, HCT_FeatureLevelES3_1)
{
	bIsDesktop = bInDesktop;
	bZeroInitialise = bInZeroInitialise;
	bBoundsChecks = bInBoundsChecks;
}

void FMetalLanguageSpec::SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir)
{
	// Framebuffer fetch
	{
		// Leave original fb ES2 fetch function as that's what the hlsl expects
		make_intrinsic_genType(ir, State, FRAMEBUFFER_FETCH_ES2, ir_invalid_opcode, IR_INTRINSIC_HALF, 0, 4, 4);

		// MRTs; first make intrinsics for each MRT, then a non-intrinsic version to use that (helps when converting to Metal)
		for (int i = 0; i < MAX_SIMULTANEOUS_RENDER_TARGETS; ++i)
		{
			char FunctionName[32];
#if PLATFORM_WINDOWS
			sprintf_s(FunctionName, 32, "%s%d", FRAMEBUFFER_FETCH_MRT, i);
#else
			sprintf(FunctionName, "%s%d", FRAMEBUFFER_FETCH_MRT, i);
#endif
			make_intrinsic_genType(ir, State, FunctionName, ir_invalid_opcode, IR_INTRINSIC_HALF, 0, 4, 4);
		}

		const auto* ReturnType = glsl_type::get_instance(GLSL_TYPE_HALF, 4, 1);
		ir_function* Func = new(State) ir_function(FRAMEBUFFER_FETCH_MRT);
		ir_function_signature* Sig = new(State) ir_function_signature(ReturnType);
		//Sig->is_builtin = true;
		Sig->is_defined = true;
		ir_variable* MRTIndex = new(State) ir_variable(glsl_type::int_type, "Arg0", ir_var_in);
		Sig->parameters.push_tail(MRTIndex);

		for (int i = 0; i < MAX_SIMULTANEOUS_RENDER_TARGETS; ++i)
		{
			// Inject:
			//	if (Arg0 == i) FRAMEBUFFER_FETCH_MRT#i();
			auto* Condition = new(State) ir_expression(ir_binop_equal, new(State) ir_dereference_variable((ir_variable*)Sig->parameters.get_head()), new(State) ir_constant(i));
			auto* If = new(State) ir_if(Condition);
			char FunctionName[32];
#if PLATFORM_WINDOWS
			sprintf_s(FunctionName, 32, "%s%d", FRAMEBUFFER_FETCH_MRT, i);
#else
			sprintf(FunctionName, "%s%d", FRAMEBUFFER_FETCH_MRT, i);
#endif
			auto* IntrinsicSig = FCodeBackend::FindEntryPointFunction(ir, State, FunctionName);
			auto* ReturnValue = new(State) ir_variable(ReturnType, nullptr, ir_var_temporary);
			exec_list Empty;
			auto* Call = new(State) ir_call(IntrinsicSig, new(State) ir_dereference_variable(ReturnValue), &Empty);
			Call->use_builtin = true;
			If->then_instructions.push_tail(ReturnValue);
			If->then_instructions.push_tail(Call);
			If->then_instructions.push_tail(new(State) ir_return(new(State) ir_dereference_variable(ReturnValue)));
			Sig->body.push_tail(If);
		}

		Func->add_signature(Sig);

		State->symbols->add_global_function(Func);
		ir->push_tail(Func);
	}

	// Memory sync/barriers
	
	{
		make_intrinsic_genType(ir, State, GROUP_MEMORY_BARRIER, ir_invalid_opcode, IR_INTRINSIC_RETURNS_VOID, 0, 0, 0);
		make_intrinsic_genType(ir, State, GROUP_MEMORY_BARRIER_WITH_GROUP_SYNC, ir_invalid_opcode, IR_INTRINSIC_RETURNS_VOID, 0, 0, 0);
		make_intrinsic_genType(ir, State, DEVICE_MEMORY_BARRIER, ir_invalid_opcode, IR_INTRINSIC_RETURNS_VOID, 0, 0, 0);
		make_intrinsic_genType(ir, State, DEVICE_MEMORY_BARRIER_WITH_GROUP_SYNC, ir_invalid_opcode, IR_INTRINSIC_RETURNS_VOID, 0, 0, 0);
		make_intrinsic_genType(ir, State, ALL_MEMORY_BARRIER, ir_invalid_opcode, IR_INTRINSIC_RETURNS_VOID, 0, 0, 0);
		make_intrinsic_genType(ir, State, ALL_MEMORY_BARRIER_WITH_GROUP_SYNC, ir_invalid_opcode, IR_INTRINSIC_RETURNS_VOID, 0, 0, 0);
	}
}
