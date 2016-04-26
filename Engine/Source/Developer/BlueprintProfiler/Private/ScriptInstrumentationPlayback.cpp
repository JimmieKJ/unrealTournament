// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintProfilerPCH.h"
#include "EditorStyleSet.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "ScriptInstrumentationPlayback"

DECLARE_CYCLE_STAT(TEXT("Statistic Update"), STAT_StatUpdateCost, STATGROUP_BlueprintProfiler);
DECLARE_CYCLE_STAT(TEXT("Node Lookup"), STAT_NodeLookupCost, STATGROUP_BlueprintProfiler);

//////////////////////////////////////////////////////////////////////////
// FBlueprintExecutionContext

bool FBlueprintExecutionContext::InitialiseContext(const FString& BlueprintPath)
{
	// Locate the blueprint from the path
	if (UObject* ObjectPtr = FindObject<UObject>(nullptr, *BlueprintPath))
	{
		UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(ObjectPtr);
		if (BPClass && BPClass->bHasInstrumentation)
		{
			BlueprintClass = BPClass;
			Blueprint = Cast<UBlueprint>(BPClass->ClassGeneratedBy);
			UbergraphFunctionName = BPClass->UberGraphFunction ? BPClass->UberGraphFunction->GetFName() : NAME_None;
		}
	}
	if (Blueprint.IsValid() && BlueprintClass.IsValid() && UbergraphFunctionName != NAME_None)
	{
		// Create new blueprint exec node
		FScriptExecNodeParams BlueprintParams;
		BlueprintParams.NodeName = FName(*BlueprintPath);
		BlueprintParams.ObservedObject = Blueprint.Get();
		BlueprintParams.OwningGraphName = NAME_None;
		BlueprintParams.DisplayName = FText::FromName(Blueprint.Get()->GetFName());
		BlueprintParams.Tooltip = LOCTEXT("NavigateToBlueprintHyperlink_ToolTip", "Navigate to the Blueprint");
		BlueprintParams.NodeFlags = EScriptExecutionNodeFlags::Class;
		BlueprintParams.Icon = const_cast<FSlateBrush*>(FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPIcon_Normal")));
		BlueprintParams.IconColor = FLinearColor(0.46f, 0.54f, 0.81f);
		BlueprintNode = MakeShareable<FScriptExecutionBlueprint>(new FScriptExecutionBlueprint(BlueprintParams));
		// Map the blueprint execution
		if (MapBlueprintExecution())
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintExecutionContext::HasProfilerDataForInstance(const FName InstanceName) const
{
	bool bHasInstanceData = false;
	if (BlueprintNode.IsValid())
	{
		FName RemappedInstanceName = RemapInstancePath(InstanceName);
		TSharedPtr<FScriptExecutionNode> Result = BlueprintNode->GetInstanceByName(RemappedInstanceName);
		bHasInstanceData = Result.IsValid();
	}
	return bHasInstanceData;
}

FName FBlueprintExecutionContext::RemapInstancePath(const FName InstanceName) const
{
	FName Result = InstanceName;
	if (const FName* RemappedName = PIEInstanceNameMap.Find(InstanceName))
	{
		Result = *RemappedName;
	}
	return Result;
}

TWeakObjectPtr<const UObject> FBlueprintExecutionContext::GetInstance(FName& InstanceNameInOut)
{
	TWeakObjectPtr<const UObject> Result;
	FName CorrectedName = InstanceNameInOut;
	if (const FName* SearchName = PIEInstanceNameMap.Find(InstanceNameInOut)) 
	{
		CorrectedName = *SearchName;
		InstanceNameInOut = CorrectedName;
	}
	if (TWeakObjectPtr<const UObject>* SearchResult = ActorInstances.Find(CorrectedName))
	{
		Result = *SearchResult;
	}
	else
	{
		// Attempt to locate the instance and map PIE objects to editor world objects
		if (const UObject* ObjectPtr = FindObject<UObject>(nullptr, *InstanceNameInOut.ToString()))
		{
			// Get Outer world
			if (UWorld* ObjectWorld = ObjectPtr->GetTypedOuter<UWorld>())
			{
				if (ObjectWorld->WorldType == EWorldType::PIE)
				{
					FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
					if (UWorld* EditorWorld = EditorWorldContext.World())
					{
						for (auto LevelIter : EditorWorld->GetLevels())
						{
							if (UObject* EditorObject = FindObject<UObject>(LevelIter, *ObjectPtr->GetName()))
							{
								CorrectedName = FName(*EditorObject->GetPathName());
								ActorInstances.Add(CorrectedName) = EditorObject;
								PIEInstanceNameMap.Add(InstanceNameInOut) = CorrectedName;
								InstanceNameInOut = CorrectedName;
								Result = EditorObject;
								break;
							}
						}
					}
				}
				else if (!ObjectPtr->HasAnyFlags(RF_Transient))
				{
					ActorInstances.Add(InstanceNameInOut) = ObjectPtr;
					PIEInstanceNameMap.Add(InstanceNameInOut) = InstanceNameInOut;
					Result = ObjectPtr;
				}
			}
		}
	}
	return Result;
}

FName FBlueprintExecutionContext::GetEventFunctionName(const FName EventName) const
{
	return (EventName == UEdGraphSchema_K2::FN_UserConstructionScript) ? UEdGraphSchema_K2::FN_UserConstructionScript : UbergraphFunctionName;
}

TSharedPtr<FBlueprintFunctionContext> FBlueprintExecutionContext::GetFunctionContext(const FName FunctionNameIn) const
{
	TSharedPtr<FBlueprintFunctionContext> Result;
	FName FunctionName = (FunctionNameIn == UEdGraphSchema_K2::GN_EventGraph) ? UbergraphFunctionName : FunctionNameIn;

	if (const TSharedPtr<FBlueprintFunctionContext>* SearchResult = FunctionContexts.Find(FunctionName))
	{
		Result = *SearchResult;
	}
	return Result;
}

TSharedPtr<FBlueprintFunctionContext> FBlueprintExecutionContext::CreateFunctionContext(const FName FunctionNameIn)
{
	FName FunctionName = (FunctionNameIn ==  UEdGraphSchema_K2::GN_EventGraph) ? UbergraphFunctionName : FunctionNameIn;
	TSharedPtr<FBlueprintFunctionContext>& Result = FunctionContexts.FindOrAdd(FunctionName);
	if (!Result.IsValid())
	{
		if (UFunction* Function = BlueprintClass->FindFunctionByName(FunctionName))
		{
			Result = MakeShareable(new FBlueprintFunctionContext(Function, BlueprintClass));
		}
	}
	check(Result.IsValid());
	return Result;
}

bool FBlueprintExecutionContext::HasProfilerDataForNode(const UEdGraphNode* GraphNode) const
{
	SCOPE_CYCLE_COUNTER(STAT_NodeLookupCost);
	const UEdGraph* OuterGraph = GraphNode ? GraphNode->GetTypedOuter<UEdGraph>() : nullptr;
	if (OuterGraph)
	{
		TSharedPtr<FBlueprintFunctionContext> FunctionContext = GetFunctionContext(OuterGraph->GetFName());
		if (FunctionContext.IsValid())
		{
			return FunctionContext->HasProfilerDataForNode(GraphNode->GetFName());
		}
	}
	return false;
}

TSharedPtr<FScriptExecutionNode> FBlueprintExecutionContext::GetProfilerDataForNode(const UEdGraphNode* GraphNode)
{
	SCOPE_CYCLE_COUNTER(STAT_NodeLookupCost);
	TSharedPtr<FScriptExecutionNode> Result;
	const UEdGraph* OuterGraph = GraphNode ? GraphNode->GetTypedOuter<UEdGraph>() : nullptr;
	if (OuterGraph)
	{
		TSharedPtr<FBlueprintFunctionContext> FunctionContext = GetFunctionContext(OuterGraph->GetFName());
		if (FunctionContext.IsValid() && FunctionContext->HasProfilerDataForNode(GraphNode->GetFName()))
		{
			Result = FunctionContext->GetProfilerDataForNode(GraphNode->GetFName());
		}
	}
	return Result;
}

void FBlueprintExecutionContext::UpdateConnectedStats()
{
	SCOPE_CYCLE_COUNTER(STAT_StatUpdateCost);
	if (BlueprintNode.IsValid())
	{
		FTracePath InitialTracePath;
		BlueprintNode->RefreshStats(InitialTracePath);
	}
}

bool FBlueprintExecutionContext::MapBlueprintExecution()
{
	bool bMappingSuccessful = false;

	if (UBlueprint* BlueprintToMap = Blueprint.Get())
	{
		// Find all event nodes for event graph and UCS
		TArray<UEdGraph*> Graphs;
		TArray<UK2Node_Event*> EventNodes;
		TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
		TArray<UK2Node*> InputEventNodes;
		BlueprintToMap->GetAllGraphs(Graphs);
		for (auto GraphIter : Graphs)
		{
			if (GraphIter->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				GraphIter->GetNodesOfClass<UK2Node_Event>(EventNodes);
			}
			if (GraphIter->GetFName() == UEdGraphSchema_K2::FN_UserConstructionScript)
			{
				GraphIter->GetNodesOfClass<UK2Node_FunctionEntry>(FunctionEntryNodes);
			}
			GraphIter->GetNodesOfClassEx<UK2Node_InputAction, UK2Node>(InputEventNodes);
			GraphIter->GetNodesOfClassEx<UK2Node_InputKey, UK2Node>(InputEventNodes);
			GraphIter->GetNodesOfClassEx<UK2Node_InputTouch, UK2Node>(InputEventNodes);
		}
		// Map execution paths for each event node
		for (auto EventIter : EventNodes)
		{
			FName EventName = EventIter->GetFunctionName();
			TSharedPtr<FBlueprintFunctionContext> EventContext = CreateFunctionContext(UbergraphFunctionName);
			FScriptExecNodeParams EventParams;
			EventParams.NodeName = EventName;
			EventParams.ObservedObject = EventIter;
			EventParams.OwningGraphName = NAME_None;
			EventParams.DisplayName = EventIter->GetNodeTitle(ENodeTitleType::ListView);
			EventParams.Tooltip = LOCTEXT("NavigateToEventLocationHyperlink_ToolTip", "Navigate to the Event");
			EventParams.NodeFlags = EScriptExecutionNodeFlags::Event;
			EventParams.IconColor = FLinearColor(0.91f, 0.16f, 0.16f);
			const FSlateBrush* EventIcon = EventIter->ShowPaletteIconOnNode() ?	FEditorStyle::GetBrush(EventIter->GetPaletteIcon(EventParams.IconColor)) :
																				FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPNode"));
			EventParams.Icon = const_cast<FSlateBrush*>(EventIcon);
			TSharedPtr<FScriptExecutionNode> EventExecNode = EventContext->CreateExecutionNode(EventParams);
			// Add event to mapped blueprint
			BlueprintNode->AddChildNode(EventExecNode);
			// Map the event execution flow
			TSharedPtr<FScriptExecutionNode> MappedEvent = MapNodeExecution(EventContext, EventIter);
			if (MappedEvent.IsValid())
			{
				EventExecNode->AddChildNode(MappedEvent);
			}
		}
		// Create Input events
		CreateInputEvents(InputEventNodes);
		// Map execution paths for each function entry node
		for (auto FunctionIter : FunctionEntryNodes)
		{
			FName FunctionName = FunctionIter->SignatureName;
			TSharedPtr<FBlueprintFunctionContext> FunctionContext = CreateFunctionContext(FunctionName);
			FScriptExecNodeParams FunctionParams;
			FunctionParams.NodeName = FunctionName;
			FunctionParams.ObservedObject = FunctionIter;
			FunctionParams.OwningGraphName = NAME_None;
			FunctionParams.DisplayName = FunctionIter->GetNodeTitle(ENodeTitleType::ListView);
			FunctionParams.Tooltip = LOCTEXT("NavigateToFunctionLocationHyperlink_ToolTip", "Navigate to the Function");
			FunctionParams.NodeFlags = EScriptExecutionNodeFlags::Event;
			FunctionParams.IconColor = FLinearColor(0.91f, 0.16f, 0.16f);
			const FSlateBrush* FunctionIcon = FunctionIter->ShowPaletteIconOnNode() ?	FEditorStyle::GetBrush(FunctionIter->GetPaletteIcon(FunctionParams.IconColor)) :
																						FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPNode"));
			FunctionParams.Icon = const_cast<FSlateBrush*>(FunctionIcon);
			TSharedPtr<FScriptExecutionNode> FunctionExecNode = FunctionContext->CreateExecutionNode(FunctionParams);
			// Add Function to mapped blueprint
			BlueprintNode->AddChildNode(FunctionExecNode);
			// Map the Function execution flow
			TSharedPtr<FScriptExecutionNode> MappedFunction = MapNodeExecution(FunctionContext, FunctionIter);
			if (MappedFunction.IsValid())
			{
				FunctionExecNode->AddChildNode(MappedFunction);
			}
		}
		bMappingSuccessful = true;
	}
	return bMappingSuccessful;
}

void FBlueprintExecutionContext::CreateInputEvents(const TArray<UK2Node*>& InputEventNodes)
{
	struct FInputEventDesc
	{
		UEdGraphNode* GraphNode;
		UFunction* EventFunction;
	};
	TArray<FInputEventDesc> InputEventDescs;
	// Extract basic event info
	if (UBlueprintGeneratedClass* BPClass = BlueprintClass.Get())
	{
		FInputEventDesc NewInputDesc;
		for (auto EventNode : InputEventNodes)
		{
			if (EventNode)
			{
				if (UK2Node_InputAction* InputActionNode = Cast<UK2Node_InputAction>(EventNode))
				{
					const FString CustomEventName = FString::Printf(TEXT("InpActEvt_%s_%s"), *InputActionNode->InputActionName.ToString());
					for (auto FunctionIter = TFieldIterator<UFunction>(BPClass); FunctionIter; ++FunctionIter)
					{
						if (FunctionIter->GetName().Contains(CustomEventName))
						{
							NewInputDesc.GraphNode = EventNode;
							NewInputDesc.EventFunction = *FunctionIter;
							InputEventDescs.Add(NewInputDesc);
						}
					}
				}
				else if (UK2Node_InputKey* InputKeyNode = Cast<UK2Node_InputKey>(EventNode))
				{
					const FName ModifierName = InputKeyNode->GetModifierName();
					const FString CustomEventName = ModifierName != NAME_None ? FString::Printf(TEXT("InpActEvt_%s_%s"), *ModifierName.ToString(), *InputKeyNode->InputKey.ToString()) :
																				FString::Printf(TEXT("InpActEvt_%s"), *InputKeyNode->InputKey.ToString());
					for (auto FunctionIter = TFieldIterator<UFunction>(BPClass); FunctionIter; ++FunctionIter)
					{
						if (FunctionIter->GetName().Contains(CustomEventName))
						{
							NewInputDesc.GraphNode = EventNode;
							NewInputDesc.EventFunction = *FunctionIter;
							InputEventDescs.Add(NewInputDesc);
						}
					}
				}
				else if (UK2Node_InputTouch* InputTouchNode = Cast<UK2Node_InputTouch>(EventNode))
				{
					struct EventPinData
					{
						EventPinData(UEdGraphPin* InPin,TEnumAsByte<EInputEvent> InEvent ){	Pin=InPin;EventType=InEvent;};
						UEdGraphPin* Pin;
						TEnumAsByte<EInputEvent> EventType;
					};
					TArray<UEdGraphPin*> ActivePins;
					if (UEdGraphPin* InputTouchPressedPin = InputTouchNode->GetPressedPin())
					{
						if (InputTouchPressedPin->LinkedTo.Num() > 0)
						{
							ActivePins.Add(InputTouchPressedPin);
						}
					}
					if (UEdGraphPin* InputTouchReleasedPin = InputTouchNode->GetReleasedPin())
					{
						if (InputTouchReleasedPin->LinkedTo.Num() > 0)
						{
							ActivePins.Add(InputTouchReleasedPin);
						}
					}
					if (UEdGraphPin* InputTouchMovedPin = InputTouchNode->GetMovedPin())
					{
						if (InputTouchMovedPin->LinkedTo.Num() > 0)
						{
							ActivePins.Add(InputTouchMovedPin);
						}
					}
					for (auto Pin : ActivePins)
					{
						const FString CustomEventName = FString::Printf(TEXT("InpTchEvt_%s"), *Pin->GetName());
						for (auto FunctionIter = TFieldIterator<UFunction>(BPClass); FunctionIter; ++FunctionIter)
						{
							if (FunctionIter->GetName().Contains(CustomEventName))
							{
								NewInputDesc.GraphNode = EventNode;
								NewInputDesc.EventFunction = *FunctionIter;
								InputEventDescs.Add(NewInputDesc);
							}
						}
					}
				}
			}
		}
	}
	// Build event contexts
	for (auto InputEventDesc : InputEventDescs)
	{
		UEdGraphNode* GraphNode = InputEventDesc.GraphNode;
		FName EventName = InputEventDesc.EventFunction->GetFName();
		TSharedPtr<FBlueprintFunctionContext> EventContext = CreateFunctionContext(UbergraphFunctionName);
		FScriptExecNodeParams EventParams;
		EventParams.NodeName = EventName;
		EventParams.ObservedObject = GraphNode;
		EventParams.OwningGraphName = NAME_None;
		EventParams.DisplayName = GraphNode->GetNodeTitle(ENodeTitleType::ListView);
		EventParams.Tooltip = LOCTEXT("NavigateToEventLocationHyperlink_ToolTip", "Navigate to the Event");
		EventParams.NodeFlags = EScriptExecutionNodeFlags::Event;
		EventParams.IconColor = FLinearColor(0.91f, 0.16f, 0.16f);
		const FSlateBrush* EventIcon = GraphNode->ShowPaletteIconOnNode() ?	FEditorStyle::GetBrush(GraphNode->GetPaletteIcon(EventParams.IconColor)) :
																			FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPNode"));
		EventParams.Icon = const_cast<FSlateBrush*>(EventIcon);
		TSharedPtr<FScriptExecutionNode> EventExecNode = EventContext->CreateExecutionNode(EventParams);
		// Add Function to mapped blueprint
		BlueprintNode->AddChildNode(EventExecNode);
		// Map the Function execution flow
		TSharedPtr<FScriptExecutionNode> MappedFunction = MapNodeExecution(EventContext, GraphNode);
		if (MappedFunction.IsValid())
		{
			EventExecNode->AddChildNode(MappedFunction);
		}
	}
}

TSharedPtr<FScriptExecutionNode> FBlueprintExecutionContext::MapNodeExecution(TSharedPtr<FBlueprintFunctionContext> FunctionContext, UEdGraphNode* NodeToMap)
{
	TSharedPtr<FScriptExecutionNode> MappedNode;
	if (NodeToMap)
	{
		MappedNode = FunctionContext->GetProfilerDataForNode(NodeToMap->GetFName());
		if (!MappedNode.IsValid())
		{
			FScriptExecNodeParams NodeParams;
			NodeParams.NodeName = NodeToMap->GetFName();
			NodeParams.ObservedObject = NodeToMap;
			NodeParams.DisplayName = NodeToMap->GetNodeTitle(ENodeTitleType::ListView);
			NodeParams.Tooltip = LOCTEXT("NavigateToNodeLocationHyperlink_ToolTip", "Navigate to the Node");
			NodeParams.NodeFlags = EScriptExecutionNodeFlags::Node;
			NodeParams.IconColor = FLinearColor(1.f, 1.f, 1.f, 0.8f);
			const FSlateBrush* NodeIcon = NodeToMap->ShowPaletteIconOnNode() ?	FEditorStyle::GetBrush(NodeToMap->GetPaletteIcon(NodeParams.IconColor)) :
																				FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPNode"));
			NodeParams.Icon = const_cast<FSlateBrush*>(NodeIcon);
			MappedNode = FunctionContext->CreateExecutionNode(NodeParams);
			// Evaluate Execution and Input Pins
			TArray<UEdGraphPin*> ExecPins;
			TArray<UEdGraphPin*> InputPins;
			for (auto Pin : NodeToMap->Pins)
			{
				if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					ExecPins.Add(Pin);
				}
				else if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					InputPins.Add(Pin);
				}
			}

			if (ExecPins.Num() == 0)
			{
				MappedNode->AddFlags(EScriptExecutionNodeFlags::PureNode);
			}
			else
			{
				MappedNode->SetPureNodeScriptCodeRange(FunctionContext->GetPureNodeScriptCodeRange(NodeToMap));
			}

			const bool bBranchNode = ExecPins.Num() > 1;
			if (bBranchNode)
			{
				// Determine branch exec type
				if (NodeToMap->IsA<UK2Node_ExecutionSequence>())
				{
					MappedNode->AddFlags(EScriptExecutionNodeFlags::SequentialBranch);
				}
				else
				{
					MappedNode->AddFlags(EScriptExecutionNodeFlags::ConditionalBranch);
				}
			}
			// Evaluate non-exec input pins (pure node execution chains)
			TArray<int32> PinScriptCodeOffsets;
			for (auto InputPin : InputPins)
			{
				// If this input pin is linked to a pure node in the source graph, create and map all known execution paths for it.
				for (auto LinkedPin : InputPin->LinkedTo)
				{
					// Note: Intermediate pure nodes can have output pins that masquerade as impure node output pins when links are "moved" from the source graph (thus
					// resulting in a false association here with one or more script code offsets), so we must first ensure that the link is really to a pure node output.
					UK2Node* OwningNode = Cast<UK2Node>(LinkedPin->GetOwningNode());
					if (OwningNode && OwningNode->IsNodePure())
					{
						FunctionContext->GetAllCodeLocationsFromPin(LinkedPin, PinScriptCodeOffsets);
						if (PinScriptCodeOffsets.Num() > 0)
						{
							TSharedPtr<FScriptExecutionNode> PureNode = MapNodeExecution(FunctionContext, OwningNode);
							check(PureNode.IsValid() && PureNode->IsPureNode());
							for (int32 i = 0; i < PinScriptCodeOffsets.Num(); ++i)
							{
								MappedNode->AddPureNode(PinScriptCodeOffsets[i], PureNode);
							}
						}
					}
				}
			}
			for (auto Pin : ExecPins)
			{
				const UEdGraphPin* LinkedPin = Pin->LinkedTo.Num() > 0 ? Pin->LinkedTo[0] : nullptr;

				if (bBranchNode)
				{
					const int32 PinScriptCodeOffset = FunctionContext->GetCodeLocationFromPin(Pin);
					// Check we have a valid script offset or that the pin isn't linked.
					check((PinScriptCodeOffset != INDEX_NONE)||(LinkedPin == nullptr));
					FScriptExecNodeParams LinkNodeParams;
					LinkNodeParams.NodeName = Pin->GetFName();
					LinkNodeParams.ObservedObject = Pin;
					LinkNodeParams.DisplayName = Pin->GetDisplayName();
					LinkNodeParams.Tooltip = LOCTEXT("ExecPin_ToolTip", "Expand execution path");
					LinkNodeParams.NodeFlags = EScriptExecutionNodeFlags::ExecPin;
					LinkNodeParams.IconColor = FLinearColor(1.f, 1.f, 1.f, 0.8f);
					const FSlateBrush* Icon = LinkedPin ?	FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPPinConnected")) : 
															FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPPinDisconnected"));
					LinkNodeParams.Icon = const_cast<FSlateBrush*>(Icon);
					TSharedPtr<FScriptExecutionNode> LinkedNode = FunctionContext->CreateExecutionNode(LinkNodeParams);
					MappedNode->AddLinkedNode(PinScriptCodeOffset, LinkedNode);
					// Map pin exec flow
					if (LinkedPin)
					{
						TSharedPtr<FScriptExecutionNode> ExecPinNode = MapNodeExecution(FunctionContext, LinkedPin->GetOwningNode());
						if (ExecPinNode.IsValid())
						{
							LinkedNode->AddChildNode(ExecPinNode);
						}
					}
				}
				else if (LinkedPin)
				{
					TSharedPtr<FScriptExecutionNode> ExecLinkedNode = MapNodeExecution(FunctionContext, LinkedPin->GetOwningNode());
					if (ExecLinkedNode.IsValid())
					{
						const int32 PinScriptCodeOffset = FunctionContext->GetCodeLocationFromPin(Pin);
						check(PinScriptCodeOffset != INDEX_NONE);
						MappedNode->AddLinkedNode(PinScriptCodeOffset, ExecLinkedNode);
					}
				}
			}
			// Evaluate Children for call sites
			if (UK2Node_CallFunction* FunctionCallSite = Cast<UK2Node_CallFunction>(NodeToMap))
			{
				const UEdGraphNode* FunctionNode = nullptr; 
				if (UEdGraph* CalledGraph = FunctionCallSite->GetFunctionGraph(FunctionNode))
				{
					// Update Exec node
					MappedNode->AddFlags(EScriptExecutionNodeFlags::FunctionCall);
					MappedNode->SetIconColor(FLinearColor(0.46f, 0.54f, 0.95f));
					MappedNode->SetToolTipText(LOCTEXT("NavigateToFunctionLocationHyperlink_ToolTip", "Navigate to the Function Callsite"));
					// Update the function context
					FunctionContext = CreateFunctionContext(CalledGraph->GetFName());
					// Grab function entries
					TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
					CalledGraph->GetNodesOfClass<UK2Node_FunctionEntry>(FunctionEntryNodes);
	
					for (auto EntryNodeIter : FunctionEntryNodes)
					{
						TSharedPtr<FScriptExecutionNode> FunctionEntry = MapNodeExecution(FunctionContext, EntryNodeIter);
						MappedNode->AddChildNode(FunctionEntry);
					}
				}
			}
			else if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(NodeToMap))
			{
				MappedNode->AddFlags(EScriptExecutionNodeFlags::CustomEvent);
			}
			//else if (UK2Node_MacroInstance* MacroCallSite = Cast<UK2Node_MacroInstance>(NodeToMap))
			//{
			//	if (UEdGraph* MacroGraph = MacroCallSite->GetMacroGraph())
			//	{
			//		// Update Exec node
			//		MappedNode->AddFlags(EScriptExecutionNodeFlags::MacroCall);
			//		IconColor = FLinearColor(0.36f, 0.34f, 0.71f);
			//		ToolTipText = LOCTEXT("NavigateToMacroLocationHyperlink_ToolTip", "Navigate to the Macro Callsite");
			//		// Grab the macro blueprint context
			//		UBlueprint* MacroBP = MacroGraph->GetTypedOuter<UBlueprint>();
			//		UBlueprintGeneratedClass* MacroBPGC = Cast<UBlueprintGeneratedClass>(MacroBP->GeneratedClass);
			//		TSharedPtr<FBlueprintExecutionContext> MacroBlueprintContext = GetBlueprintContext(MacroBPGC->GetPathName());
			//		// Grab function entries
			//		TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
			//		MacroGraph->GetNodesOfClass<UK2Node_FunctionEntry>(FunctionEntryNodes);
	
			//		for (auto EntryNodeIter : FunctionEntryNodes)
			//		{
			//			TSharedPtr<FScriptExecutionNode> MacroEntry = MapNodeExecution(MacroBlueprintContext, EntryNodeIter);
			//			MappedNode->AddChildNode(MacroEntry);
			//		}
			//	}
			//}
		}
	}
	return MappedNode;
}
//////////////////////////////////////////////////////////////////////////
// FBlueprintFunctionContext

