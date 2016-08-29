// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "CompilerResultsLog.h"
#include "EdGraphUtilities.h"
#include "VectorVM.h"
#include "ComponentReregisterContext.h"
#include "NiagaraCompiler_VectorVM.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"

#include "EdGraphSchema_Niagara.h"

#define LOCTEXT_NAMESPACE "NiagaraCompiler"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraCompiler, All, All);

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

	FNiagaraExpression_GetAttribute(class FNiagaraCompiler* InCompiler, const FNiagaraVariableInfo& InAttribute)
		: FNiagaraExpression(InCompiler, InAttribute)
	{
		ResultLocation = ENiagaraExpressionResultLocation::InputData;
	}

	virtual bool Process()override
	{
		check(ResultLocation == ENiagaraExpressionResultLocation::InputData);
		ResultIndex = Compiler->GetAttributeIndex(Result);
		return true;
	}
};

/** Expression that gets a constant. */
class FNiagaraExpression_GetConstant : public FNiagaraExpression
{
public:

	FNiagaraExpression_GetConstant(class FNiagaraCompiler* InCompiler, const FNiagaraVariableInfo& InConstant, bool bIsInternal)
		: FNiagaraExpression(InCompiler, InConstant)
		, bInternal(bIsInternal)
	{
		ResultLocation = ENiagaraExpressionResultLocation::Constants;
		//For matrix constants we must add 4 input expressions that will be filled in as the constant is processed.
		//They must at least exist now so that other expressions can reference them.
		if (Result.Type == ENiagaraDataType::Matrix)
		{
			static const FName ResultName(TEXT("MatrixComponent"));
			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, FNiagaraVariableInfo(ResultName, ENiagaraDataType::Vector))));
			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, FNiagaraVariableInfo(ResultName, ENiagaraDataType::Vector))));
			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, FNiagaraVariableInfo(ResultName, ENiagaraDataType::Vector))));
			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, FNiagaraVariableInfo(ResultName, ENiagaraDataType::Vector))));
		}
	}

	virtual bool Process()override
	{
		check(ResultLocation == ENiagaraExpressionResultLocation::Constants);
		//Get the index of the constant. Also gets a component index if this is for a packed scalar constant.
		Compiler->GetConstantResultIndex(Result, bInternal, ResultIndex);

		if (Result.Type == ENiagaraDataType::Matrix)
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

		return true;
	}

	bool bInternal;
};

/** Expression that just collects some other expressions together. E.g for a matrix. */
class FNiagaraExpression_Collection : public FNiagaraExpression
{
public:

	FNiagaraExpression_Collection(class FNiagaraCompiler* InCompiler, const FNiagaraVariableInfo& Result, TArray<TNiagaraExprPtr>& InSourceExpressions)
		: FNiagaraExpression(InCompiler, Result)
	{
		SourceExpressions = InSourceExpressions;
	}
};

//////////////////////////////////////////////////////////////////////////

bool FNiagaraCompiler::CheckInputs(FName OpName, TArray<TNiagaraExprPtr>& Inputs)
{
	//check the types of the input expressions.
	const FNiagaraOpInfo* OpInfo = FNiagaraOpInfo::GetOpInfo(OpName);
	check(OpInfo);
	int32 NumInputs = Inputs.Num();
	check(OpInfo->Inputs.Num() == NumInputs);
	bool bError = false;
	for (int32 i = 0; i < NumInputs ; ++i)
	{
		check(Inputs[i].IsValid());
		if (OpInfo->Inputs[i].DataType != Inputs[i]->Result.Type)
		{
			bError = true;
			FText ErrorText = FText::Format(LOCTEXT("Expression {0} has incorrect inputs!\nExpected: {1} - Actual: {2}", ""),
				FText::FromString(OpName.ToString()),
				FText::AsNumber((int32)OpInfo->Inputs[i].DataType),
				FText::AsNumber((int32)((TNiagaraExprPtr)Inputs[i])->Result.Type.GetValue()));
			MessageLog.Error(*ErrorText.ToString());
		}
	}
	return bError;
}

