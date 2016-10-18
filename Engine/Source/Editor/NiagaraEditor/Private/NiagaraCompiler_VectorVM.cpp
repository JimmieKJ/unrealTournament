// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "CompilerResultsLog.h"
#include "EdGraphUtilities.h"
#include "VectorVM.h"
#include "ComponentReregisterContext.h"
#include "NiagaraCompiler_VectorVM.h"

#include "NiagaraNodeOutput.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeReadDataSet.h"
#include "NiagaraNodeWriteDataSet.h"

#define LOCTEXT_NAMESPACE "NiagaraCompiler_VectorVM"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraCompiler_VectorVM, All, All);

/** Expression handling native operations of the VectorVM. */
class FNiagaraExpression_VMOperation : public FNiagaraExpression
{
public:
	EVectorVMOp OpCode;

	FNiagaraExpression_VMOperation(FNiagaraCompiler* Compiler, const FNiagaraVariableInfo& InResult, TArray<TNiagaraExprPtr>& InputExpressions, EVectorVMOp Op)
		: FNiagaraExpression(Compiler, InResult)
		, OpCode(Op)
	{
		SourceExpressions = InputExpressions;
	}

	virtual ~FNiagaraExpression_VMOperation()
	{
		if (ResultLocation == ENiagaraExpressionResultLocation::Temporaries && ResultIndex != INDEX_NONE)
		{
			Compiler->FreeTemporary(ResultIndex);
		}
	}

	virtual bool Process()override
	{
		FNiagaraCompiler_VectorVM* VMCompiler = (FNiagaraCompiler_VectorVM*)Compiler;

		//Should be invalid at this point.
		check(ResultIndex == INDEX_NONE);

		ResultIndex = VMCompiler->AquireTemporary();
		if (ResultIndex == INDEX_NONE)
		{
			return false;
		}

		ResultLocation = ENiagaraExpressionResultLocation::Temporaries;
		check(ResultIndex < VectorVM::NumTempRegisters);

		VMCompiler->WriteCode((uint8)OpCode);

		//Add the bitfield defining whether each source operand comes from a constant or a register.
		int32 NumOperands = SourceExpressions.Num();
		check(NumOperands <= 4);

		uint8 OpTypeMask = VectorVM::CreateSrcOperandMask(NumOperands > 0 ? VMCompiler->GetVMOperandType(GetSourceExpression(0).Get()) : EVectorVMOperandLocation::TemporaryRegister,
			NumOperands > 1 ? VMCompiler->GetVMOperandType(GetSourceExpression(1).Get()) : EVectorVMOperandLocation::TemporaryRegister,
			NumOperands > 2 ? VMCompiler->GetVMOperandType(GetSourceExpression(2).Get()) : EVectorVMOperandLocation::TemporaryRegister,
			NumOperands > 3 ? VMCompiler->GetVMOperandType(GetSourceExpression(3).Get()) : EVectorVMOperandLocation::TemporaryRegister);
		VMCompiler->WriteCode(OpTypeMask);

		//Write dest
		VMCompiler->WriteCode(VMCompiler->GetResultVMIndex(this));

		//Add the locations for each of the source operands.
		for (int32 SrcIdx = 0; SrcIdx < SourceExpressions.Num(); ++SrcIdx)
		{
			VMCompiler->WriteCode(VMCompiler->GetResultVMIndex(GetSourceExpression(SrcIdx).Get()));
		}
		return true;
	}
};

/** Expression to write attribute data output in the VM. */
class FNiagaraExpression_VMOutput : public FNiagaraExpression
{
public:

	FNiagaraExpression_VMOutput(class FNiagaraCompiler* InCompiler, const FNiagaraVariableInfo& InAttribute, TNiagaraExprPtr& InSourceExpression)
		: FNiagaraExpression(InCompiler, InAttribute)
	{
		ResultLocation = ENiagaraExpressionResultLocation::OutputData;
		SourceExpressions.Add(InSourceExpression);
		check(SourceExpressions.Num() == 1);
	}

