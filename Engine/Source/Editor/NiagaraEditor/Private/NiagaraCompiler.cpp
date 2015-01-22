// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "Engine/NiagaraScript.h"
#include "Components/NiagaraComponent.h"
#include "CompilerResultsLog.h"
#include "EdGraphUtilities.h"
#include "VectorVM.h"
#include "ComponentReregisterContext.h"
#include "NiagaraCompiler_VectorVM.h"
#include "NiagaraEditorCommon.h"

#define LOCTEXT_NAMESPACE "NiagaraCompiler"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraCompiler,All,All);

void FNiagaraEditorModule::CompileScript(UNiagaraScript* ScriptToCompile)
{
	check(ScriptToCompile != NULL);
	if (ScriptToCompile->Source == NULL)
	{
		UE_LOG(LogNiagaraCompiler, Error, TEXT("No source for Niagara script: %s"), *ScriptToCompile->GetPathName());
		return;
	}

	TComponentReregisterContext<UNiagaraComponent> ComponentReregisterContext;

	FNiagaraCompiler_VectorVM Compiler;
	Compiler.CompileScript(ScriptToCompile);

	//Compile for compute here too?
}

//////////////////////////////////////////////////////////////////////////

/** Expression that gets an input attribute. */
class FNiagaraExpression_GetAttribute : public FNiagaraExpression
{
public:

	FNiagaraExpression_GetAttribute(class FNiagaraCompiler* InCompiler, ENiagaraDataType InType, FName InName)
		: FNiagaraExpression(InCompiler, InType)
		, AttributeName(InName)
	{
		ResultLocation = ENiagaraExpressionResultLocation::InputData;
	}

	virtual void Process()override
	{
		check(ResultLocation == ENiagaraExpressionResultLocation::InputData);
		ResultIndex = Compiler->GetAttributeIndex(AttributeName);
	}

	FName AttributeName;
};

/** Expression that gets a constant. */
class FNiagaraExpression_GetConstant : public FNiagaraExpression
{
public:

	FNiagaraExpression_GetConstant(class FNiagaraCompiler* InCompiler, ENiagaraDataType InType, FName InName, bool bIsInternal)
		: FNiagaraExpression(InCompiler, InType)
		, ConstantName(InName)
		, bInternal(bIsInternal)
	{
		ResultLocation = ENiagaraExpressionResultLocation::Constants;
		//For matrix constants we must add 4 input expressions that will be filled in as the constant is processed.
		//They must at least exist now so that other expressions can reference them.
		if (ResultType == ENiagaraDataType::Matrix)
		{
			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, ENiagaraDataType::Vector)));
			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, ENiagaraDataType::Vector)));
			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, ENiagaraDataType::Vector)));
			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, ENiagaraDataType::Vector)));
		}
	}

	virtual void Process()override
	{
		check(ResultLocation == ENiagaraExpressionResultLocation::Constants);
		//Get the index of the constant. Also gets a component index if this is for a packed scalar constant.
		Compiler->GetConstantResultIndex(ConstantName, bInternal, ResultIndex, ComponentIndex);
		
		if (ResultType == ENiagaraDataType::Matrix)
		{
			check(SourceExpressions.Num() == 4);
			TNiagaraExprPtr Src0 = GetSourceExpression(0);
			TNiagaraExprPtr Src1 = GetSourceExpression(1);
			TNiagaraExprPtr Src2 = GetSourceExpression(2);
			TNiagaraExprPtr Src3 = GetSourceExpression(3);
			Src0->ResultLocation = ENiagaraExpressionResultLocation::Constants;
			Src0->ResultIndex = ResultIndex;
			Src1->ResultLocation = ENiagaraExpressionResultLocation::Constants;
			Src1->ResultIndex = ResultIndex + 1;
			Src2->ResultLocation = ENiagaraExpressionResultLocation::Constants;
			Src2->ResultIndex = ResultIndex + 2;
			Src3->ResultLocation = ENiagaraExpressionResultLocation::Constants;
			Src3->ResultIndex = ResultIndex + 3;
		}
	}

	FName ConstantName;
	bool bInternal;
};

/** Expression that just collects some other expressions together. E.g for a matrix. */
class FNiagaraExpression_Collection : public FNiagaraExpression
{
public:

	FNiagaraExpression_Collection(class FNiagaraCompiler* InCompiler, ENiagaraDataType Type, TArray<TNiagaraExprPtr>& InSourceExpressions)
		: FNiagaraExpression(InCompiler, Type)
	{
		SourceExpressions = InSourceExpressions;
	}
};

//////////////////////////////////////////////////////////////////////////

void FNiagaraCompiler::CheckInputs(FName OpName, TArray<TNiagaraExprPtr>& Inputs)
{
	//check the types of the input expressions.
	const FNiagaraOpInfo* OpInfo = FNiagaraOpInfo::GetOpInfo(OpName);
	check(OpInfo);
	int32 NumInputs = Inputs.Num();
	check(OpInfo->Inputs.Num() == NumInputs);
	for (int32 i = 0; i < NumInputs ; ++i)
	{
		if (!(OpInfo->Inputs[i].DataType == Inputs[i]->ResultType || Inputs[i]->ResultType == ENiagaraDataType::Unknown))
		{
			UE_LOG(LogNiagaraCompiler, Error, TEXT("Exression %s has incorrect inputs!\nExpected: %d - Actual: %d"), *OpName.ToString(), (int32)OpInfo->Inputs[i].DataType, (int32)((TNiagaraExprPtr)Inputs[i])->ResultType);
		}
	}
}

