// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Compiler version.
 */
enum
{
	HLSLCC_VersionMajor = 0,
	HLSLCC_VersionMinor = 59,
};

/**
 * The shader frequency.
 */
enum EHlslShaderFrequency
{
	HSF_VertexShader,
	HSF_PixelShader,
	HSF_GeometryShader,
	HSF_HullShader,
	HSF_DomainShader,
	HSF_ComputeShader,
	HSF_FrequencyCount,
	HSF_InvalidFrequency = -1
};

/**
 * Compilation flags. See PackUniformBuffers.h for details on Grouping/Packing uniforms.
 */
enum EHlslCompileFlag
{
	/** Disables validation of the IR. */
	HLSLCC_NoValidation = 0x1,
	/** Disabled preprocessing. */
	HLSLCC_NoPreprocess = 0x2,
	/** Pack uniforms into typed arrays. */
	HLSLCC_PackUniforms = 0x4,
	/** Assume that input shaders output into DX11 clip space,
	 * and adjust them for OpenGL clip space. */
	HLSLCC_DX11ClipSpace = 0x8,
	/** Print AST for debug purposes. */
	HLSLCC_PrintAST = 0x10,
	// Removed any structures embedded on uniform buffers flattens them into elements of the uniform buffer (Mostly for ES 2: this implies PackUniforms).
	HLSLCC_FlattenUniformBufferStructures = 0x20 | HLSLCC_PackUniforms,
	// Removes uniform buffers and flattens them into globals (Mostly for ES 2: this implies PackUniforms & Flatten Structures).
	HLSLCC_FlattenUniformBuffers = 0x40 | HLSLCC_PackUniforms | HLSLCC_FlattenUniformBufferStructures,
	// Groups flattened uniform buffers per uniform buffer source/precision (Implies Flatten UBs)
	HLSLCC_GroupFlattenedUniformBuffers = 0x80 | HLSLCC_FlattenUniformBuffers,
	// Remove redundant subexpressions [including texture fetches] (to workaround certain drivers who can't optimize redundant texture fetches)
	HLSLCC_ApplyCommonSubexpressionElimination = 0x100,
	// Expand subexpressions/obfuscate (to workaround certain drivers who can't deal with long nested expressions)
	HLSLCC_ExpandSubexpressions = 0x200,
	// Generate shaders compatible with the separate_shader_objects extension
	HLSLCC_SeparateShaderObjects = 0x400,
};

/**
 * Cross compilation target.
 */
enum EHlslCompileTarget
{
	HCT_FeatureLevelSM4,	// Equivalent to GLSL 1.50
	HCT_FeatureLevelES3_1Ext,// Equivalent to GLSL ES 310 + extensions
	HCT_FeatureLevelSM5,	// Equivalent to GLSL 4.3
	HCT_FeatureLevelES2,	// Equivalent to GLSL ES2 1.00
	HCT_FeatureLevelES3_1,	// Equivalent to GLSL ES 3.1

	/** Invalid target sentinel. */
	HCT_InvalidTarget,
};

class ir_function_signature;

// Interface for generating source code
class FCodeBackend
{
protected:
	// Built from EHlslCompileFlag
	const unsigned int HlslCompileFlags;

public:
	FCodeBackend(unsigned int InHlslCompileFlags) : HlslCompileFlags(InHlslCompileFlags) {}
	virtual ~FCodeBackend() {}

	/**
	* Generate GLSL code for the given instructions.
	* @param ir - IR instructions.
	* @param ParseState - Parse state.
	* @returns Target source code that implements the IR instructions (Glsl, etc).
	*/
	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) = 0;

	// Returns false if there were restrictions that made compilation fail
	virtual bool ApplyAndVerifyPlatformRestrictions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) { return true; }

	// Returns false if any issues
	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) { return false; }

	// Returns false if any issues. This should be called after every specialized step that might modify IR.
	inline bool OptimizeAndValidate(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
	{
		if (Optimize(Instructions, ParseState))
		{
			return Validate(Instructions, ParseState);
		}
		return false;
	}

	bool Optimize(exec_list* Instructions, _mesa_glsl_parse_state* ParseState);
	bool Validate(exec_list* Instructions, _mesa_glsl_parse_state* ParseState);

	static ir_function_signature* FindEntryPointFunction(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, const char* EntryPoint);

protected:
	ir_function_signature* GetMainFunction(exec_list* Instructions);
};

/**
 * Cross compile HLSL shader code to GLSL.
 * @param InSourceFilename - The filename of the shader source code. This file
 *                           is referenced when generating compile errors.
 * @param InShaderSource - HLSL shader source code.
 * @param InEntryPoint - The name of the entry point.
 * @param InShaderFrequency - The shader frequency.
 * @param InFlags - Flags, see the EHlslCompileFlag enum.
 * @param InCompileTarget - Cross compilation target.
 * @param OutShaderSource - Upon return contains GLSL shader source.
 * @param OutErrorLog - Upon return contains the error log, if any.
 * @returns 0 if compilation failed, non-zero otherwise.
 */
#ifdef __GNUC__
__attribute__ ((visibility("default")))
#endif // __GNUC__
int HlslCrossCompile(
	const char* InSourceFilename,
	const char* InShaderSource,
	const char* InEntryPoint,
	EHlslShaderFrequency InShaderFrequency,
	FCodeBackend* InShaderBackEnd,
	struct ILanguageSpec* InLanguageSpec,
	unsigned int InFlags,
	EHlslCompileTarget InCompileTarget,
	char** OutShaderSource,
	char** OutErrorLog
	);