	virtual bool Process()override
	{
		FNiagaraCompiler_VectorVM* VMCompiler = (FNiagaraCompiler_VectorVM*)Compiler;
		check(ResultLocation == ENiagaraExpressionResultLocation::OutputData);
		ResultIndex = VMCompiler->GetAttributeIndex(Result);
		
		FNiagaraExpression* SrcExpr = GetSourceExpression(0).Get();
		check(SourceExpressions.Num() == 1);

		uint8 DestIndex = VMCompiler->GetResultVMIndex(this);
		uint8 SrcIndex = VMCompiler->GetResultVMIndex(SrcExpr);

		VMCompiler->WriteCode((uint8)EVectorVMOp::output);
		VMCompiler->WriteCode(VectorVM::CreateSrcOperandMask(VMCompiler->GetVMOperandType(SrcExpr)));
		VMCompiler->WriteCode(DestIndex);
		VMCompiler->WriteCode(SrcIndex);
		return true;
	}
};

/** Expression to read from shared data. */
class FNiagaraExpression_VMDataSetRead : public FNiagaraExpression
{
public:

	FNiagaraExpression_VMDataSetRead(class FNiagaraCompiler* InCompiler, FNiagaraDataSetID InEvent, const FNiagaraVariableInfo& InVariable, TNiagaraExprPtr& IndexExpr)
		: FNiagaraExpression(InCompiler, InVariable)
		, Event(InEvent)
	{
		SourceExpressions.Add(IndexExpr);
	}

	~FNiagaraExpression_VMDataSetRead()
	{
		Compiler->FreeTemporary(ResultIndex);
	}

	virtual bool Process()override
	{
		FNiagaraCompiler_VectorVM* VMCompiler = (FNiagaraCompiler_VectorVM*)Compiler;
		
		ResultIndex = VMCompiler->AquireTemporary();
		if (ResultIndex == INDEX_NONE)
		{
			return false;
		}
		ResultLocation = ENiagaraExpressionResultLocation::Temporaries;
		check(ResultIndex < VectorVM::NumTempRegisters);
		
		uint8 DestIndex = VMCompiler->GetResultVMIndex(this);
		int32 SetIndex;
		FNiagaraDataSetProperties* SetProps = VMCompiler->GetSharedDataIndex(Event, true, SetIndex);
		check(SetIndex <= 255);
		uint8 VarIndex = VMCompiler->GetSharedDataVariableIndex(SetProps, Result);

		check(SourceExpressions.Num() == 1);
		FNiagaraExpression* IndexExpr = GetSourceExpression(0).Get();
		uint8 IndexIndex = VMCompiler->GetResultVMIndex(IndexExpr);

		VMCompiler->WriteCode((uint8)EVectorVMOp::shareddataread);
		VMCompiler->WriteCode(DestIndex);//Result location

		VMCompiler->WriteCode((int8)SetIndex);//Index of data set
		VMCompiler->WriteCode(VarIndex);//Index of variable within data set view
		VMCompiler->WriteCode(IndexIndex);//Index of data in variable buffer
		return true;
	}

	FNiagaraDataSetID Event;
};

/** Expression to write to shared data. */
class FNiagaraExpression_VMDataSetWrite : public FNiagaraExpression
{
public:

	FNiagaraExpression_VMDataSetWrite(class FNiagaraCompiler* InCompiler, FNiagaraDataSetID InEvent, const FNiagaraVariableInfo& InVariable, TNiagaraExprPtr& IndexExpr, TNiagaraExprPtr& DataExpr)
		: FNiagaraExpression(InCompiler, InVariable)
		, Event(InEvent)
	{
		ResultLocation = ENiagaraExpressionResultLocation::SharedData;
		SourceExpressions.Add(IndexExpr);
		SourceExpressions.Add(DataExpr);
	}