bool FBlueprintFunctionContext::HasProfilerDataForNode(const FName NodeName) const
{
	return ExecutionNodes.Contains(NodeName);
}

TSharedPtr<FScriptExecutionNode> FBlueprintFunctionContext::GetProfilerDataForNode(const FName NodeName)
{
	TSharedPtr<FScriptExecutionNode> Result;
	if (TSharedPtr<FScriptExecutionNode>* SearchResult = ExecutionNodes.Find(NodeName))
	{
		Result = *SearchResult;
	}
	return Result;
}

const UEdGraphNode* FBlueprintFunctionContext::GetNodeFromCodeLocation(const int32 ScriptOffset)
{
	TWeakObjectPtr<const UEdGraphNode>& Result = ScriptOffsetToNodes.FindOrAdd(ScriptOffset);
	if (!Result.IsValid() && BlueprintClass.IsValid())
	{
		if (const UEdGraphNode* GraphNode = BlueprintClass->GetDebugData().FindSourceNodeFromCodeLocation(Function.Get(), ScriptOffset, true))
		{
			Result = GraphNode;
		}
	}
	return Result.Get();
}

const UEdGraphPin* FBlueprintFunctionContext::GetPinFromCodeLocation(const int32 ScriptOffset)
{
	TWeakObjectPtr<const UEdGraphPin>& Result = ScriptOffsetToPins.FindOrAdd(ScriptOffset);
	if (!Result.IsValid() && BlueprintClass.IsValid())
	{
		if (const UEdGraphPin* GraphPin = BlueprintClass->GetDebugData().FindSourcePinFromCodeLocation(Function.Get(), ScriptOffset))
		{
			Result = GraphPin;
		}
	}
	return Result.Get();
}

