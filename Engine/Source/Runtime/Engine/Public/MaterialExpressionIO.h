// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//
//	FExpressionInput
//

//@warning: FExpressionInput is mirrored in MaterialExpression.h and manually "subclassed" in Material.h (FMaterialInput)
struct FExpressionInput
{
	/** Material expression that this input is connected to, or NULL if not connected. */
	class UMaterialExpression*	Expression;

	/** Index into Expression's outputs array that this input is connected to. */
	int32						OutputIndex;

	/** 
	 * Optional name of the input.  
	 * Note that this is the only member which is not derived from the output currently connected. 
	 */
	FString						InputName;

	int32						Mask,
								MaskR,
								MaskG,
								MaskB,
								MaskA;
	uint32						GCC64Padding; // @todo 64: if the C++ didn't mismirror this structure, we might not need this

	FExpressionInput()
		: Expression(NULL)
		, OutputIndex(0)
		, Mask(0)
		, MaskR(0)
		, MaskG(0)
		, MaskB(0)
		, MaskA(0)
		, GCC64Padding(0)
	{
	}

	ENGINE_API int32 Compile(class FMaterialCompiler* Compiler, int32 MultiplexIndex=INDEX_NONE);

	/**
	 * Tests if the input has a material expression connected to it
	 *
	 * @return	true if an expression is connected, otherwise false
	 */
	bool IsConnected() const { return (NULL != Expression); }

	/** Connects output of InExpression to this input */
	ENGINE_API void Connect( int32 InOutputIndex, class UMaterialExpression* InExpression );
};

//
//	FExpressionOutput
//

struct FExpressionOutput
{
	FString	OutputName;
	int32	Mask,
		MaskR,
		MaskG,
		MaskB,
		MaskA;

	FExpressionOutput(int32 InMask = 0, int32 InMaskR = 0, int32 InMaskG = 0, int32 InMaskB = 0, int32 InMaskA = 0) :
		Mask(InMask),
		MaskR(InMaskR),
		MaskG(InMaskG),
		MaskB(InMaskB),
		MaskA(InMaskA)
	{}

	FExpressionOutput(FString InOutputName, int32 InMask = 0, int32 InMaskR = 0, int32 InMaskG = 0, int32 InMaskB = 0, int32 InMaskA = 0) :
		OutputName(InOutputName),
		Mask(InMask),
		MaskR(InMaskR),
		MaskG(InMaskG),
		MaskB(InMaskB),
		MaskA(InMaskA)
	{}
};

//
//	FMaterialInput
//

template<class InputType> struct FMaterialInput : FExpressionInput
{
	uint32	UseConstant : 1;
	InputType	Constant;
};

struct FColorMaterialInput : FMaterialInput<FColor>
{
	ENGINE_API int32 CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property);
};
struct FScalarMaterialInput : FMaterialInput<float>
{
	ENGINE_API int32 CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property);
};
struct FVectorMaterialInput : FMaterialInput<FVector>
{
	ENGINE_API int32 CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property);
};
struct FVector2MaterialInput : FMaterialInput<FVector2D>
{
	ENGINE_API int32 CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property);
};
struct FMaterialAttributesInput : FMaterialInput<int32>
{
	FMaterialAttributesInput() : FMaterialInput<int32>(), PropertyConnectedBitmask(0)
	{ 
		// ensure PropertyConnectedBitmask can contain all properties.
		static_assert((uint32)(MP_MAX)-1 <= (8 * sizeof(PropertyConnectedBitmask)), "PropertyConnectedBitmask cannot contain entire EMaterialProperty enumeration.");
	}

	ENGINE_API int32 CompileWithDefault(class FMaterialCompiler* Compiler, EMaterialProperty Property);
	ENGINE_API bool IsConnected(EMaterialProperty Property) { return ((PropertyConnectedBitmask >> (uint32)Property) & 0x1) != 0; }
	ENGINE_API void SetConnectedProperty(EMaterialProperty Property, bool bIsConnected) 
	{
		PropertyConnectedBitmask = bIsConnected ? PropertyConnectedBitmask | (1 << (uint32)Property) : PropertyConnectedBitmask & ~(1 << (uint32)Property);
	}

	// each bit corresponds to EMaterialProperty connection status.
	uint32 PropertyConnectedBitmask;
};