	virtual bool Process()override
	{
		FNiagaraCompiler_VectorVM* VMCompiler = (FNiagaraCompiler_VectorVM*)Compiler;
		check(ResultLocation == ENiagaraExpressionResultLocation::SharedData);
		FNiagaraDataSetProperties* DataSetProps = VMCompiler->GetSharedDataIndex(Event, false, ResultIndex);
		int32 VarIndex = VMCompiler->GetSharedDataVariableIndex(DataSetProps, Result);

		FNiagaraExpression* IndexExpr = GetSourceExpression(0).Get();
		FNiagaraExpression* DataExpr = GetSourceExpression(1).Get();
		check(SourceExpressions.Num() == 2);

		uint8 IndexIndex = VMCompiler->GetResultVMIndex(IndexExpr);
		uint8 DataIndex = VMCompiler->GetResultVMIndex(DataExpr);		

		//TODO - Make this better, either ensure this is in a register by now or allow the VM to handle from constants?
		//Though, why you'd have the conditional come from a constant, IDK.
		if (!(DataExpr->ResultLocation == ENiagaraExpressionResultLocation::Temporaries || DataExpr->ResultLocation == ENiagaraExpressionResultLocation::InputData))
		{
			VMCompiler->Error(LOCTEXT("CompileFaliure_CannotWriteConstantToSharedData", "Writing of constants to shared data is not currently supported"), NULL, NULL);
			return false;
		}
		check(IndexExpr->ResultLocation == ENiagaraExpressionResultLocation::Temporaries);

		VMCompiler->WriteCode((uint8)EVectorVMOp::shareddatawrite);
		
		VMCompiler->WriteCode(ResultIndex);//Set idx
		VMCompiler->WriteCode(VarIndex);//Index of variable buffer in the set.
		VMCompiler->WriteCode(IndexIndex);//Index of the temporary buffer that contains the data index.

		VMCompiler->WriteCode(DataIndex);//Temp buffer index for the data to write.
		return true;
	}

	FNiagaraDataSetID Event;
};

/** Expression that gets the next index in a shared data set and increments the counter. */ 
class FNiagaraExpression_AcquireSharedDataIndex : public FNiagaraExpression
{
public:
	FNiagaraExpression_AcquireSharedDataIndex(class FNiagaraCompiler* InCompiler, FNiagaraDataSetID InDataSet, bool bInWrap, TNiagaraExprPtr& ValidExpr)
		: FNiagaraExpression(InCompiler, FNiagaraVariableInfo(TEXT("AcquireIndex"), ENiagaraDataType::Vector))
		, DataSet(InDataSet)
		, bWrap(bInWrap)
	{
		ResultLocation = ENiagaraExpressionResultLocation::Temporaries;
		SourceExpressions.Add(ValidExpr);
	}

	~FNiagaraExpression_AcquireSharedDataIndex()
	{
		Compiler->FreeTemporary(ResultIndex);
	}

	virtual bool Process()override
	{
		FNiagaraCompiler_VectorVM* VMCompiler = (FNiagaraCompiler_VectorVM*)Compiler;
		check(ResultLocation == ENiagaraExpressionResultLocation::Temporaries);
		ResultIndex = VMCompiler->AquireTemporary();
		if (ResultIndex == INDEX_NONE)
		{
			return false;
		}

		FNiagaraExpression* ValidExpr = GetSourceExpression(0).Get();
		check(SourceExpressions.Num() == 1);

		uint8 ValidIndex = VMCompiler->GetResultVMIndex(ValidExpr);

		int32 SharedDataSetIndex;
		VMCompiler->GetSharedDataIndex(DataSet, false, SharedDataSetIndex);
		check(SharedDataSetIndex <= 255)
		//TODO - Make this better, either ensure this is in a register by now or allow the VM to handle from constants?
		//Though, why you'd have the conditional come from a constant, IDK.
		if (!(ValidExpr->ResultLocation == ENiagaraExpressionResultLocation::Temporaries || ValidExpr->ResultLocation == ENiagaraExpressionResultLocation::InputData))
		{
			VMCompiler->Error(LOCTEXT("CompileFaliure_CannotUseConstantForSharedDataValidMask", "Use of constants for the shared data valid mask is not supported currently."), NULL, NULL);
			return false;
		}

		VMCompiler->WriteCode(bWrap ? (uint8)EVectorVMOp::aquireshareddataindexwrap : (uint8)EVectorVMOp::aquireshareddataindex);
		VMCompiler->WriteCode(ResultIndex);
		VMCompiler->WriteCode(ValidIndex);
		VMCompiler->WriteCode((uint8)SharedDataSetIndex);
		return true;
	}

	FNiagaraDataSetID DataSet;
	bool bWrap;
};

/** Expression that gets the next index in a shared data set and decrements the counter. */ 
class FNiagaraExpression_ConsumeSharedDataIndex : public FNiagaraExpression
{
public:
	FNiagaraExpression_ConsumeSharedDataIndex(class FNiagaraCompiler* InCompiler, FNiagaraDataSetID InDataSet, bool bInWrap)
		: FNiagaraExpression(InCompiler, FNiagaraVariableInfo(TEXT("ConsumeIndex"), ENiagaraDataType::Vector))
		, DataSet(InDataSet)
		, bWrap(bInWrap)
	{
		ResultLocation = ENiagaraExpressionResultLocation::Temporaries;
	}