bool FNiagaraCompiler::CheckOutputs(FName OpName, TArray<TNiagaraExprPtr>& Outputs)
{
	//check the types of the input expressions.
	const FNiagaraOpInfo* OpInfo = FNiagaraOpInfo::GetOpInfo(OpName);
	check(OpInfo);
	int32 NumOutputs = Outputs.Num();
	bool bError = false;
	check(OpInfo->Outputs.Num() == NumOutputs);
	for (int32 i = 0; i < NumOutputs; ++i)
	{
		check(Outputs[i].IsValid());
		if (OpInfo->Outputs[i].DataType != Outputs[i]->Result.Type)
		{
			bError = true;
			FText ErrorText = FText::Format(LOCTEXT("Expression {0} has incorrect inputs!\nExpected: {1} - Actual: {2}", ""),
				FText::FromString(OpName.ToString()),
				FText::AsNumber((int32)OpInfo->Outputs[i].DataType),
				FText::AsNumber((int32)((TNiagaraExprPtr)Outputs[i])->Result.Type.GetValue()));
			MessageLog.Error(*ErrorText.ToString());
		}
	}
	return bError;
}

void FNiagaraCompiler::Error(FText ErrorText, UNiagaraNode* Node, UEdGraphPin* Pin)
{
	if (Pin)
	{
		FString ErrorString = FString::Printf(TEXT("Node: @@ - Pin: @@ - %s"), *ErrorText.ToString());
		MessageLog.Error(*ErrorString, Node, Pin);
	}
	else
	{
		FString ErrorString = FString::Printf(TEXT("Node: @@ - %s"), *ErrorText.ToString());
		MessageLog.Error(*ErrorString, Node);
	}
}

void FNiagaraCompiler::Warning(FText WarningText, UNiagaraNode* Node, UEdGraphPin* Pin)
{
	FString WarnString = FString::Printf(TEXT("Node: @@ - Pin: @@ - %s"), *WarningText.ToString());
	MessageLog.Warning(*WarnString, Node, Pin);
}

int32 FNiagaraCompiler::GetAttributeIndex(const FNiagaraVariableInfo& Attr)const
{
	return SourceGraph->GetAttributeIndex(Attr);
}

FNiagaraDataSetProperties* FNiagaraCompiler::GetSharedDataIndex(FNiagaraDataSetID DataSet, bool bForRead, int32& OutIndex)
{
	//Shared data is laid out:
	//EventReceivers
	//EventGenerators

	int32 SetIndex = 0;
	FNiagaraDataSetProperties* Ret = NULL;
	auto SearchSharedArea = [&](TArray<FNiagaraDataSetProperties>& SharedSetGroup) -> FNiagaraDataSetProperties*
	{
		for (FNiagaraDataSetProperties& Props : SharedSetGroup)
		{
			if (DataSet == Props.ID)
			{
				OutIndex = SetIndex;
				return &Props;
			}
			++SetIndex;
		}
		return NULL;
	};

	if (bForRead)
	{
		Ret = SearchSharedArea(EventReceivers);
	}
	else
	{
		SetIndex = EventReceivers.Num();
		Ret = SearchSharedArea(EventGenerators);
	}
	check(Ret);
	return Ret;
}

int32 FNiagaraCompiler::GetSharedDataVariableIndex(FNiagaraDataSetProperties* SharedDataSet, const FNiagaraVariableInfo& Variable)
{
	check(SharedDataSet);

	for (int32 i = 0; i < SharedDataSet->Variables.Num(); ++i)
	{
		if (SharedDataSet->Variables[i] == Variable)
		{
			return i;
		}
	}

	return INDEX_NONE;
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
		for (int32 i = 0; i < Outputs.Num(); ++i)
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
			//If we link to a function call, merge it's graph in.
			TArray<UNiagaraScript*> Stack;
			UNiagaraNodeFunctionCall* Function = Cast<UNiagaraNodeFunctionCall>(Pin->LinkedTo[0]->GetOwningNode());
			while(Function)
			{
				if (!MergeFunctionIntoMainGraph(Function, Stack))
				{
					return nullptr;
				}
				
				Function = Cast<UNiagaraNodeFunctionCall>(Pin->LinkedTo[0]->GetOwningNode());
			}
			//Any functon calls linked to this pin are now merged into the main graph, compile it.
			Ret = CompileOutputPin(Pin->LinkedTo[0]);
		}
		else if (!Pin->bDefaultValueIsIgnored)
		{
			//No connections to this input so add the default as a const expression.
			const UEdGraphSchema_Niagara* Schema = Cast<const UEdGraphSchema_Niagara>(Pin->GetSchema());
			ENiagaraDataType PinType = Schema->GetPinDataType(Pin);

			FString ConstString = Pin->GetDefaultAsString();
			FName ConstName(*ConstString);

			FNiagaraVariableInfo Var(ConstName, PinType);

			switch (PinType)
			{
			case ENiagaraDataType::Scalar:
			{
				float Default;
				Schema->GetPinDefaultValue(Pin, Default);
				ConstantData.SetOrAddInternal(Var, Default);
			}
				break;
			case ENiagaraDataType::Vector:
			{
				FVector4 Default;
				Schema->GetPinDefaultValue(Pin, Default);
				ConstantData.SetOrAddInternal(Var, Default);
			}
				break;
			case ENiagaraDataType::Matrix:
			{
				FMatrix Default;
				Schema->GetPinDefaultValue(Pin, Default);
				ConstantData.SetOrAddInternal(Var, Default);
			}
				break;
			}
			Ret = Expression_GetInternalConstant(Var);
		}
	}
	else
	{
		Ret = CompileOutputPin(Pin);
	}

	return Ret;
}

