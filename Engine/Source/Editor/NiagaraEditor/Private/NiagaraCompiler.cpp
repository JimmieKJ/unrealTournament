// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
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

	FNiagaraExpression_GetAttribute(class FNiagaraCompiler* InCompiler, const FNiagaraVariableInfo& InAttribute)
		: FNiagaraExpression(InCompiler, InAttribute)
	{
		ResultLocation = ENiagaraExpressionResultLocation::InputData;
	}

	virtual void Process()override
	{
		check(ResultLocation == ENiagaraExpressionResultLocation::InputData);
		ResultIndex = Compiler->GetAttributeIndex(Result);
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
// 		else if (Result.Type == ENiagaraDataType::Curve)
// 		{
// 			static const FName ResultName(TEXT("Curve"));
// 			SourceExpressions.Add(MakeShareable(new FNiagaraExpression(InCompiler, FNiagaraVariableInfo(ResultName, ENiagaraDataType::Curve))));
// 		}
	}

	virtual void Process()override
	{
		check(ResultLocation == ENiagaraExpressionResultLocation::Constants);
		//Get the index of the constant. Also gets a component index if this is for a packed scalar constant.
		Compiler->GetConstantResultIndex(Result, bInternal, ResultIndex, ComponentIndex);
		
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
		else if (Result.Type == ENiagaraDataType::Curve)
		{
			ResultLocation = ENiagaraExpressionResultLocation::BufferConstants;
		}
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

void FNiagaraCompiler::CheckInputs(FName OpName, TArray<TNiagaraExprPtr>& Inputs)
{
	//check the types of the input expressions.
	const FNiagaraOpInfo* OpInfo = FNiagaraOpInfo::GetOpInfo(OpName);
	check(OpInfo);
	int32 NumInputs = Inputs.Num();
	check(OpInfo->Inputs.Num() == NumInputs);
	for (int32 i = 0; i < NumInputs ; ++i)
	{
		if (OpInfo->Inputs[i].DataType != Inputs[i]->Result.Type)
		{
			UE_LOG(LogNiagaraCompiler, Error, TEXT("Exression %s has incorrect inputs!\nExpected: %d - Actual: %d"), *OpName.ToString(), (int32)OpInfo->Inputs[i].DataType, (int32)((TNiagaraExprPtr)Inputs[i])->Result.Type.GetValue());
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
		if (OpInfo->Outputs[i].DataType != Outputs[i]->Result.Type)
		{
			UE_LOG(LogNiagaraCompiler, Error, TEXT("Exression %s has incorrect outputs!\nExpected: %d - Actual: %d"), *OpName.ToString(), (int32)OpInfo->Outputs[i].DataType, (int32)((TNiagaraExprPtr)Outputs[i])->Result.Type.GetValue());
		}
	}
}

int32 FNiagaraCompiler::GetAttributeIndex(const FNiagaraVariableInfo& Attr)const
{
	return SourceGraph->GetAttributeIndex(Attr);
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
			ENiagaraDataType PinType = Schema->GetPinType(Pin);

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
			case ENiagaraDataType::Curve:
			{
				FNiagaraDataObject *Default = nullptr;
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

TNiagaraExprPtr FNiagaraCompiler::GetExternalCurveConstant(const FNiagaraVariableInfo& Constant)
{
	SetOrAddConstant<FNiagaraDataObject*>(false, Constant, nullptr);
	return Expression_GetExternalConstant(Constant);
}

// TODO: Refactor this (and some other stuff) into a preprocessing step for use by any compiler?
bool FNiagaraCompiler::MergeInFunctionNodes()
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
	TMap<FName, FReconnectionInfo> InputConnections;
	TMap<FName, FReconnectionInfo> OutputConnections;

	TArray<class UEdGraphPin*> FuncCallInputPins;
	TArray<class UEdGraphPin*> FuncCallOutputPins;

	//Copies the function graph into the main graph.
	//Removes the Function call in the main graph and the input and output nodes in the function graph, reconnecting their pins appropriately.
	auto MergeFunctionIntoMainGraph = [&](UNiagaraNodeFunctionCall* InFunc, UNiagaraGraph* FuncGraph)
	{
		InputConnections.Empty();
		OutputConnections.Empty();
		FuncCallInputPins.Empty();
		FuncCallOutputPins.Empty();

		check(InFunc && FuncGraph);
		if (InFunc->FunctionScript)
		{
			//Get all the pins that are connected to the inputs of the function call node in the main graph.
			InFunc->GetInputPins(FuncCallInputPins);
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
			InFunc->GetOutputPins(FuncCallOutputPins);
			for (UEdGraphPin* FuncCallOutputPin : FuncCallOutputPins)
			{
				FName OutputName(*FuncCallOutputPin->PinName);
				for (UEdGraphPin* LinkTo : FuncCallOutputPin->LinkedTo)
				{
					check(LinkTo->Direction == EGPD_Input);
					FReconnectionInfo& OutputConnection = OutputConnections.FindOrAdd(OutputName);
					OutputConnection.To.Add(LinkTo);
				}
			}

			//Remove the function call node from the graph now that we have everything we need from it.
			SourceGraph->RemoveNode(InFunc);

			//Keep a list of the Input and Output nodes we see in the function graph so that we can remove (most of) them later.
			TArray<UEdGraphNode*, TInlineAllocator<64>> ToRemove;

			//Search the nodes in the function graph, finding any connections to input or output nodes.
			for (UEdGraphNode* FuncGraphNode : FuncGraph->Nodes)
			{
				if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(FuncGraphNode))
				{
					check(InputNode->Pins.Num() == 1);
					//Get an array of "To" pins from one or more input nodes referencing each named input.
					FReconnectionInfo& InputConnection = InputConnections.FindOrAdd(InputNode->Input.Name);
					if (InputConnection.From)
					{
						//We have a connection from the function call so remove the input node and connect to that.
						ToRemove.Add(InputNode);
					}
					else
					{
						//This input has no connection from the function call so what do we do here? 
						//For now we just leave the input node and connect back to it. 
						//This will mean unconnected pins from the function call will look for constants or attributes. 
						//In some cases we may want to just take the default value from the function call pin instead?
						//Maybe have some properties on the function call defining that.
						InputConnection.From = InputNode->Pins[0];
					}

					TArray<UEdGraphPin*>& LinkToPins = InputNode->Pins[0]->LinkedTo;
					for (UEdGraphPin* ToPin : LinkToPins)
					{
						check(ToPin->Direction == EGPD_Input);
						InputConnection.To.Add(ToPin);
					}
				}
				else if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(FuncGraphNode))
				{
					//Unlike the input nodes, we don't have the option of keeping these if there is no "From" pin. The default values from the node pins should be used.
					ToRemove.Add(OutputNode);

					//For each output, get the "From" pin to be reconnected later.
					for (int32 OutputIdx = 0; OutputIdx < OutputNode->Outputs.Num(); ++OutputIdx)
					{
						FName OutputName = OutputNode->Outputs[OutputIdx].Name;

						UEdGraphPin* OutputNodePin = OutputNode->Pins[OutputIdx];
						check(OutputNodePin->LinkedTo.Num() <= 1);
						FReconnectionInfo& OutputConnection = OutputConnections.FindOrAdd(OutputName);
						UEdGraphPin* LinkFromPin = OutputNodePin->LinkedTo.Num() == 1 ? OutputNodePin->LinkedTo[0] : NULL;
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
			}

			//Remove all the In and Out nodes from the function graph.
			for (UEdGraphNode* Remove : ToRemove)
			{
				FuncGraph->RemoveNode(Remove);
			}

			//Copy the nodes from the function graph over into the main graph.
			FuncGraph->MoveNodesToAnotherGraph(SourceGraph, false);

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
	};

	//Helper struct for traversing nested function calls.
	struct FFunctionContext
	{
		//True if this context's function has been merged into the main graph.
		bool bProcessed;
		//The index of this context into the ContextPool. 
		int32 PoolIdx;
		//Pointer back to the parent context for traversal.
		FFunctionContext* Parent;
		//The function call node for this function in the source/parent graph.
		UNiagaraNodeFunctionCall* Function;
		//The graph for this function that we are going to merge into the main graph.
		UNiagaraGraph* FunctionGraph;
		//The script from which the graph is copied. Used for re entrance check.
		UNiagaraScript* Script;

		//Contexts for function calls in this function graph.
		TArray<FFunctionContext*, TInlineAllocator<64>> SubFunctionCalls;

		FFunctionContext()
			: bProcessed(false)
			, PoolIdx(INDEX_NONE)
			, Parent(NULL)
			, Function(NULL)
			, FunctionGraph(NULL)
			, Script(NULL)
		{
		}

		/**
		We don't allow re-entrant functions as this would cause an infinite loop of merging in graphs.
		Maybe in the future if we allow branching in the VM we can allow this.
		*/
		bool CheckForReentrance()const
		{
			UNiagaraNodeFunctionCall* Func = Function;
			FFunctionContext* Curr = Parent;
			while (Curr)
			{
				if (Curr->Script == Script)
					return true;

				Curr = Curr->Parent;
			}
			return false;
		}

		FString GetCallstack()const
		{
			FString Ret;
			const FFunctionContext* Curr = this;
			while (Curr)
			{
				if (Curr->Script)
				{
					Ret.Append(*(Curr->Script->GetPathName()));
				}
				else
				{
					Ret.Append(TEXT("Unknown"));
				}

				Ret.Append(TEXT("\n"));

				Curr = Curr->Parent;
			}
			return Ret;
		}
	};

	//A pool of contexts on the stack to avoid loads of needless, small heap allocations.
	TArray<FFunctionContext, TInlineAllocator<512>> ContextPool;
	ContextPool.Reserve(512);

	FFunctionContext RootContext;
	FFunctionContext* CurrentContext = &RootContext;
	CurrentContext->FunctionGraph = SourceGraph;
	CurrentContext->Script = Script;

	//Depth first traversal of all function calls.
	while (CurrentContext)
	{
		//Find any sub functions and process this function call.
		if (!CurrentContext->bProcessed)
		{
			CurrentContext->bProcessed = true;

			//Find any sub functions and check for re-entrance.
			if (CurrentContext->FunctionGraph)
			{
				for (UEdGraphNode* Node : CurrentContext->FunctionGraph->Nodes)
				{
					UNiagaraNodeFunctionCall* FuncNode = Cast<UNiagaraNodeFunctionCall>(Node);
					if (FuncNode)
					{
						int32 NewIdx = ContextPool.AddZeroed();
						FFunctionContext* SubFuncContext = &ContextPool[NewIdx];
						CurrentContext->SubFunctionCalls.Push(SubFuncContext);
						SubFuncContext->Parent = CurrentContext;
						SubFuncContext->Function = FuncNode;
						SubFuncContext->PoolIdx = NewIdx;
						SubFuncContext->Script = FuncNode->FunctionScript;

						if (SubFuncContext->CheckForReentrance())
						{
							FString Callstack = SubFuncContext->GetCallstack();
							MessageLog.Error(TEXT("Reentrant function call!\n%s"), *Callstack);
							return false;
						}

						//Copy the function graph as we'll be modifying it as we merge in with the main graph.
						UNiagaraScriptSource* FuncSource = CastChecked<UNiagaraScriptSource>(FuncNode->FunctionScript->Source);
						check(FuncSource);
						SubFuncContext->FunctionGraph = CastChecked<UNiagaraGraph>(FEdGraphUtilities::CloneGraph(FuncSource->NodeGraph, NULL, &MessageLog));
					}
				}
			}

			//Merge this function into the main graph now.
			if (CurrentContext->Function && CurrentContext->FunctionGraph)
			{
				MergeFunctionIntoMainGraph(CurrentContext->Function, CurrentContext->FunctionGraph);
			}
		}

		if (CurrentContext->SubFunctionCalls.Num() > 0)
		{
			//Move to the next sub function.
			CurrentContext = CurrentContext->SubFunctionCalls.Pop();
		}
		else
		{
			//Done processing this function so remove it and move back to the parent.
			if (CurrentContext->PoolIdx != INDEX_NONE)
			{
				CurrentContext->FunctionGraph->MarkPendingKill();

				ContextPool.RemoveAtSwap(CurrentContext->PoolIdx);
			}
			CurrentContext = CurrentContext->Parent;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE