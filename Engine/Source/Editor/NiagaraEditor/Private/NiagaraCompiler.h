// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "INiagaraCompiler.h"
#include "CompilerResultsLog.h"
#include "Niagara/NiagaraScriptConstantData.h"

/** Base class for Niagara compilers. Children of this will include a compiler for the VectorVM and for Compute shaders. Possibly others. */
class NIAGARAEDITOR_API FNiagaraCompiler : public INiagaraCompiler
{
protected:
	/** The script we are compiling. */
	UNiagaraScript* Script;

	/** The source of the script we are compiling. */
	UNiagaraScriptSource* Source;

	/** The set of expressions generated from the script source. */
	TArray<TNiagaraExprPtr> Expressions;

	/** The particle attribute names. */
	TArray<FName> Attributes;
	
	//All internal and external constants used in the graph.
	FNiagaraScriptConstantData ConstantData;

	/** Map of Pins to expressions. Allows us to reuse expressions for pins that have already been compiled. */
	TMap<UEdGraphPin*, TNiagaraExprPtr> PinToExpression;

	//Message log. Automatically handles marking the NodeGraph with errors.
	FCompilerResultsLog MessageLog;

	/** Compiles an output Pin on a graph node. Caches the result for any future inputs connected to it. */
	TNiagaraExprPtr CompileOutputPin(UEdGraphPin* Pin);

	/** Creates an expression retrieving a particle attribute. */
	TNiagaraExprPtr Expression_GetAttribute(FName AttributeName);

	/** Creates an expression retrieving an external constant. */
	TNiagaraExprPtr Expression_GetExternalConstant(FName ConstantName, ENiagaraDataType Type);

	/** Creates an expression retrieving an internal constant. */
	TNiagaraExprPtr Expression_GetInternalConstant(FName ConstantName, ENiagaraDataType Type);

	/** Creates an expression collecting some source expressions together for use by future expressions. */
	TNiagaraExprPtr Expression_Collection(TArray<TNiagaraExprPtr>& SourceExpressions);

	/** Sets the value of a constant. Overwrites the value if it already exists. */
	template< typename T >
	void SetOrAddConstant(bool bInternal, FName Name, const T& Default);

public:

	//Begin INiagaraCompiler Interface
	virtual TNiagaraExprPtr CompilePin(UEdGraphPin* Pin)override;
	virtual void GetParticleAttributes(TArray<FName>& OutAttributes)override;
	virtual TNiagaraExprPtr GetAttribute(FName AttributeName)override;
	virtual TNiagaraExprPtr GetExternalConstant(FName ConstantName, float Default)override;
	virtual TNiagaraExprPtr GetExternalConstant(FName ConstantName, FVector4 Default)override;
	virtual TNiagaraExprPtr GetExternalConstant(FName ConstantName, const FMatrix& Default)override;

	virtual void CheckInputs(FName OpName, TArray<TNiagaraExprPtr>& Inputs)override;
	virtual void CheckOutputs(FName OpName, TArray<TNiagaraExprPtr>& Outputs)override;
	//End INiagaraCompiler Interface
	
	/** Gets the index into a constants table of the constant specified by Name and bInternal. */
	virtual void GetConstantResultIndex(FName Name, bool bInternal, int32& OutResultIndex, int32& OutComponentIndex) = 0;
	/**	Gets the index of a named attribute into either the input or output data. */
	int32 GetAttributeIndex(FName Name);

	/**	Gets the index of a free temporary location. */
	virtual int32 AquireTemporary() = 0;
	/** Frees the passed temporary index for use by other expressions. */
	virtual void FreeTemporary(int32 TempIndex) = 0;
};

template< typename T >
void FNiagaraCompiler::SetOrAddConstant(bool bInternal, FName Name, const T& Default)
{
	if (bInternal)
		ConstantData.SetOrAddInternal(Name, Default);
	else
		ConstantData.SetOrAddExternal(Name, Default);
}