void FNiagaraCompiler::GetParticleAttributes(TArray<FNiagaraVariableInfo>& OutAttributes)const
{
	check(SourceGraph);
	SourceGraph->GetAttributes(OutAttributes);
}

TNiagaraExprPtr FNiagaraCompiler::Expression_GetAttribute(const FNiagaraVariableInfo& Attribute)
{
	Usage.bReadsAttriubteData = true;

	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_GetAttribute(this, Attribute)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler::Expression_GetExternalConstant(const FNiagaraVariableInfo& Constant)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_GetConstant(this, Constant, false)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler::Expression_GetInternalConstant(const FNiagaraVariableInfo& Constant)
{
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_GetConstant(this, Constant, true)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler::Expression_Collection(TArray<TNiagaraExprPtr>& SourceExpressions)
{
	//Todo - Collection expressions are just to collect parts of a matrix currently. Its a crappy way to do things that should be replaced.
	//Definitely don't start using it for collections of other things.
	int32 Index = Expressions.Add(MakeShareable(new FNiagaraExpression_Collection(this, FNiagaraVariableInfo(NAME_None, ENiagaraDataType::Vector), SourceExpressions)));
	return Expressions[Index];
}

TNiagaraExprPtr FNiagaraCompiler::GetAttribute(const FNiagaraVariableInfo& Attribute)
{
	return Expression_GetAttribute(Attribute);
}

TNiagaraExprPtr FNiagaraCompiler::GetExternalConstant(const FNiagaraVariableInfo& Constant, float Default)
{
	SetOrAddConstant(false, Constant, Default);
	return Expression_GetExternalConstant(Constant);
}
TNiagaraExprPtr FNiagaraCompiler::GetExternalConstant(const FNiagaraVariableInfo& Constant, const FVector4& Default)
{
	SetOrAddConstant(false, Constant, Default);
	return Expression_GetExternalConstant(Constant);
}
TNiagaraExprPtr FNiagaraCompiler::GetExternalConstant(const FNiagaraVariableInfo& Constant, const FMatrix& Default)
{
	SetOrAddConstant(false, Constant, Default);
	return Expression_GetExternalConstant(Constant);
}
TNiagaraExprPtr FNiagaraCompiler::GetInternalConstant(const FNiagaraVariableInfo& Constant, float Default)
{
	SetOrAddConstant(true, Constant, Default);
	return Expression_GetInternalConstant(Constant);
}
TNiagaraExprPtr FNiagaraCompiler::GetInternalConstant(const FNiagaraVariableInfo& Constant, const FVector4& Default)
{
	SetOrAddConstant(true, Constant, Default);
	return Expression_GetInternalConstant(Constant);
}
TNiagaraExprPtr FNiagaraCompiler::GetInternalConstant(const FNiagaraVariableInfo& Constant, const FMatrix& Default)
{
	SetOrAddConstant(true, Constant, Default);
	return Expression_GetInternalConstant(Constant);
}

bool FNiagaraCompiler::MergeFunctionIntoMainGraph(UNiagaraNodeFunctionCall* FunctionNode, TArray<UNiagaraScript*>& FunctionStack)
{
	struct FReconnectionInfo
	{
	public:
		UEdGraphPin* From;
		TArray<UEdGraphPin*> To;

		//Fallback default value if an input connection is not connected.
		FString FallbackDefault;

		FReconnectionInfo()
			: From(NULL)
		{}
	};

	if (UNiagaraScript* FuncScript = FunctionNode->FunctionScript)
	{
		//Check for reentrance.
		if (FunctionStack.Find(FuncScript) != INDEX_NONE)
		{
			FString Callstack;
			Callstack.Append(FuncScript->GetPathName()); Callstack.Append(TEXT("\n"));
			for (UNiagaraScript* StackScript : FunctionStack)
			{
				Callstack.Append(StackScript->GetPathName()); Callstack.Append(TEXT("\n"));
			}
			MessageLog.Error(*FString::Printf(TEXT("Reentrant function call!\n%s"), *Callstack));
			return false;
		}
		FunctionStack.Add(FuncScript);

		// Clone the source graph so we can modify it as needed; merging in the child graphs
		UNiagaraScriptSource* FuncSource = CastChecked<UNiagaraScriptSource>(FuncScript->Source);
		UNiagaraGraph* FuncGraph = CastChecked<UNiagaraGraph>(FEdGraphUtilities::CloneGraph(FuncSource->NodeGraph, NULL, &MessageLog));
		FEdGraphUtilities::MergeChildrenGraphsIn(FuncGraph, FuncGraph, /*bRequireSchemaMatch=*/ true);

		//Copies the function graph into the main graph.
		//Removes the Function call in the main graph and the input and output nodes in the function graph, reconnecting their pins appropriately.
		//auto MergeFunctionIntoMainGraph = [&](UNiagaraNodeFunctionCall* InFunc, UNiagaraGraph* FuncGraph)

		TMap<FName, FReconnectionInfo> InputConnections;
		TMap<FName, FReconnectionInfo> OutputConnections;
		TArray<class UEdGraphPin*> FuncCallInputPins;
		TArray<class UEdGraphPin*> FuncCallOutputPins;

		//Get all the pins that are connected to the inputs of the function call node in the main graph.
		FunctionNode->GetInputPins(FuncCallInputPins);
		for (UEdGraphPin* FuncCallInputPin : FuncCallInputPins)
		{
			FName InputName(*FuncCallInputPin->PinName);
			FReconnectionInfo& InputConnection = InputConnections.FindOrAdd(InputName);
			if (FuncCallInputPin->LinkedTo.Num() > 0)
			{
				check(FuncCallInputPin->LinkedTo.Num() == 1);
				UEdGraphPin* LinkFrom = FuncCallInputPin->LinkedTo[0];
				check(LinkFrom->Direction == EGPD_Output);
				InputConnection.From = LinkFrom;
			}
			else
			{
				//This input has no link so we need the default value from the pin.
				InputConnection.FallbackDefault = FuncCallInputPin->GetDefaultAsString();
			}
		}
		//Get all the pins that are connected to the outputs of the function call node in the main graph.
		FunctionNode->GetOutputPins(FuncCallOutputPins);
		for (UEdGraphPin* FuncCallOutputPin : FuncCallOutputPins)
		{
			FName OutputName(*FuncCallOutputPin->PinName);
			for (UEdGraphPin* LinkTo : FuncCallOutputPin->LinkedTo)
			{
				if (LinkTo)
				{
					check(LinkTo->Direction == EGPD_Input);
					FReconnectionInfo& OutputConnection = OutputConnections.FindOrAdd(OutputName);
					OutputConnection.To.Add(LinkTo);
				}
			}
		}

		//Remove the function call node from the graph now that we have everything we need from it.
		SourceGraph->RemoveNode(FunctionNode);
		UNiagaraNodeOutput* SourceGraphOuputNode = SourceGraph->FindOutputNode();
		check(SourceGraphOuputNode);

		//Keep a list of the Input and Output nodes we see in the function graph so that we can remove (most of) them later.
		TArray<UEdGraphNode*, TInlineAllocator<64>> ToRemove;

		//For each input node in the function graph, get all the LinkTo connections for reconnecting to the main graph later.
		TArray <UNiagaraNodeInput*> FuncGraphInputNodes;
		FuncGraph->FindInputNodes(FuncGraphInputNodes);
		for (UNiagaraNodeInput* FuncGraphInputNode : FuncGraphInputNodes)
		{
			check(FuncGraphInputNode && FuncGraphInputNode->Pins.Num() == 1);
			//Get an array of "To" pins from one or more input nodes referencing each named input.
			FReconnectionInfo& InputConnection = InputConnections.FindOrAdd(FuncGraphInputNode->Input.Name);
			if (InputConnection.From)
			{
				//We have a connection from the function call so remove the input node and connect to that.
				ToRemove.Add(FuncGraphInputNode);
			}
			else
			{
				//This input has no connection from the function call so what do we do here? 
				//For now we just leave the input node and connect back to it. 
				//This will mean unconnected pins from the function call will look for constants or attributes. 
				//In some cases we may want to just take the default value from the function call pin instead?
				//Maybe have some properties on the function call defining that.
				InputConnection.From = FuncGraphInputNode->Pins[0];
			}

			TArray<UEdGraphPin*>& LinkToPins = FuncGraphInputNode->Pins[0]->LinkedTo;
			for (UEdGraphPin* ToPin : LinkToPins)
			{
				check(ToPin->Direction == EGPD_Input);
				UEdGraphNode* OwningNode = ToPin->GetOwningNode();
				if (!OwningNode->IsA(UNiagaraNodeOutput::StaticClass()))
				{
					//As long as the owning node will survive into the source graph, connect directly to it.
					//Otherwise, we handle the connection in the OutputConnections.
					InputConnection.To.Add(ToPin);
				}
			}
		}

		//For each output of the output node in the function graph, get the "From" pin to be reconnected later.
		{
			UNiagaraNodeOutput* FuncGraphOutputNode = FuncGraph->FindOutputNode();
			check(FuncGraphOutputNode);

			//Unlike the input nodes, we don't have the option of keeping these if there is no "From" pin. The default values from the node pins should be used.
			ToRemove.Add(FuncGraphOutputNode);

			for (int32 OutputIdx = 0; OutputIdx < FuncGraphOutputNode->Outputs.Num(); ++OutputIdx)
			{
				FName OutputName = FuncGraphOutputNode->Outputs[OutputIdx].Name;

				UEdGraphPin* OutputNodePin = FuncGraphOutputNode->Pins[OutputIdx];
				check(OutputNodePin->LinkedTo.Num() <= 1);
				FReconnectionInfo& OutputConnection = OutputConnections.FindOrAdd(OutputName);
				UEdGraphPin* LinkFromPin = OutputNodePin->LinkedTo.Num() == 1 ? OutputNodePin->LinkedTo[0] : NULL;

				if (LinkFromPin)
				{
					UEdGraphNode* OwningNode = LinkFromPin->GetOwningNode();
					if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(OwningNode))
					{
						//We are connected directly to an input node so we must find the corresponding input connection and connect to that.
						if (FReconnectionInfo* InputConnection = InputConnections.Find(InputNode->Input.Name))
						{
							LinkFromPin = InputConnection->From;
						}
					}
				}

				if (LinkFromPin)
				{
					check(LinkFromPin->Direction == EGPD_Output);
					OutputConnection.From = LinkFromPin;
				}
				else
				{
					//This output is not connected so links to it in the main graph must use it's default value.
					OutputConnection.FallbackDefault = OutputNodePin->GetDefaultAsString();
				}
			}
		}

		//Remove all the In and Out nodes from the function graph.
		for (UEdGraphNode* Remove : ToRemove)
		{
			FuncGraph->RemoveNode(Remove);
		}

		TArray<UEdGraphNode*> FuncNodes = FuncGraph->Nodes;
		TArray<UEdGraphNode*> ExistingNodes = SourceGraph->Nodes;
		//Copy the nodes from the function graph over into the main graph.
		FuncGraph->MoveNodesToAnotherGraph(SourceGraph, false);

		FIntPoint FuncNodePos(FunctionNode->NodePosX, FunctionNode->NodePosY);
		FIntRect Bounds = FEdGraphUtilities::CalculateApproximateNodeBoundaries(FuncNodes);
		FIntPoint Center;
		FIntPoint Extents;
		Bounds.GetCenterAndExtents(Center, Extents);

		FIntPoint Offset = FuncNodePos - Center;
		for (UEdGraphNode* FuncNode : FuncNodes)
		{
			FuncNode->NodePosX += Offset.X;
			FuncNode->NodePosY += Offset.Y;
		}

		for (UEdGraphNode* Node : ExistingNodes)
		{
			if (Node->NodePosX < FuncNodePos.X)
			{
				Node->NodePosX -= Extents.X;
			}
			else
			{
				Node->NodePosX += Extents.X;
			}

			if (Node->NodePosY < FuncNodePos.Y)
			{
				Node->NodePosY -= Extents.Y;
			}
			else
			{
				Node->NodePosY += Extents.Y;
			}
		}

		//Finally, do all the reconnection.
		auto MakeConnection = [&](FReconnectionInfo& Info)
		{
			for (UEdGraphPin* LinkTo : Info.To)
			{
				if (Info.From)
				{
					Info.From->MakeLinkTo(LinkTo);
				}
				else
				{
					LinkTo->DefaultValue = Info.FallbackDefault;
				}
			}
		};
		for (TPair<FName, FReconnectionInfo>& ReconnectInfo : InputConnections){ MakeConnection(ReconnectInfo.Value); }
		for (TPair<FName, FReconnectionInfo>& ReconnectInfo : OutputConnections){ MakeConnection(ReconnectInfo.Value); }
	}
	else
	{ 
		Error(LOCTEXT("MergeNullFunction", "Function call node must reference a valid script."), FunctionNode, NULL);
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE