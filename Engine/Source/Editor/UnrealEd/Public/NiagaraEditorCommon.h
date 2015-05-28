// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"


//List of operations supported by the Niagara compiler(s) as visible from the outside.
//Additional information about each operation is added in FNiagaraOpInfo::Init
#define NiagaraOpList\
	NiagaraOp(Add)\
	NiagaraOp(Subtract)\
	NiagaraOp(Multiply)\
	NiagaraOp(MultiplyAdd)\
	NiagaraOp(Lerp)\
	NiagaraOp(Reciprocal)\
	NiagaraOp(ReciprocalSqrt)\
	NiagaraOp(Sqrt)\
	NiagaraOp(Negate)\
	NiagaraOp(Abs)\
	NiagaraOp(Exp)\
	NiagaraOp(Exp2)\
	NiagaraOp(Log)\
	NiagaraOp(LogBase2)\
	NiagaraOp(Sin)\
	NiagaraOp(Cos)\
	NiagaraOp(Tan)\
	NiagaraOp(ASin)\
	NiagaraOp(ACos)\
	NiagaraOp(ATan)\
	NiagaraOp(ATan2)\
	NiagaraOp(Ceil)\
	NiagaraOp(Floor)\
	NiagaraOp(Modulo)\
	NiagaraOp(Fractional)\
	NiagaraOp(Truncate)\
	NiagaraOp(Clamp)\
	NiagaraOp(Min)\
	NiagaraOp(Max)\
	NiagaraOp(Pow)\
	NiagaraOp(Sign)\
	NiagaraOp(Step)\
	NiagaraOp(Dot)\
	NiagaraOp(Cross)\
	NiagaraOp(Normalize)\
	NiagaraOp(Random)\
	NiagaraOp(Length)\
	NiagaraOp(Noise)\
	NiagaraOp(Scalar)\
	NiagaraOp(Vector)\
	NiagaraOp(Matrix)\
	NiagaraOp(Compose)\
	NiagaraOp(ComposeX)\
	NiagaraOp(ComposeY)\
	NiagaraOp(ComposeZ)\
	NiagaraOp(ComposeW)\
	NiagaraOp(Split)\
	NiagaraOp(GetX)\
	NiagaraOp(GetY)\
	NiagaraOp(GetZ)\
	NiagaraOp(GetW)\
	NiagaraOp(TransformVector)\
	NiagaraOp(Transpose)\
	NiagaraOp(Inverse)\
	NiagaraOp(LessThan)\
	NiagaraOp(Sample)\
	NiagaraOp(Write)\
	NiagaraOp(EventBroadcast)\
	NiagaraOp(EaseIn)\
	NiagaraOp(EaseInOut)\

enum class ENiagaraExpressionResultLocation
{
	InputData,
	OutputData,
	Constants,
	Temporaries,
	BufferConstants,
	Unknown,
};



typedef TSharedPtr<class FNiagaraExpression> TNiagaraExprPtr;

/** Base class for all Niagara Expresions. Intermediate data collected from the node graph used for compiling Niagara scripts. */
class FNiagaraExpression : public TSharedFromThis<FNiagaraExpression>
{
public:
	FNiagaraExpression(class FNiagaraCompiler* InCompiler, const FNiagaraVariableInfo& InResult)
		: Compiler(InCompiler)
		, ResultLocation(ENiagaraExpressionResultLocation::Unknown)
		, ResultIndex(INDEX_NONE)
		, ComponentIndex(INDEX_NONE)
		, Result(InResult)
	{
	}

	virtual ~FNiagaraExpression()
	{
	}

	/** Perform the work specific to the expression type. */
	virtual void Process(){}

	virtual void PostProcess()
	{
		SourceExpressions.Empty();
	}

	FORCEINLINE TNiagaraExprPtr GetSourceExpression(int32 i){ return StaticCastSharedPtr<FNiagaraExpression>(SourceExpressions[i]); }

	/** Back pointer into the compiler. */
	class FNiagaraCompiler* Compiler;

	/** Location of this expressions result. */
	ENiagaraExpressionResultLocation ResultLocation;

	/** Index of this result within it's location. */
	int32 ResultIndex;

	/** The index of the component we're accessing. Typically this is unused and set to INDEX_NONE. Scalar ops will set and use it however. */
	int32 ComponentIndex;

	/** The variable info for the result of this expression. */
	FNiagaraVariableInfo Result;

	//Expressions that this one relies on. Will be processed before this one.
	TArray<TNiagaraExprPtr> SourceExpressions;
};

DECLARE_DELEGATE_ThreeParams(FNiagaraOpDelegate, class INiagaraCompiler*, TArray<TNiagaraExprPtr>&, TArray<TNiagaraExprPtr> &);

/** Information about an input or output of a Niagara operation node. */
class UNREALED_API FNiagaraOpInOutInfo
{
public:
	FName Name;
	ENiagaraDataType DataType;
	FText FriendlyName;
	FText Description;
	FString Default;

	FNiagaraOpInOutInfo(FName InName, ENiagaraDataType InType, FText InFriendlyName, FText InDescription, FString InDefault)
		: Name(InName)
		, DataType(InType)
		, FriendlyName(InFriendlyName)
		, Description(InDescription)
		, Default(InDefault)
	{

	}

	FNiagaraOpInOutInfo(FName InName, ENiagaraDataType InType)
		: Name(InName)
		, DataType(InType)
		, FriendlyName(FText::FromName(InName))
		, Description(FText::FromName(InName))
	{
		switch (InType)
		{
		case ENiagaraDataType::Scalar: Default = TEXT("1.0f");	break;
		case ENiagaraDataType::Vector: Default = TEXT("1.0f,1.0f,1.0f,1.0f");	break;
		case ENiagaraDataType::Matrix: Default = TEXT("1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,1.0f");	break;
		case ENiagaraDataType::Curve: Default = TEXT("");	break;
		};
	}
};

/** Information about a Niagara operation. */
class UNREALED_API FNiagaraOpInfo
{
public:
	FNiagaraOpInfo()
	: Name(NAME_None)
	, OpDelegate(NULL)
	{}

	FName Name;
	FText FriendlyName;
	FText Description;
	TArray<FNiagaraOpInOutInfo> Inputs;
	TArray<FNiagaraOpInOutInfo> Outputs;
	FNiagaraOpDelegate OpDelegate;

	static void Init();
	static const FNiagaraOpInfo* GetOpInfo(FName Name);
	static TMap<FName, FNiagaraOpInfo>& GetOpInfoMap();

#define NiagaraOp(OpName) static const FName OpName;
	NiagaraOpList
#undef NiagaraOp
};

class FNiagaraNodeResult
{
public:
	FNiagaraNodeResult(TNiagaraExprPtr InExpression, UEdGraphPin* Pin)
		: Expression(InExpression)
		, OutputPin(Pin)
	{
		check(InExpression.IsValid());
		check(Pin != NULL);
	}

	TNiagaraExprPtr Expression;
	UEdGraphPin* OutputPin;
};