const int32 FBlueprintFunctionContext::GetCodeLocationFromPin(const UEdGraphPin* Pin) const
{
	if (BlueprintClass.IsValid())
	{
		return BlueprintClass.Get()->GetDebugData().FindCodeLocationFromSourcePin(Pin, Function.Get());
	}

	return INDEX_NONE;
}

void FBlueprintFunctionContext::GetAllCodeLocationsFromPin(const UEdGraphPin* Pin, TArray<int32>& OutCodeLocations) const
{
	if (BlueprintClass.IsValid())
	{
		BlueprintClass.Get()->GetDebugData().FindAllCodeLocationsFromSourcePin(Pin, Function.Get(), OutCodeLocations);
	}
}

FInt32Range FBlueprintFunctionContext::GetPureNodeScriptCodeRange(const UEdGraphNode* Node) const
{
	if (BlueprintClass.IsValid())
	{
		return BlueprintClass->GetDebugData().FindPureNodeScriptCodeRangeFromSourceNode(Node, Function.Get());
	}

	return FInt32Range(INDEX_NONE);
}

TSharedPtr<FScriptExecutionNode> FBlueprintFunctionContext::CreateExecutionNode(FScriptExecNodeParams& InitParams)
{
	TSharedPtr<FScriptExecutionNode>& ScriptExecNode = ExecutionNodes.FindOrAdd(InitParams.NodeName);
	check(!ScriptExecNode.IsValid());
	UEdGraph* OuterGraph = InitParams.ObservedObject.IsValid() ? InitParams.ObservedObject.Get()->GetTypedOuter<UEdGraph>() : nullptr;
	InitParams.OwningGraphName = OuterGraph ? OuterGraph->GetFName() : NAME_None;
	ScriptExecNode = MakeShareable(new FScriptExecutionNode(InitParams));
	return ScriptExecNode;
}

