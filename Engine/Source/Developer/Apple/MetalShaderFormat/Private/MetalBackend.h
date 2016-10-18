// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "hlslcc.h"
PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include "ir.h"
#include "PackUniformBuffers.h"
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
#include "LanguageSpec.h"


class FMetalLanguageSpec : public ILanguageSpec
{
public:
	virtual bool SupportsDeterminantIntrinsic() const override
	{
		//@todo-rco: Temp workaround for Seed 2 & 3
		return false;// true;
	}

	virtual bool SupportsTransposeIntrinsic() const override
	{
		//@todo-rco: Temp workaround for Seed 2 & 3
		return false;// true;
	}
	virtual bool SupportsIntegerModulo() const override { return true; }

	virtual bool SupportsMatrixConversions() const override { return false; }

	virtual void SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir) override;

	virtual bool AllowsSharingSamplers() const override { return true; }

	virtual bool UseSamplerInnerType() const override { return true; }

	virtual bool CanConvertBetweenHalfAndFloat() const override { return false; }

	virtual bool NeedsAtomicLoadStore() const override { return true; }
};

struct FBuffers;

// Generates Metal compliant code from IR tokens
struct FMetalCodeBackend : public FCodeBackend
{
	FMetalCodeBackend(unsigned int InHlslCompileFlags, EHlslCompileTarget InTarget, bool bInDesktop, bool bInZeroInitialise, bool bInBoundsChecks);

	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override;

	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) override;

	// Return false if there were restrictions that made compilation fail
	virtual bool ApplyAndVerifyPlatformRestrictions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override;

	void PackInputsAndOutputs(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency, exec_list& InputVars);
	void MovePackedUniformsToMain(exec_list* ir, _mesa_glsl_parse_state* state, FBuffers& OutBuffers);
	void FixIntrinsics(exec_list* ir, _mesa_glsl_parse_state* state);
	void RemovePackedVarReferences(exec_list* ir, _mesa_glsl_parse_state* State);
	void PromoteInputsAndOutputsGlobalHalfToFloat(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency);
	void ConvertHalfToFloatUniformsAndSamples(exec_list* ir, _mesa_glsl_parse_state* State, bool bConvertUniforms, bool bConvertSamples);
	void BreakPrecisionChangesVisitor(exec_list* ir, _mesa_glsl_parse_state* State);

	bool bIsDesktop;
	bool bZeroInitialise;
	bool bBoundsChecks;
};
