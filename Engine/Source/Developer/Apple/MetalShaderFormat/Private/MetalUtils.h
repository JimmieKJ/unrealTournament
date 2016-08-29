// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "hlslcc.h"
#include "ir.h"
#include "PackUniformBuffers.h"
#include "CustomStdAllocator.h"

inline FCustomStdString FixVecPrefix(FCustomStdString Type)
{
	if (!strncmp("vec", Type.c_str(), 3))
	{
		FCustomStdString Num = Type.substr(3);
		Type = "float";
		Type += Num;
	}
	else if (!strncmp("bvec", Type.c_str(), 4))
	{
		FCustomStdString Num = Type.substr(4);
		Type = "bool";
		Type += Num;
	}
	else if (!strncmp("ivec", Type.c_str(), 4))
	{
		FCustomStdString Num = Type.substr(4);
		Type = "int";
		Type += Num;
	}
	else if (!strncmp("uvec", Type.c_str(), 4))
	{
		FCustomStdString Num = Type.substr(4);
		Type = "uint";
		Type += Num;
	}
	else if (!strncmp("mat", Type.c_str(), 3))
	{
		FCustomStdString Num = Type.substr(3);
		Type = "float";
		Type += Num;
		Type += "x";
		Type += Num;
	}

	return Type;
}

/** Track external variables. */
struct extern_var : public exec_node
{
	ir_variable* var;
	explicit extern_var(ir_variable* in_var) : var(in_var) {}
};


struct FBuffers
{
	TIRVarSet AtomicVariables;

	TArray<class ir_instruction*> Buffers;
    
    TArray<class ir_instruction*> Textures;

	// Information about textures & samplers; we need to have unique samplerstate indices, as one they can be used independent of each other
	TArray<FCustomStdString> UniqueSamplerStates;

	int32 GetUniqueSamplerStateIndex(const FCustomStdString& Name, bool bAddIfNotFound, bool& bOutAdded)
	{
		int32 Found = INDEX_NONE;
		bOutAdded = false;
		if (UniqueSamplerStates.Find(Name, Found))
		{
			return Found;
		}
		
		if (bAddIfNotFound)
		{
			UniqueSamplerStates.Add(Name);
			bOutAdded = true;
			return UniqueSamplerStates.Num() - 1;
		}

		return Found;
	}

	int GetIndex(ir_variable* Var, bool bIsDesktop)
	{
		for (int i = 0, n = Buffers.Num(); i < n; ++i)
		{
			if (Buffers[i] == Var)
			{
				if (Var->type->is_image())
				{
					return 30 - i;
				}
				return i;
			}
        }
        
        for (int i = 0, n = Textures.Num(); i < n; ++i)
        {
            if (Textures[i] == Var)
            {
                if (Var->type->is_image())
                {
                    return (bIsDesktop ? 126 : 30) - i;
                }
                return i;
            }
        }

		return -1;
	}

	int GetIndex(const FCustomStdString& Name, bool bIsDesktop)
	{
		for (int i = 0, n = Buffers.Num(); i < n; ++i)
		{
			auto* Var = Buffers[i] ? Buffers[i]->as_variable() : nullptr;
			if (Var && Var->name && Var->name == Name)
			{
				if (Var->type->is_image())
				{
					return 30 - i;
				}
				return i;
			}
		}
        
        for (int i = 0, n = Textures.Num(); i < n; ++i)
        {
            auto* Var = Textures[i] ? Textures[i]->as_variable() : nullptr;
            if (Var && Var->name && Var->name == Name)
            {
                if (Var->type->is_image())
                {
                    return (bIsDesktop ? 126 : 30) - i;
                }
                return i;
            }
        }

		return -1;
	}

	void SortBuffers()
	{
		TArray<class ir_instruction*> AllBuffers;
		AllBuffers.AddZeroed(Buffers.Num());
		TIRVarList CBuffers;
		// Put packed UB's into their location (h=0, m=1, etc); leave holes if not using a packed define
		// and group the regular CBuffers in another list
		for (int i = 0, n = Buffers.Num(); i < n; ++i)
		{
			auto* Var = Buffers[i]->as_variable();
			check(Var);
			if (Var->semantic && strlen(Var->semantic) == 1)
			{
				int Index = ConvertArrayTypeToIndex((EArrayType)Var->semantic[0]);
				if (AllBuffers.Num() <= Index)
				{
					int32 Count = Index + 1 - AllBuffers.Num();
					AllBuffers.AddZeroed(Count);
					//AllBuffers.Resize(Index + 1, nullptr);
				}
				AllBuffers[Index] = Var;
			}
			else
			{
				CBuffers.push_back(Var);
			}
		}

		// Fill the holes in the packed array list with real UB's
		for (int i = 0; i < AllBuffers.Num() && !CBuffers.empty(); ++i)
		{
			if (!AllBuffers[i])
			{
				AllBuffers[i] = CBuffers.front();
				CBuffers.erase(CBuffers.begin());
			}
		}

		Buffers = AllBuffers;
	}
};

const glsl_type* PromoteHalfToFloatType(_mesa_glsl_parse_state* state, const glsl_type* type);

namespace MetalUtils
{
	ir_dereference_variable* GenerateInput(EHlslShaderFrequency Frequency, bool bIsDesktop, _mesa_glsl_parse_state* ParseState, const char* InputSemantic,
		const glsl_type* InputType, exec_list* DeclInstructions, exec_list* PreCallInstructions);

	ir_dereference_variable* GenerateOutput(EHlslShaderFrequency Frequency, bool bIsDesktop, _mesa_glsl_parse_state* ParseState, const char* OutputSemantic,
		const glsl_type* OutputType, exec_list* DeclInstructions, exec_list* PreCallInstructions, exec_list* PostCallInstructions);
}

const int MAX_SIMULTANEOUS_RENDER_TARGETS = 8;