void FNiagaraCompiler::CheckOutputs(FName OpName, TArray<TNiagaraExprPtr>& Outputs)
{
	//check the types of the input expressions.
	const FNiagaraOpInfo* OpInfo = FNiagaraOpInfo::GetOpInfo(OpName);
	check(OpInfo);
	int32 NumOutputs = Outputs.Num();
	check(OpInfo->Outputs.Num() == NumOutputs);
	for (int32 i = 0; i < NumOutputs; ++i)
	{
		if (!(OpInfo->Outputs[i].DataType == Outputs[i]->ResultType || Outputs[i]->ResultType == ENiagaraDataType::Unknown))
		{
			UE_LOG(LogNiagaraCompiler, Error, TEXT("Exression %s has incorrect outputs!\nExpected: %d - Actual: %d"), *OpName.ToString(), (int32)OpInfo->Outputs[i].DataType, (int32)((TNiagaraExprPtr)Outputs[i])->ResultType);
		}
	}
}

int32 FNiagaraCompiler::GetAttributeIndex(FName Name)
{
	return Attributes.Find(Name);
}

TNiagaraExprPtr FNiagaraCompiler::CompileOutputPin(UEdGraphPin* Pin)
{
	check(Pin->Direction == EGPD_Output);

	TNiagaraExprPtr Ret;

	TNiagaraExprPtr* Expr = PinToExpression.Find(Pin);
	if (Expr)
	{
		Ret = *Expr; //We've compiled this pin before. Return it's expression.
	}
	else
	{
		//Otherwise we need to compile the node to get its output pins.
		UNiagaraNode* Node = Cast<UNiagaraNode>(Pin->GetOwningNode());
		TArray<FNiagaraNodeResult> Outputs;
		Node->Compile(this, Outputs);
		for (int32 i=0; i<Outputs.Num(); ++i)
		{
			PinToExpression.Add(Outputs[i].OutputPin, Outputs[i].Expression);
			//We also grab the expression for the pin we're currently interested in. Otherwise we'd have to search the map for it.
			Ret = Outputs[i].OutputPin == Pin ? Outputs[i].Expression : Ret;
		}
	}

	return Ret;
}

TNiagaraExprPtr FNiagaraCompiler::CompilePin(UEdGraphPin* Pin)
{
	check(Pin);
	TNiagaraExprPtr Ret;
	if (Pin->Direction == EGPD_Input)
	{
		if (Pin->LinkedTo.Num() > 0)
		{
			// > 1 ???
			Ret = CompileOutputPin(Pin->LinkedTo[0]);
		}
		else if (!Pin->bDefaultValueIsIgnored)
		{
			//No connections to this input so add the default as a const expression.
			const UEdGraphSchema_Niagara* Schema = Cast<const UEdGraphSchema_Niagara>(Pin->GetSchema());
			ENiagaraDataType DataType = Schema->GetPinDataType(Pin);

			FString ConstString = Pin->GetDefaultAsString();
			FName ConstName(*ConstString);
			
			if (DataType == ENiagaraDataType::Scalar)
			{
				float Default;
				Schema->GetPinDefaultValue(Pin, Default);
				ConstantData.SetOrAddInternal(ConstName, Default);
			}
			else if (DataType == ENiagaraDataType::Vector)
			{
				FVector4 Default;
				Schema->GetPinDefaultValue(Pin, Default);
				ConstantData.SetOrAddInternal(ConstName, Default);

			}
			else if (DataType == ENiagaraDataType::Matrix)
			{
				FMatrix Default;
				Schema->GetPinDefaultValue(Pin, Default);
				ConstantData.SetOrAddInternal(ConstName, Default);
			}
			Ret = Expression_GetInternalConstant(ConstName, DataType);
		}
	}
	else
	{
		Ret = CompileOutputPin(Pin);
	}

	return Ret;
}

void FNiagaraCompiler::GetParticleAttributes(TArray<FName>& OutAttributes)
{
	check(Source);
	Source->GetParticleAttributes(OutAttributes);
}

TNiagaraExprPtr FNiagaraCompiler::Expression_GetAttribute(FName AttributeName)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_GetAttribute(this, ENiagaraDataType::Vector, AttributeName)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler::Expression_GetExternalConstant(FName ConstantName, ENiagaraDataType Type)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_GetConstant(this, Type, ConstantName, false)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler::Expression_GetInternalConstant(FName ConstantName, ENiagaraDataType Type)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_GetConstant(this, Type, ConstantName, true)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler::Expression_Collection(TArray<TNiagaraExprPtr>& SourceExpressions)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_Collection(this, ENiagaraDataType::Unknown, SourceExpressions)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler::GetAttribute(FName AttributeName)
{
	return Expression_GetAttribute(AttributeName);
}

TNiagaraExprPtr FNiagaraCompiler::GetExternalConstant(FName ConstantName, float Default)
{
	SetOrAddConstant(false, ConstantName, Default);
	return Expression_GetExternalConstant(ConstantName, ENiagaraDataType::Scalar);
}

TNiagaraExprPtr FNiagaraCompiler::GetExternalConstant(FName ConstantName, FVector4 Default)
{
	SetOrAddConstant(false, ConstantName, Default);
	return Expression_GetExternalConstant(ConstantName, ENiagaraDataType::Vector);
}

TNiagaraExprPtr FNiagaraCompiler::GetExternalConstant(FName ConstantName, const FMatrix& Default)
{
	SetOrAddConstant(false, ConstantName, Default);
	return Expression_GetExternalConstant(ConstantName, ENiagaraDataType::Matrix);
}

#undef LOCTEXT_NAMESPACE