//////////////////////////////////////////////////////////////////////////
// FScriptEventPlayback

bool FScriptEventPlayback::Process(const TArray<FScriptInstrumentedEvent>& SignalData, const int32 StartIdx, const int32 StopIdx)
{
	struct PerNodeEventHelper
	{
		TArray<FTracePath> EntryTracePaths;
		TArray<FTracePath> InputTracePaths;
		TSharedPtr<FScriptExecutionNode> ExecNode;
		TSharedPtr<FBlueprintFunctionContext> FunctionContext;
		TArray<FScriptInstrumentedEvent> UnproccessedEvents;
	};

	const int32 NumEvents = (StopIdx+1) - StartIdx;
	ProcessingState = EEventProcessingResult::Failed;
	const bool bEventIsResuming = SignalData[StartIdx].IsResumeEvent() ;

	if (BlueprintContext.IsValid() && InstanceName != NAME_None)
	{
		check(SignalData[StartIdx].IsEvent());
		EventName = bEventIsResuming ? EventName : SignalData[StartIdx].GetFunctionName();
		FName CurrentFunctionName = BlueprintContext->GetEventFunctionName(SignalData[StartIdx].GetFunctionName());
		TSharedPtr<FBlueprintFunctionContext> FunctionContext = BlueprintContext->GetFunctionContext(CurrentFunctionName);
		TSharedPtr<FScriptExecutionNode> EventNode = FunctionContext->GetProfilerDataForNode(EventName);
		// Find Associated graph nodes and submit into a node map for later processing.
		TMap<const UEdGraphNode*, PerNodeEventHelper> NodeMap;
		ProcessingState = EEventProcessingResult::Success;
		int32 LastEventIdx = StartIdx;
		const int32 EventStartOffset = SignalData[StartIdx].IsResumeEvent() ? 3 : 1;

		for (int32 SignalIdx = StartIdx + EventStartOffset; SignalIdx < StopIdx; ++SignalIdx)
		{
			const FScriptInstrumentedEvent& CurrSignal = SignalData[SignalIdx];
			// Update script function.
			if (CurrentFunctionName != CurrSignal.GetFunctionName())
			{
				CurrentFunctionName = CurrSignal.GetFunctionName();
				FunctionContext = BlueprintContext->GetFunctionContext(CurrentFunctionName);
				check(FunctionContext.IsValid());
			}
			if (const UEdGraphNode* GraphNode = FunctionContext->GetNodeFromCodeLocation(CurrSignal.GetScriptCodeOffset()))
			{
				PerNodeEventHelper& NodeEntry = NodeMap.FindOrAdd(GraphNode);
				if (!NodeEntry.FunctionContext.IsValid())
				{
					NodeEntry.FunctionContext = FunctionContext;
					NodeEntry.ExecNode = FunctionContext->GetProfilerDataForNode(GraphNode->GetFName());
				}
				switch (CurrSignal.GetType())
				{
					case EScriptInstrumentation::PureNodeEntry:
					{
						if (const UEdGraphPin* Pin = FunctionContext->GetPinFromCodeLocation(CurrSignal.GetScriptCodeOffset()))
						{
							if (NodeEntry.FunctionContext->HasProfilerDataForNode(Pin->GetOwningNode()->GetFName()))
							{
								TracePath.AddExitPin(CurrSignal.GetScriptCodeOffset());
								NodeEntry.InputTracePaths.Insert(TracePath, 0);
							}
						}
						NodeEntry.UnproccessedEvents.Add(CurrSignal);
						break;
					}
					case EScriptInstrumentation::NodeEntry:
					{
						// Handle timings for events called as functions
						if (NodeEntry.ExecNode->IsCustomEvent())
						{
							// Ensure this is a different event
							if (const UK2Node_Event* EventGraphNode = Cast<UK2Node_Event>(GraphNode))
							{
								const FName NewEventName = EventGraphNode->GetFunctionName();
								if (NewEventName != EventName)
								{
									TracePath.Reset();
									EventTimings.Add(EventName) = SignalData[SignalIdx].GetTime() - SignalData[LastEventIdx].GetTime();
									EventName = NewEventName;
									EventNode = FunctionContext->GetProfilerDataForNode(EventName);
									LastEventIdx = SignalIdx;
								}
							}
						}
						NodeEntry.EntryTracePaths.Push(TracePath);
						NodeEntry.UnproccessedEvents.Add(CurrSignal);
						break;
					}
					case EScriptInstrumentation::PushState:
					{
						TraceStack.Push(TracePath);
						NodeEntry.UnproccessedEvents.Add(CurrSignal);
						break;
					}
					case EScriptInstrumentation::RestoreState:
					{
						check(TraceStack.Num());
						TracePath = TraceStack.Last();
						break;
					}
					case EScriptInstrumentation::SuspendState:
					{
						ProcessingState = EEventProcessingResult::Suspended;
						break;
					}
					case EScriptInstrumentation::PopState:
					{
						if (TraceStack.Num())
						{
							TracePath = TraceStack.Pop();
						}
						// Consolidate enclosed timings for execution sequence nodes
						if (GraphNode->IsA<UK2Node_ExecutionSequence>())
						{
							FScriptInstrumentedEvent OverrideEvent(CurrSignal);
							OverrideEvent.OverrideType(EScriptInstrumentation::NodeExit);
							NodeEntry.UnproccessedEvents.Add(OverrideEvent);
							int32 NodeEntryIdx = INDEX_NONE;
							for (int32 NodeEventIdx = NodeEntry.UnproccessedEvents.Num()-2; NodeEventIdx >= 0; --NodeEventIdx)
							{
								const FScriptInstrumentedEvent& NodeEvent = NodeEntry.UnproccessedEvents[NodeEventIdx];

								if (NodeEvent.GetType() == EScriptInstrumentation::NodeEntry)
								{
									if (NodeEntryIdx != INDEX_NONE)
									{
										// Keep only the first node entry.
										NodeEntry.UnproccessedEvents.RemoveAt(NodeEventIdx, 1);
									}
									NodeEntryIdx = NodeEventIdx;
								}
								else if (NodeEvent.GetType() == EScriptInstrumentation::NodeExit)
								{
									// Strip out all node exits apart from the PopExecution overridden event.
									NodeEntry.UnproccessedEvents.RemoveAt(NodeEventIdx, 1);
								}
							}
						}
						break;
					}
					case EScriptInstrumentation::NodeExit:
					{
						// Cleanup branching multiple exits and correct the tracepath
						if (NodeEntry.UnproccessedEvents.Num() && NodeEntry.UnproccessedEvents.Last().GetType() == EScriptInstrumentation::NodeExit)
						{
							NodeEntry.UnproccessedEvents.Pop();
							if (NodeEntry.EntryTracePaths.Num())
							{
								TracePath = NodeEntry.EntryTracePaths.Last();
							}
						}
						TracePath.AddExitPin(CurrSignal.GetScriptCodeOffset());
						NodeEntry.UnproccessedEvents.Add(CurrSignal);
						break;
					}
					default:
					{
						NodeEntry.UnproccessedEvents.Add(CurrSignal);
						break;
					}
				}
			}
		}
		// Process last event timing
		double* TimingData = bEventIsResuming ? EventTimings.Find(EventName) : nullptr;
		if (TimingData)
		{
			*TimingData += SignalData[StopIdx].GetTime() - SignalData[LastEventIdx].GetTime();
		}
		else
		{
			EventTimings.Add(EventName) = SignalData[StopIdx].GetTime() - SignalData[LastEventIdx].GetTime();
		}
		// Process outstanding event timings, adding to previous timings if existing.
		if (ProcessingState != EEventProcessingResult::Suspended)
		{
			for (auto EventTiming : EventTimings)
			{
				FTracePath EventTracePath;
				const FName EventFunctionName = BlueprintContext->GetEventFunctionName(EventTiming.Key);
				FunctionContext = BlueprintContext->GetFunctionContext(EventFunctionName);
				check (FunctionContext.IsValid());
				EventNode = FunctionContext->GetProfilerDataForNode(EventTiming.Key);
				check (EventNode.IsValid());
				TSharedPtr<FScriptPerfData> PerfData = EventNode->GetPerfDataByInstanceAndTracePath(InstanceName, EventTracePath);
				PerfData->AddEventTiming(0.0, SignalData[StopIdx].GetTime() - SignalData[LastEventIdx].GetTime());
			}
			EventTimings.Reset();
		}
		// Process Node map timings -- this can probably be rolled back into submission during the initial processing and lose this extra iteration.
		for (auto NodeEntry : NodeMap)
		{
			TSharedPtr<FScriptExecutionNode> ExecNode = NodeEntry.Value.ExecNode;
			double PureNodeEntryTime = 0.0;
			double LastPureNodeTime = 0.0;
			double NodeEntryTime = 0.0;
			int32 TracePathIdx = 0;
			for (auto EventIter = NodeEntry.Value.UnproccessedEvents.CreateIterator(); EventIter; ++EventIter)
			{
				switch(EventIter->GetType())
				{
					case EScriptInstrumentation::PushState:
					{
						LastPureNodeTime = EventIter->GetTime();
						break;
					}
					case EScriptInstrumentation::PureNodeEntry:
					{
						if (PureNodeEntryTime == 0.0)
						{
							PureNodeEntryTime = EventIter->GetTime();
						}

						const int32 ScriptCodeOffset = EventIter->GetScriptCodeOffset();
						if (const UEdGraphPin* Pin = FunctionContext->GetPinFromCodeLocation(ScriptCodeOffset))
						{
							if (FunctionContext->HasProfilerDataForNode(Pin->GetOwningNode()->GetFName()))
							{
								TSharedPtr<FScriptExecutionNode> PureNode = NodeEntry.Value.FunctionContext->GetProfilerDataForNode(Pin->GetOwningNode()->GetFName());
								TSharedPtr<FScriptPerfData> PerfData = PureNode->GetPerfDataByInstanceAndTracePath(InstanceName, NodeEntry.Value.InputTracePaths.Pop());
								PerfData->AddEventTiming(0.0f, EventIter->GetTime() - LastPureNodeTime);
							}
						}

						LastPureNodeTime = EventIter->GetTime();
						break;
					}
					case EScriptInstrumentation::NodeEntry:
					{
						if (NodeEntryTime == 0.0)
						{
							NodeEntryTime = EventIter->GetTime();
						}
						break;
					}
					case EScriptInstrumentation::NodeExit:
					{
						check(NodeEntryTime != 0.0 );
						const FTracePath NodeTracePath = NodeEntry.Value.EntryTracePaths[TracePathIdx++];
						TSharedPtr<FScriptPerfData> PerfData = ExecNode->GetPerfDataByInstanceAndTracePath(InstanceName, NodeTracePath);
						double PureNodeDuration = PureNodeEntryTime != 0.0 ? (NodeEntryTime - PureNodeEntryTime) : 0.0;
						double NodeDuration = EventIter->GetTime() - NodeEntryTime;
						PerfData->AddEventTiming(PureNodeDuration, NodeDuration);
						PureNodeEntryTime = NodeEntryTime = 0.0;
						break;
					}
				}
			}
		}
	}
	return ProcessingState != EEventProcessingResult::Failed;
}

#undef LOCTEXT_NAMESPACE