	~FNiagaraExpression_ConsumeSharedDataIndex()
	{
		Compiler->FreeTemporary(ResultIndex);
	}

	virtual bool Process()override
	{
		FNiagaraCompiler_VectorVM* VMCompiler = (FNiagaraCompiler_VectorVM*)Compiler;
		ResultIndex = VMCompiler->AquireTemporary();
		if (ResultIndex == INDEX_NONE)
		{
			return false;
		}
			
		int32 SharedDataSetIndex;
		VMCompiler->GetSharedDataIndex(DataSet, true, SharedDataSetIndex);
			
		VMCompiler->WriteCode(bWrap ? (uint8)EVectorVMOp::consumeshareddataindexwrap : (uint8)EVectorVMOp::consumeshareddataindex);
		VMCompiler->WriteCode(ResultIndex);
		VMCompiler->WriteCode((uint8)SharedDataSetIndex);
		return true;
	}

	FNiagaraDataSetID DataSet;
	bool bWrap;
};

//Can remove this if we refactor the compiler to allow a single expression to output multiple results.
class FNiagaraExpression_IsValidSharedDataIndex : public FNiagaraExpression
{
public:
	FNiagaraExpression_IsValidSharedDataIndex(class FNiagaraCompiler* InCompiler, FNiagaraDataSetID InDataSet, TNiagaraExprPtr& IndexExpr)
		: FNiagaraExpression(InCompiler, FNiagaraVariableInfo(TEXT("IsValidIndex"), ENiagaraDataType::Vector))
		, DataSet(InDataSet)
	{
		ResultLocation = ENiagaraExpressionResultLocation::Temporaries;
		SourceExpressions.Add(IndexExpr);
	}

	~FNiagaraExpression_IsValidSharedDataIndex()
	{
		Compiler->FreeTemporary(ResultIndex);
	}

	virtual bool Process()override
	{
		FNiagaraCompiler_VectorVM* VMCompiler = (FNiagaraCompiler_VectorVM*)Compiler;
		ResultIndex = VMCompiler->AquireTemporary();
		if (ResultIndex == INDEX_NONE)
		{
			return false;
		}
		check(SourceExpressions.Num() == 1);

		int32 IndexIndex = VMCompiler->GetResultVMIndex(GetSourceExpression(0).Get());

		int32 SharedDataSetIndex;
		FNiagaraDataSetProperties* DataSetProps = VMCompiler->GetSharedDataIndex(DataSet, true, SharedDataSetIndex);//This is a bit wonky. Might have to rework it a bit
	
		VMCompiler->WriteCode((uint8)EVectorVMOp::shareddataindexvalid);
		VMCompiler->WriteCode(ResultIndex);
		VMCompiler->WriteCode(IndexIndex);
		VMCompiler->WriteCode(SharedDataSetIndex);
		return true;
	}

	FNiagaraDataSetID DataSet;
};

bool FNiagaraCompiler_VectorVM::ExpressionIsConstant(FNiagaraExpression*  Expression)
{
	return Expression->ResultLocation == ENiagaraExpressionResultLocation::Constants;
}

bool FNiagaraCompiler_VectorVM::ExpressionIsBufferConstant(FNiagaraExpression*  Expression)
{
	return Expression->ResultLocation == ENiagaraExpressionResultLocation::BufferConstants;
}

EVectorVMOperandLocation FNiagaraCompiler_VectorVM::GetVMOperandType(FNiagaraExpression *Expression)
{
	switch (Expression->ResultLocation)
	{
	case ENiagaraExpressionResultLocation::Temporaries: return EVectorVMOperandLocation::TemporaryRegister;
	case ENiagaraExpressionResultLocation::InputData: return EVectorVMOperandLocation::InputRegister;
	case ENiagaraExpressionResultLocation::OutputData: return EVectorVMOperandLocation::OutputRegister;
	case ENiagaraExpressionResultLocation::Constants: return EVectorVMOperandLocation::Constant;
	case ENiagaraExpressionResultLocation::BufferConstants: return EVectorVMOperandLocation::DataObjConstant;
	case ENiagaraExpressionResultLocation::SharedData: return EVectorVMOperandLocation::SharedData;
	default: return EVectorVMOperandLocation::Undefined;
	};
}

uint8 FNiagaraCompiler_VectorVM::GetResultVMIndex(FNiagaraExpression* Expression)
{
	if (Expression->ResultLocation == ENiagaraExpressionResultLocation::InputData)
	{
		return VectorVM::FirstInputRegister + Expression->ResultIndex;
	}
	else if (Expression->ResultLocation == ENiagaraExpressionResultLocation::OutputData)
	{
		return VectorVM::FirstOutputRegister + Expression->ResultIndex;
	}
	else
	{
		//TempRegisters start at 0 so can use index directly.
		//Buffer constants, Constants and SharedData all exist in their own space so use index directly.
		return Expression->ResultIndex;
	}
}

ENiagaraDataType FNiagaraCompiler_VectorVM::GetConstantResultIndex(const FNiagaraVariableInfo& Constant, bool bInternal, int32& OutResultIndex)
{
	ENiagaraDataType Type;
	ConstantData.GetTableIndex(Constant, bInternal, OutResultIndex, Type);
	return Type;
}

int32 FNiagaraCompiler_VectorVM::AquireTemporary()
{
	int32 Free = TempRegisters.Find(false);
	if (Free != INDEX_NONE)
	{
		TempRegisters[Free] = true;
	}
	else
	{
		Error(LOCTEXT("ScriptUsesTooManyTemporariesError","This script uses too many temporary registers. It has to be simplified to work."), NULL, NULL);
	}

	return Free;
}

void FNiagaraCompiler_VectorVM::FreeTemporary(int32 TempIndex)
{
	check(TempIndex < TempRegisters.Num() && TempIndex >= 0);
	TempRegisters[TempIndex] = false;
}

void FNiagaraCompiler_VectorVM::WriteCode(uint8 InCode)
{
	check(Script);
	Script->ByteCode.Add(InCode);
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::Output(const FNiagaraVariableInfo& Attr, TNiagaraExprPtr& SourceExpression)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_VMOutput(this, Attr, SourceExpression)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::SharedDataRead(const FNiagaraDataSetID& DataSet, const FNiagaraVariableInfo& Variable, TNiagaraExprPtr& IndexExpr)
{
	bool bFound = false;
	check(DataSet.Type == ENiagaraDataSetType::Event);//TODO: Other data set types.
	for (FNiagaraDataSetProperties& SetProps : EventReceivers)
	{
		if (SetProps.ID == DataSet)
		{
			SetProps.Variables.AddUnique(Variable);
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		int32 NewIdx = EventReceivers.AddDefaulted();
		EventReceivers[NewIdx].ID = DataSet;
		EventReceivers[NewIdx].Variables.Add(Variable);
	}
	
	//TODO - This can be moved up into FNiagaraCompiler when I work around the issue of moving the shared data into a temporary.
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_VMDataSetRead(this, DataSet, Variable, IndexExpr)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::SharedDataWrite(const FNiagaraDataSetID& DataSet, const FNiagaraVariableInfo& Variable, TNiagaraExprPtr& IndexExpr, TNiagaraExprPtr& DataExpr)
{
	bool bFound = false;
	check(DataSet.Type == ENiagaraDataSetType::Event);//TODO: Other data set types.
	for (FNiagaraDataSetProperties& SetProps : EventGenerators)
	{
		if (SetProps.ID == DataSet)
		{
			SetProps.Variables.AddUnique(Variable);
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		int32 NewIdx = EventGenerators.AddDefaulted();
		EventGenerators[NewIdx].ID = DataSet;
		EventGenerators[NewIdx].Variables.Add(Variable);
	}

	//TODO - This can be moved up into FNiagaraCompiler when I work around the issue of moving the shared data into a temporary.
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_VMDataSetWrite(this, DataSet, Variable, IndexExpr, DataExpr)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::AcquireSharedDataIndex(const FNiagaraDataSetID& DataSet, bool bWrap, TNiagaraExprPtr& ValidExpr)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_AcquireSharedDataIndex(this, DataSet, bWrap, ValidExpr)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::ConsumeSharedDataIndex(const FNiagaraDataSetID& DataSet, bool bWrap)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_ConsumeSharedDataIndex(this, DataSet, bWrap)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::SharedDataIndexIsValid(const FNiagaraDataSetID& DataSet, TNiagaraExprPtr& IndexExpr)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_IsValidSharedDataIndex(this, DataSet, IndexExpr)));
	return Expressions[Index];
}

bool FNiagaraCompiler_VectorVM::CompileScript(UNiagaraScript* InScript)
{
	//TODO - Most of this function can be refactored up into FNiagaraCompiler once we know how the compute script and VM script will coexist in the UNiagaraScript.

	check(InScript);

	Script = InScript;
	Source = CastChecked<UNiagaraScriptSource>(Script->Source);

	//Clear previous graph errors.
	bool bHasClearedGraphErrors = false;
	for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
	{
		if (Node->bHasCompilerMessage)
		{
			Node->ErrorMsg.Empty();
			Node->ErrorType = EMessageSeverity::Info;
			Node->bHasCompilerMessage = false;
			Node->Modify(true);
			bHasClearedGraphErrors = true;
		}
	}
	if (bHasClearedGraphErrors)
	{
		Source->NodeGraph->NotifyGraphChanged();
	}

	//Should we roll our own message/error log and put it in a window somewhere?
	MessageLog.SetSourcePath(InScript->GetPathName());

	// Clone the source graph so we can modify it as needed; merging in the child graphs
	SourceGraph = CastChecked<UNiagaraGraph>(FEdGraphUtilities::CloneGraph(Source->NodeGraph, NULL, &MessageLog));
	FEdGraphUtilities::MergeChildrenGraphsIn(SourceGraph, SourceGraph, /*bRequireSchemaMatch=*/ true);


	TempRegisters.AddZeroed(VectorVM::NumTempRegisters);

	// Find the output node and compile it.
	UNiagaraNodeOutput* OutputNode = SourceGraph->FindOutputNode();
	TArray<FNiagaraNodeResult> OutputExpressions;
	OutputNode->Compile(this, OutputExpressions);

	//Find any write data set nodes and compile them.
	TArray<UNiagaraNodeWriteDataSet*> WriteDataSetNodes;
	SourceGraph->FindWriteDataSetNodes(WriteDataSetNodes);
	for (UNiagaraNodeWriteDataSet* WriteNode : WriteDataSetNodes)
	{
		WriteNode->Compile(this, OutputExpressions);
	}

	//Clear the pin to expressions map. All reused expressions will now be referenced by the expressions using them.
	PinToExpression.Empty();//TODO - Replace this with expression caching at a lower level. Will be more useful and nicer.

	//Todo - track usage of temp registers so we can allocate and free and reuse from a smaller set.
	int32 NextTempRegister = 0;
	Script->ByteCode.Empty();

	//Now we have a set of expressions from which we can generate the bytecode.
	for (int32 i = 0; i < Expressions.Num(); ++i)
	{
		TNiagaraExprPtr Expr = Expressions[i];
		if (Expr.IsValid())
		{
			if (!Expr->Process())
			{
				Error(LOCTEXT("CompilationError", "Compilation Error"), NULL, NULL);
				break;
			}

			Expr->PostProcess();
		}
		else
		{
			check(0);//Replace with graceful bailout?
		}

		Expressions[i].Reset();
	}

	Expressions.Empty();

	if (MessageLog.NumErrors == 0)
	{
		check(TempRegisters.Find(true) == INDEX_NONE);//Make sure there aren't any temp registers still in use. If so, some expression is leaking them.

		Script->Modify();
		Source->FlattenedNodeGraph = SourceGraph;
		Source->FlattenedNodeGraph->Modify();			
		Source->FlattenedNodeGraph->NotifyGraphChanged();

		// Terminate with the 'done' opcode.
		Script->ByteCode.Add((int8)EVectorVMOp::done);
		Script->ConstantData = ConstantData;
		Script->Attributes = OutputNode->Outputs;
		Script->EventGenerators = EventGenerators;
		Script->EventReceivers = EventReceivers;
		Script->Usage = Usage;
		return true;
	}

	//Compilation has failed.
	Script->Attributes.Empty();
	Script->ByteCode.Empty();
	Script->ConstantData.Empty();
	Script->EventGenerators.Empty();
	Script->EventReceivers.Empty();
	Script->Usage = FNiagaraScriptUsageInfo();
	//Tell the graph to show the errors.
	Source->NodeGraph->NotifyGraphChanged();
		
	return false;
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::Expression_VMNative(EVectorVMOp Op, TArray<TNiagaraExprPtr>& InputExpressions)
{
	static const FName OpName(TEXT("VMOp"));
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_VMOperation(this, FNiagaraVariableInfo(OpName, ENiagaraDataType::Vector), InputExpressions, Op)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::Expression_VMNative(EVectorVMOp Op, TNiagaraExprPtr A)
{
	TArray<TNiagaraExprPtr> In; In.Add(A);
	return Expression_VMNative(Op, In);
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::Expression_VMNative(EVectorVMOp Op, TNiagaraExprPtr A, TNiagaraExprPtr B)
{
	TArray<TNiagaraExprPtr> In; In.Add(A); In.Add(B);
	return Expression_VMNative(Op, In);
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::Expression_VMNative(EVectorVMOp Op, TNiagaraExprPtr A, TNiagaraExprPtr B, TNiagaraExprPtr C)
{
	TArray<TNiagaraExprPtr> In; In.Add(A); In.Add(B); In.Add(C);
	return Expression_VMNative(Op, In);
}

TNiagaraExprPtr FNiagaraCompiler_VectorVM::Expression_VMNative(EVectorVMOp Op, TNiagaraExprPtr A, TNiagaraExprPtr B, TNiagaraExprPtr C, TNiagaraExprPtr D)
{
	TArray<TNiagaraExprPtr> In; In.Add(A); In.Add(B); In.Add(C); In.Add(D);
	return Expression_VMNative(Op, In);
}

bool FNiagaraCompiler_VectorVM::Add_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::add, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Subtract_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add((Expression_VMNative(EVectorVMOp::sub, InputExpressions)));
	return true;
}

bool FNiagaraCompiler_VectorVM::Multiply_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::mul, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Divide_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::div, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::MultiplyAdd_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::mad, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Lerp_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::lerp, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Reciprocal_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::rcp, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ReciprocalSqrt_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::rsq, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Sqrt_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::sqrt, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Negate_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::neg, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Abs_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::abs, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Exp_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::exp, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Exp2_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::exp2, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Log_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::log, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::LogBase2_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::log2, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Sin_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::sin, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Cos_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::cos, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Tan_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::tan, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ASin_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::asin, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ACos_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::acos, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ATan_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::atan, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ATan2_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::atan2, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Ceil_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::ceil, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Floor_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::floor, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Modulo_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::fmod, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Fractional_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::frac, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Truncate_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::trunc, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Clamp_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::clamp, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Min_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::min, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Max_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::max, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Pow_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::pow, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Sign_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::sign, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Step_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::step, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Dot_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::dot, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Cross_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::cross, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Normalize_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::normalize, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Random_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::random, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Length_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::length, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Noise_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::noise, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Scalar_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	GetX_Internal(InputExpressions, OutputExpressions);
	return true;
}

bool FNiagaraCompiler_VectorVM::Vector_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(InputExpressions[0]);
	return true;
}

bool FNiagaraCompiler_VectorVM::Matrix_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	//Just collect the matrix rows into an expression for what ever use subsequent expressions have.
	TNiagaraExprPtr MatrixExpr = Expression_Collection(InputExpressions);
	MatrixExpr->Result.Type = ENiagaraDataType::Matrix;
	OutputExpressions.Add(MatrixExpr);
	return true;
}

bool FNiagaraCompiler_VectorVM::Compose_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::compose, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ComposeX_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::composex, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ComposeY_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::composey, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ComposeZ_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::composez, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::ComposeW_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::composew, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::Split_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	GetX_Internal(InputExpressions, OutputExpressions);
	GetY_Internal(InputExpressions, OutputExpressions);
	GetZ_Internal(InputExpressions, OutputExpressions);
	GetW_Internal(InputExpressions, OutputExpressions);
	return true;
}

bool FNiagaraCompiler_VectorVM::GetX_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::splatx, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::GetY_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::splaty, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::GetZ_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::splatz, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::GetW_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::splatw, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::TransformVector_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	TNiagaraExprPtr MatrixExpr = InputExpressions[0];
	check(MatrixExpr->SourceExpressions.Num() == 4);
	TNiagaraExprPtr Row0 = MatrixExpr->GetSourceExpression(0);
	TNiagaraExprPtr Row1 = MatrixExpr->GetSourceExpression(1);
	TNiagaraExprPtr Row2 = MatrixExpr->GetSourceExpression(2);
	TNiagaraExprPtr Row3 = MatrixExpr->GetSourceExpression(3);

	TNiagaraExprPtr VectorExpr = InputExpressions[1];

	TArray<TNiagaraExprPtr> Temp;
	Temp.Add(Expression_VMNative(EVectorVMOp::splatx, VectorExpr));
	Temp.Add(Expression_VMNative(EVectorVMOp::splaty, VectorExpr));
	Temp.Add(Expression_VMNative(EVectorVMOp::splatz, VectorExpr));
	Temp.Add(Expression_VMNative(EVectorVMOp::splatw, VectorExpr));

	Temp[0] = Expression_VMNative(EVectorVMOp::mul, MatrixExpr->GetSourceExpression(0), Temp[0]);
	Temp[1] = Expression_VMNative(EVectorVMOp::mul, MatrixExpr->GetSourceExpression(1), Temp[1]);
	Temp[2] = Expression_VMNative(EVectorVMOp::mul, MatrixExpr->GetSourceExpression(2), Temp[2]);
	Temp[3] = Expression_VMNative(EVectorVMOp::mul, MatrixExpr->GetSourceExpression(3), Temp[3]);

	Temp[0] = Expression_VMNative(EVectorVMOp::add, Temp[0], Temp[1]);
	Temp[1] = Expression_VMNative(EVectorVMOp::add, Temp[2], Temp[3]);
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::add, Temp[0], Temp[1]));
	return true;
}

bool FNiagaraCompiler_VectorVM::Transpose_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	TNiagaraExprPtr MatrixExpr = InputExpressions[0];
	check(MatrixExpr->SourceExpressions.Num() == 4);
	TNiagaraExprPtr Row0 = MatrixExpr->GetSourceExpression(0);
	TNiagaraExprPtr Row1 = MatrixExpr->GetSourceExpression(1);
	TNiagaraExprPtr Row2 = MatrixExpr->GetSourceExpression(2);
	TNiagaraExprPtr Row3 = MatrixExpr->GetSourceExpression(3);

	//TODO: can probably make this faster if I provide direct shuffle ops in the VM rather than using the flexible, catch-all op compose.
	//Maybe don't want to make that externally usable though? Maybe for power users...
	TArray<TNiagaraExprPtr> Columns;
	Columns.Add(Expression_VMNative(EVectorVMOp::composex, Row0, Row1, Row2, Row3));
	Columns.Add(Expression_VMNative(EVectorVMOp::composey, Row0, Row1, Row2, Row3));
	Columns.Add(Expression_VMNative(EVectorVMOp::composez, Row0, Row1, Row2, Row3));
	Columns.Add(Expression_VMNative(EVectorVMOp::composew, Row0, Row1, Row2, Row3));

	Matrix_Internal(Columns, OutputExpressions);
	return true;
}


bool FNiagaraCompiler_VectorVM::Inverse_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	Error(LOCTEXT("VMNoImpl", "VM Operation not yet implemented"), nullptr, nullptr);
	return false;
}


bool FNiagaraCompiler_VectorVM::LessThan_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::lessthan, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::GreaterThan_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	check(InputExpressions.Num() == 2);
	//Use lessthan op with swapped inputs.
	TArray<TNiagaraExprPtr> SwappedInputs;
	SwappedInputs.Add(InputExpressions[1]);
	SwappedInputs.Add(InputExpressions[0]);
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::lessthan, SwappedInputs));
	return true;
}

bool FNiagaraCompiler_VectorVM::Select_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::select, InputExpressions));
	return true;
}

bool FNiagaraCompiler_VectorVM::EaseIn_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::easein, InputExpressions));
	return true;
}


bool FNiagaraCompiler_VectorVM::EaseInOut_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)
{
	OutputExpressions.Add(Expression_VMNative(EVectorVMOp::easeinout, InputExpressions));
	return true;
}


#undef LOCTEXT_NAMESPACE
