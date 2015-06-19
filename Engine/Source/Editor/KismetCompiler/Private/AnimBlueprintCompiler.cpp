// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "KismetCompilerPrivatePCH.h"
#include "KismetEditorUtilities.h"

#include "AnimBlueprintCompiler.h"
#include "../../../Runtime/Engine/Classes/Kismet/KismetArrayLibrary.h"
#include "../../../Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetReinstanceUtilities.h"
#include "AnimationGraphSchema.h"
#include "AnimStateNode.h"
#include "AnimStateConduitNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationCustomTransitionGraph.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationTransitionGraph.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_CustomTransitionResult.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_TransitionPoseEvaluator.h"
#include "AnimGraphNode_TransitionResult.h"
#include "K2Node_TransitionRuleGetter.h"

#define LOCTEXT_NAMESPACE "AnimBlueprintCompiler"

//
// Forward declarations.
//
class UAnimStateNodeBase;

//////////////////////////////////////////////////////////////////////////

template<>
FString FNetNameMapping::MakeBaseName<UAnimGraphNode_Base>(const UAnimGraphNode_Base* Net)
{
	return FString::Printf(TEXT("%s_%s"), *Net->GetDescriptiveCompiledName(), *Net->NodeGuid.ToString());
}

//////////////////////////////////////////////////////////////////////////
// FAnimBlueprintCompiler::FEffectiveConstantRecord

bool FAnimBlueprintCompiler::FEffectiveConstantRecord::Apply(UObject* Object)
{
	uint8* StructPtr = NodeVariableProperty->ContainerPtrToValuePtr<uint8>(Object);
	uint8* PropertyPtr = ConstantProperty->ContainerPtrToValuePtr<uint8>(StructPtr);

	if (ArrayIndex != INDEX_NONE)
	{
		UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(ConstantProperty);

		// Peer inside the array
		FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyPtr);

		if (ArrayHelper.IsValidIndex(ArrayIndex))
		{
			FBlueprintEditorUtils::ImportKismetDefaultValueToProperty(LiteralSourcePin, ArrayProperty->Inner, ArrayHelper.GetRawPtr(ArrayIndex), Object);
		}
		else
		{
			return false;
		}
	}
	else
	{
		FBlueprintEditorUtils::ImportKismetDefaultValueToProperty(LiteralSourcePin, ConstantProperty, PropertyPtr, Object);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// FAnimBlueprintCompiler

FAnimBlueprintCompiler::FAnimBlueprintCompiler(UAnimBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions, TArray<UObject*>* InObjLoaded)
	: FKismetCompilerContext(SourceSketch, InMessageLog, InCompileOptions, InObjLoaded)
	, AnimBlueprint(SourceSketch)
	, bIsDerivedAnimBlueprint(false)
{
	// Determine if there is an anim blueprint in the ancestry of this class
	bIsDerivedAnimBlueprint = UAnimBlueprint::FindRootAnimBlueprint(AnimBlueprint) != NULL;
}

FAnimBlueprintCompiler::~FAnimBlueprintCompiler()
{
}

UEdGraphSchema_K2* FAnimBlueprintCompiler::CreateSchema()
{
	AnimSchema = NewObject<UAnimationGraphSchema>();
	return AnimSchema;
}

UK2Node_CallFunction* FAnimBlueprintCompiler::SpawnCallAnimInstanceFunction(UEdGraphNode* SourceNode, FName FunctionName)
{
	//@TODO: SKELETON: This is a call on a parent function (UAnimInstance::StaticClass() specifically), should we treat it as self or not?
	UK2Node_CallFunction* FunctionCall = SpawnIntermediateNode<UK2Node_CallFunction>(SourceNode);
	FunctionCall->FunctionReference.SetSelfMember(FunctionName);
	FunctionCall->AllocateDefaultPins();

	return FunctionCall;
}

void FAnimBlueprintCompiler::CreateEvaluationHandler(UAnimGraphNode_Base* VisualAnimNode, FEvaluationHandlerRecord& Record)
{
	// Shouldn't create a handler if there is nothing to work with
	check(Record.ServicedProperties.Num() > 0);
	check(Record.NodeVariableProperty != NULL);

	//@TODO: Want to name these better
	const FString FunctionName = FString::Printf(TEXT("%s_%s_%s"), *Record.EvaluationHandlerProperty->GetName(), *VisualAnimNode->GetOuter()->GetName(), *VisualAnimNode->GetName());
	Record.HandlerFunctionName = FName(*FunctionName);
	
	// Add a custom event in the graph
	UK2Node_CustomEvent* EntryNode = SpawnIntermediateNode<UK2Node_CustomEvent>(VisualAnimNode, ConsolidatedEventGraph);
	EntryNode->bInternalEvent = true;
	EntryNode->CustomFunctionName = Record.HandlerFunctionName;
	EntryNode->AllocateDefaultPins();

	// The ExecChain is the current exec output pin in the linear chain
	UEdGraphPin* ExecChain = Schema->FindExecutionPin(*EntryNode, EGPD_Output);

	// Create a struct member write node to store the parameters into the animation node
	UK2Node_StructMemberSet* AssignmentNode = SpawnIntermediateNode<UK2Node_StructMemberSet>(VisualAnimNode, ConsolidatedEventGraph);
	AssignmentNode->VariableReference.SetSelfMember(Record.NodeVariableProperty->GetFName());
	AssignmentNode->StructType = Record.NodeVariableProperty->Struct;
	AssignmentNode->AllocateDefaultPins();

	// Wire up the variable node execution wires
	UEdGraphPin* ExecVariablesIn = Schema->FindExecutionPin(*AssignmentNode, EGPD_Input);
	ExecChain->MakeLinkTo(ExecVariablesIn);
	ExecChain = Schema->FindExecutionPin(*AssignmentNode, EGPD_Output);

	// Run thru each property
	TSet<FName> PropertiesBeingSet;

	for (auto TargetPinIt = AssignmentNode->Pins.CreateIterator(); TargetPinIt; ++TargetPinIt)
	{
		UEdGraphPin* TargetPin = *TargetPinIt;
		FString PropertyNameStr = TargetPin->PinName;
		FName PropertyName(*PropertyNameStr);

		// Does it get serviced by this handler?
		if (FAnimNodeSinglePropertyHandler* SourceInfo = Record.ServicedProperties.Find(PropertyName))
		{
			if (TargetPin->PinType.bIsArray)
			{
				// Grab the array that we need to set members for
				UK2Node_StructMemberGet* FetchArrayNode = SpawnIntermediateNode<UK2Node_StructMemberGet>(VisualAnimNode, ConsolidatedEventGraph);
				FetchArrayNode->VariableReference.SetSelfMember(Record.NodeVariableProperty->GetFName());
				FetchArrayNode->StructType = Record.NodeVariableProperty->Struct;
				FetchArrayNode->AllocatePinsForSingleMemberGet(PropertyName);

				UEdGraphPin* ArrayVariableNode = FetchArrayNode->FindPin(PropertyNameStr);

				if (SourceInfo->ArrayPins.Num() > 0)
				{
					// Set each element in the array
					for (auto SourcePinIt = SourceInfo->ArrayPins.CreateIterator(); SourcePinIt; ++SourcePinIt)
					{
						int32 ArrayIndex = SourcePinIt.Key();
						UEdGraphPin* SourcePin = SourcePinIt.Value();

						// Create an array element set node
						UK2Node_CallArrayFunction* ArrayNode = SpawnIntermediateNode<UK2Node_CallArrayFunction>(VisualAnimNode, ConsolidatedEventGraph);
						ArrayNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Set), UKismetArrayLibrary::StaticClass());
						ArrayNode->AllocateDefaultPins();

						// Connect the execution chain
						ExecChain->MakeLinkTo(ArrayNode->GetExecPin());
						ExecChain = ArrayNode->GetThenPin();

						// Connect the input array
						UEdGraphPin* TargetArrayPin = ArrayNode->FindPinChecked(TEXT("TargetArray"));
						TargetArrayPin->MakeLinkTo(ArrayVariableNode);
						ArrayNode->PinConnectionListChanged(TargetArrayPin);

						// Set the array index
						UEdGraphPin* TargetIndexPin = ArrayNode->FindPinChecked(TEXT("Index"));
						TargetIndexPin->DefaultValue = FString::FromInt(ArrayIndex);

						// Wire up the data input
						UEdGraphPin* TargetItemPin = ArrayNode->FindPinChecked(TEXT("Item"));
						TargetItemPin->CopyPersistentDataFromOldPin(*SourcePin);
						MessageLog.NotifyIntermediateObjectCreation(TargetItemPin, SourcePin);
						SourcePin->BreakAllPinLinks();
					}
				}
			}
			else
			{
				// Single property
				if (SourceInfo->SinglePin != NULL)
				{
					UEdGraphPin* SourcePin = SourceInfo->SinglePin;

					PropertiesBeingSet.Add(FName(*SourcePin->PinName));
					TargetPin->CopyPersistentDataFromOldPin(*SourcePin);
					MessageLog.NotifyIntermediateObjectCreation(TargetPin, SourcePin);
					SourcePin->BreakAllPinLinks();
				}
			}
		}
	}

	// Remove any unused pins from the assignment node to avoid smashing constant values
	for (int32 PinIndex = 0; PinIndex < AssignmentNode->ShowPinForProperties.Num(); ++PinIndex)
	{
		FOptionalPinFromProperty& TestProperty = AssignmentNode->ShowPinForProperties[PinIndex];
		TestProperty.bShowPin = PropertiesBeingSet.Contains(TestProperty.PropertyName);
	}
	AssignmentNode->ReconstructNode();
}

void FAnimBlueprintCompiler::ProcessAnimationNode(UAnimGraphNode_Base* VisualAnimNode)
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();

	// Early out if this node has already been processed
	if (AllocatedAnimNodes.Contains(VisualAnimNode))
	{
		return;
	}

	// Make sure the visual node has a runtime node template
	const UScriptStruct* NodeType = VisualAnimNode->GetFNodeType();
	if (NodeType == NULL)
	{
		MessageLog.Error(TEXT("@@ has no animation node member"), VisualAnimNode);
		return;
	}

	// Give the visual node a chance to do validation
	{
		const int32 PreValidationErrorCount = MessageLog.NumErrors;
		VisualAnimNode->ValidateAnimNodeDuringCompilation(AnimBlueprint->TargetSkeleton, MessageLog);
		VisualAnimNode->BakeDataDuringCompilation(MessageLog);
		if (MessageLog.NumErrors != PreValidationErrorCount)
		{
			return;
		}
	}

	// Create a property for the node
	const FString NodeVariableName = ClassScopeNetNameMap.MakeValidName(VisualAnimNode);

	FEdGraphPinType NodeVariableType;
	NodeVariableType.PinCategory = Schema->PC_Struct;
	NodeVariableType.PinSubCategoryObject = NodeType;

	UStructProperty* NewProperty = Cast<UStructProperty>(CreateVariable(FName(*NodeVariableName), NodeVariableType));

	if (NewProperty == NULL)
	{
		MessageLog.Error(TEXT("Failed to create node property for @@"), VisualAnimNode);
	}

	// Register this node with the compile-time data structures
	const int32 AllocatedIndex = AllocateNodeIndexCounter++;
	AllocatedAnimNodes.Add(VisualAnimNode, NewProperty);
	AllocatedNodePropertiesToNodes.Add(NewProperty, VisualAnimNode);
	AllocatedAnimNodeIndices.Add(VisualAnimNode, AllocatedIndex);
	AllocatedPropertiesByIndex.Add(AllocatedIndex, NewProperty);

	UAnimGraphNode_Base* TrueSourceObject = MessageLog.FindSourceObjectTypeChecked<UAnimGraphNode_Base>(VisualAnimNode);
	SourceNodeToProcessedNodeMap.Add(TrueSourceObject, VisualAnimNode);

	// Register the slightly more permanent debug information
	NewAnimBlueprintClass->GetAnimBlueprintDebugData().NodePropertyToIndexMap.Add(TrueSourceObject, AllocatedIndex);
	NewAnimBlueprintClass->GetAnimBlueprintDebugData().NodeGuidToIndexMap.Add(TrueSourceObject->NodeGuid, AllocatedIndex);
	NewAnimBlueprintClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourceObject, NewProperty);

	// Node-specific compilation that requires compiler state info
	if (UAnimGraphNode_StateMachineBase* StateMachineInstance = Cast<UAnimGraphNode_StateMachineBase>(VisualAnimNode))
	{
		// Compile the state machine
		ProcessStateMachine(StateMachineInstance);
	}
	else if (UAnimGraphNode_UseCachedPose* UseCachedPose = Cast<UAnimGraphNode_UseCachedPose>(VisualAnimNode))
	{
		// Handle a save/use cached pose linkage
		ProcessUseCachedPose(UseCachedPose);
	}

	// Record pose pins for later patchup and gather pins that have an associated evaluation handler
	TMap<FName, FEvaluationHandlerRecord> EvaluationHandlers;

	for (auto SourcePinIt = VisualAnimNode->Pins.CreateIterator(); SourcePinIt; ++SourcePinIt)
	{
		UEdGraphPin* SourcePin = *SourcePinIt;
		bool bConsumed = false;

		// Register pose links for future use
		if ((SourcePin->Direction == EGPD_Input) && (Schema->IsPosePin(SourcePin->PinType)))
		{
			// Input pose pin, going to need to be linked up
			FPoseLinkMappingRecord LinkRecord = VisualAnimNode->GetLinkIDLocation(NodeType, SourcePin);
			if (LinkRecord.IsValid())
			{
				ValidPoseLinkList.Add(LinkRecord);
				bConsumed = true;
			}
		}
		else
		{
			// Does this pin have an associated evaluation handler?
			UProperty* SourcePinProperty;
			int32 SourceArrayIndex;
			VisualAnimNode->GetPinAssociatedProperty(NodeType, SourcePin, /*out*/ SourcePinProperty, /*out*/ SourceArrayIndex);
			
			if (SourcePinProperty != NULL)
			{
				if (SourcePin->LinkedTo.Num() == 0)
				{
					// Literal that can be pushed into the CDO instead of re-evaluated every frame
					new (ValidAnimNodePinConstants) FEffectiveConstantRecord(NewProperty, SourcePin, SourcePinProperty, SourceArrayIndex);
					bConsumed = true;
				}
				else
				{
					// Dynamic value that needs to be wired up and evaluated each frame
					FString EvaluationHandlerStr = SourcePinProperty->GetMetaData(Schema->NAME_OnEvaluate);
					FName EvaluationHandlerName(*EvaluationHandlerStr);
					if (EvaluationHandlerName == NAME_None)
					{
						EvaluationHandlerName = Schema->DefaultEvaluationHandlerName;
					}

					EvaluationHandlers.FindOrAdd(EvaluationHandlerName).RegisterPin(SourcePin, SourcePinProperty, SourceArrayIndex);
					bConsumed = true;
				}
			}
		}

		if (!bConsumed && (SourcePin->Direction == EGPD_Input))
		{
			//@TODO: ANIMREFACTOR: It's probably OK to have certain pins ignored eventually, but this is very helpful during development
			MessageLog.Note(TEXT("@@ was visible but ignored"), SourcePin);
		}
	}

	// Match the associated property to each evaluation handler
	for (TFieldIterator<UProperty> NodePropIt(NodeType); NodePropIt; ++NodePropIt)
	{
		if (UStructProperty* StructProp = Cast<UStructProperty>(*NodePropIt))
		{
			if (StructProp->Struct == FExposedValueHandler::StaticStruct())
			{
				// Register this property to the list of pins that need to be updated
				// (it's OK if there isn't an entry for this handler, it means that the values are static and don't need to be calculated every frame)
				FName EvaluationHandlerName(StructProp->GetFName());
				if (FEvaluationHandlerRecord* pRecord = EvaluationHandlers.Find(EvaluationHandlerName))
				{
					pRecord->NodeVariableProperty = NewProperty;
					pRecord->EvaluationHandlerProperty = StructProp;
				}
			}
		}
	}

	// Generate a new event to update the value of these properties
	for (auto HandlerIt = EvaluationHandlers.CreateIterator(); HandlerIt; ++HandlerIt)
	{
		FName EvaluationHandlerName = HandlerIt.Key();
		FEvaluationHandlerRecord& Record = HandlerIt.Value();

		if (Record.IsValid())
		{
			CreateEvaluationHandler(VisualAnimNode, Record);
			ValidEvaluationHandlerList.Add(Record);
		}
		else
		{
			MessageLog.Error(*FString::Printf(TEXT("A property on @@ references a non-existent %s property named %s"), VisualAnimNode), *(Schema->NAME_OnEvaluate.ToString()), *(EvaluationHandlerName.ToString()));
		}
	}
}

void FAnimBlueprintCompiler::ProcessUseCachedPose(UAnimGraphNode_UseCachedPose* UseCachedPose)
{
	bool bSuccessful = false;

	// if compiling only skeleton, we don't have to worry about linking save node
	if (CompileOptions.CompileType == EKismetCompileType::SkeletonOnly)
	{
		return;
	}

	// Link to the saved cached pose
	if(UseCachedPose->SaveCachedPoseNode.IsValid())
	{
		if (UAnimGraphNode_SaveCachedPose* AssociatedSaveNode = SaveCachedPoseNodes.FindRef(UseCachedPose->SaveCachedPoseNode->CacheName))
		{
			UStructProperty* LinkProperty = FindField<UStructProperty>(FAnimNode_UseCachedPose::StaticStruct(), TEXT("LinkToCachingNode"));
			check(LinkProperty);

			FPoseLinkMappingRecord LinkRecord = FPoseLinkMappingRecord::MakeFromMember(UseCachedPose, AssociatedSaveNode, LinkProperty);
			if (LinkRecord.IsValid())
			{
				ValidPoseLinkList.Add(LinkRecord);
			}
			bSuccessful = true;
		}
	}
	
	if(!bSuccessful)
	{
		MessageLog.Error(*LOCTEXT("NoAssociatedSaveNode", "@@ does not have an associated Save Cached Pose node").ToString(), UseCachedPose);
	}
}

int32 FAnimBlueprintCompiler::GetAllocationIndexOfNode(UAnimGraphNode_Base* VisualAnimNode)
{
	ProcessAnimationNode(VisualAnimNode);
	int32* pResult = AllocatedAnimNodeIndices.Find(VisualAnimNode);
	return (pResult != NULL) ? *pResult : INDEX_NONE;
}

void FAnimBlueprintCompiler::PruneIsolatedAnimationNodes(const TArray<UAnimGraphNode_Base*>& RootSet, TArray<UAnimGraphNode_Base*>& GraphNodes)
{
	struct FNodeVisitorDownPoseWires
	{
		TSet<UEdGraphNode*> VisitedNodes;
		const UAnimationGraphSchema* Schema;

		FNodeVisitorDownPoseWires()
		{
			Schema = GetDefault<UAnimationGraphSchema>();
		}

		void TraverseNodes(UEdGraphNode* Node)
		{
			VisitedNodes.Add(Node);

			// Follow every exec output pin
			for (int32 i = 0; i < Node->Pins.Num(); ++i)
			{
				UEdGraphPin* MyPin = Node->Pins[i];

				if ((MyPin->Direction == EGPD_Input) && (Schema->IsPosePin(MyPin->PinType)))
				{
					for (int32 j = 0; j < MyPin->LinkedTo.Num(); ++j)
					{
						UEdGraphPin* OtherPin = MyPin->LinkedTo[j];
						UEdGraphNode* OtherNode = OtherPin->GetOwningNode();
						if (!VisitedNodes.Contains(OtherNode))
						{
							TraverseNodes(OtherNode);
						}
					}
				}
			}
		}
	};

	// Prune the nodes that aren't reachable via an animation pose link
	FNodeVisitorDownPoseWires Visitor;

	for (auto RootIt = RootSet.CreateConstIterator(); RootIt; ++RootIt)
	{
		UAnimGraphNode_Base* RootNode = *RootIt;
		Visitor.TraverseNodes(RootNode);
	}

	for (int32 NodeIndex = 0; NodeIndex < GraphNodes.Num(); ++NodeIndex)
	{
		UAnimGraphNode_Base* Node = GraphNodes[NodeIndex];
		if (!Visitor.VisitedNodes.Contains(Node) && !IsNodePure(Node))
		{
			Node->BreakAllNodeLinks();
			GraphNodes.RemoveAtSwap(NodeIndex);
			--NodeIndex;
		}
	}
}

void FAnimBlueprintCompiler::ProcessAnimationNodesGivenRoot(TArray<UAnimGraphNode_Base*>& AnimNodeList, const TArray<UAnimGraphNode_Base*>& RootSet)
{
	// Now prune based on the root set
	PruneIsolatedAnimationNodes(RootSet, AnimNodeList);

	// Process the remaining nodes
	for (auto SourceNodeIt = AnimNodeList.CreateIterator(); SourceNodeIt; ++SourceNodeIt)
	{
		UAnimGraphNode_Base* VisualAnimNode = *SourceNodeIt;
		ProcessAnimationNode(VisualAnimNode);
	}
}

void FAnimBlueprintCompiler::ProcessAllAnimationNodes()
{
	// Build the raw node list
	TArray<UAnimGraphNode_Base*> AnimNodeList;
	ConsolidatedEventGraph->GetNodesOfClass<UAnimGraphNode_Base>(/*out*/ AnimNodeList);

	TArray<UK2Node_TransitionRuleGetter*> Getters;
	ConsolidatedEventGraph->GetNodesOfClass<UK2Node_TransitionRuleGetter>(/*out*/ Getters);

	// Find the root node
	UAnimGraphNode_Root* PrePhysicsRoot = NULL;
	TArray<UAnimGraphNode_Base*> RootSet;

	AllocateNodeIndexCounter = 0;
	NewAnimBlueprintClass->RootAnimNodeIndex = 0;//INDEX_NONE;

	for (auto SourceNodeIt = AnimNodeList.CreateIterator(); SourceNodeIt; ++SourceNodeIt)
	{
		UAnimGraphNode_Base* SourceNode = *SourceNodeIt;
		if (UAnimGraphNode_Root* PossibleRoot = Cast<UAnimGraphNode_Root>(SourceNode))
		{
			if (UAnimGraphNode_Root* Root = ExactCast<UAnimGraphNode_Root>(PossibleRoot))
			{
				if (PrePhysicsRoot != NULL)
				{
					MessageLog.Error(*FString::Printf(*LOCTEXT("ExpectedOneFunctionEntry_Error", "Expected only one animation root, but found both @@ and @@").ToString()),
						PrePhysicsRoot, Root);
				}
				else
				{
					RootSet.Add(Root);
					PrePhysicsRoot = Root;
				}
			}
		}
		else if (UAnimGraphNode_SaveCachedPose* SavePoseRoot = Cast<UAnimGraphNode_SaveCachedPose>(SourceNode))
		{
			//@TODO: Ideally we only add these if there is a UseCachedPose node referencing them, but those can be anywhere and are hard to grab
			SaveCachedPoseNodes.Add(SavePoseRoot->CacheName, SavePoseRoot);
			RootSet.Add(SavePoseRoot);
		}
	}

	if (PrePhysicsRoot != NULL)
	{
		// Process the animation nodes
		ProcessAnimationNodesGivenRoot(AnimNodeList, RootSet);

		// Process the getter nodes in the graph if there were any
		for (auto GetterIt = Getters.CreateIterator(); GetterIt; ++GetterIt)
		{
			ProcessTransitionGetter(*GetterIt, NULL); // transition nodes should not appear at top-level
		}

		NewAnimBlueprintClass->RootAnimNodeIndex = GetAllocationIndexOfNode(PrePhysicsRoot);
	}
	else
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("ExpectedAFunctionEntry_Error", "Expected an animation root, but did not find one").ToString()));
	}
}

int32 FAnimBlueprintCompiler::ExpandGraphAndProcessNodes(UEdGraph* SourceGraph, UAnimGraphNode_Base* SourceRootNode, UAnimStateTransitionNode* TransitionNode, TArray<UEdGraphNode*>* ClonedNodes)
{
	// Clone the nodes from the source graph
	UEdGraph* ClonedGraph = FEdGraphUtilities::CloneGraph(SourceGraph, NULL, &MessageLog, true);

	// Grab all the animation nodes and find the corresponding root node in the cloned set
	UAnimGraphNode_Base* TargetRootNode = NULL;
	TArray<UAnimGraphNode_Base*> AnimNodeList;
	TArray<UK2Node_TransitionRuleGetter*> Getters;
	for (auto NodeIt = ClonedGraph->Nodes.CreateIterator(); NodeIt; ++NodeIt)
	{
		UEdGraphNode* Node = *NodeIt;

		if (UK2Node_TransitionRuleGetter* GetterNode = Cast<UK2Node_TransitionRuleGetter>(Node))
		{
			Getters.Add(GetterNode);
		}
		else if (UAnimGraphNode_Base* TestNode = Cast<UAnimGraphNode_Base>(Node))
		{
			AnimNodeList.Add(TestNode);

			//@TODO: There ought to be a better way to determine this
			if (MessageLog.FindSourceObject(TestNode) == MessageLog.FindSourceObject(SourceRootNode))
			{
				TargetRootNode = TestNode;
			}
		}

		if (ClonedNodes != NULL)
		{
			ClonedNodes->Add(Node);
		}
	}
	check(TargetRootNode);

	// Move the cloned nodes into the consolidated event graph
	const bool bIsLoading = Blueprint->bIsRegeneratingOnLoad || IsAsyncLoading();
	ClonedGraph->MoveNodesToAnotherGraph(ConsolidatedEventGraph, bIsLoading);

	// Process any animation nodes
	{
		TArray<UAnimGraphNode_Base*> RootSet;
		RootSet.Add(TargetRootNode);
		ProcessAnimationNodesGivenRoot(AnimNodeList, RootSet);
	}

	// Process the getter nodes in the graph if there were any
	for (auto GetterIt = Getters.CreateIterator(); GetterIt; ++GetterIt)
	{
		ProcessTransitionGetter(*GetterIt, TransitionNode);
	}

	// Returns the index of the processed cloned version of SourceRootNode
	return GetAllocationIndexOfNode(TargetRootNode);	
}

void FAnimBlueprintCompiler::ProcessStateMachine(UAnimGraphNode_StateMachineBase* StateMachineInstance)
{
	struct FMachineCreator
	{
	public:
		int32 MachineIndex;
		TMap<UAnimStateNodeBase*, int32> StateIndexTable;
		TMap<UAnimStateTransitionNode*, int32> TransitionIndexTable;
		UAnimBlueprintGeneratedClass* AnimBlueprintClass;
		UAnimGraphNode_StateMachineBase* StateMachineInstance;
		FCompilerResultsLog& MessageLog;
	public:
		FMachineCreator(FCompilerResultsLog& InMessageLog, UAnimGraphNode_StateMachineBase* InStateMachineInstance, int32 InMachineIndex, UAnimBlueprintGeneratedClass* InNewClass)
			: MachineIndex(InMachineIndex)
			, AnimBlueprintClass(InNewClass)
			, StateMachineInstance(InStateMachineInstance)
			, MessageLog(InMessageLog)
		{
			FStateMachineDebugData& MachineInfo = GetMachineSpecificDebugData();
			MachineInfo.MachineIndex = MachineIndex;
			MachineInfo.MachineInstanceNode = MessageLog.FindSourceObjectTypeChecked<UAnimGraphNode_StateMachineBase>(InStateMachineInstance);

			StateMachineInstance->GetNode().StateMachineIndexInClass = MachineIndex;

			FBakedAnimationStateMachine& BakedMachine = GetMachine();
			BakedMachine.MachineName = StateMachineInstance->EditorStateMachineGraph->GetFName();
			BakedMachine.InitialState = INDEX_NONE;
		}

		FBakedAnimationStateMachine& GetMachine()
		{
			return AnimBlueprintClass->BakedStateMachines[MachineIndex];
		}

		FStateMachineDebugData& GetMachineSpecificDebugData()
		{
			UAnimationStateMachineGraph* SourceGraph = MessageLog.FindSourceObjectTypeChecked<UAnimationStateMachineGraph>(StateMachineInstance->EditorStateMachineGraph);
			return AnimBlueprintClass->GetAnimBlueprintDebugData().StateMachineDebugData.FindOrAdd(SourceGraph);
		}

		int32 FindOrAddState(UAnimStateNodeBase* StateNode)
		{
			if (int32* pResult = StateIndexTable.Find(StateNode))
			{
				return *pResult;
			}
			else
			{
				FBakedAnimationStateMachine& BakedMachine = GetMachine();

				const int32 StateIndex = BakedMachine.States.Num();
				StateIndexTable.Add(StateNode, StateIndex);
				new (BakedMachine.States) FBakedAnimationState();

				UAnimStateNodeBase* SourceNode = MessageLog.FindSourceObjectTypeChecked<UAnimStateNodeBase>(StateNode);
				GetMachineSpecificDebugData().NodeToStateIndex.Add(SourceNode, StateIndex);
				if (UAnimStateNode* SourceStateNode = Cast<UAnimStateNode>(SourceNode))
				{
					AnimBlueprintClass->GetAnimBlueprintDebugData().StateGraphToNodeMap.Add(SourceStateNode->BoundGraph, SourceStateNode);
				}

				return StateIndex;
			}
		}

		int32 FindOrAddTransition(UAnimStateTransitionNode* TransitionNode)
		{
			if (int32* pResult = TransitionIndexTable.Find(TransitionNode))
			{
				return *pResult;
			}
			else
			{
				FBakedAnimationStateMachine& BakedMachine = GetMachine();

				const int32 TransitionIndex = BakedMachine.Transitions.Num();
				TransitionIndexTable.Add(TransitionNode, TransitionIndex);
				new (BakedMachine.Transitions) FAnimationTransitionBetweenStates();

				UAnimStateTransitionNode* SourceTransitionNode = MessageLog.FindSourceObjectTypeChecked<UAnimStateTransitionNode>(TransitionNode);
				GetMachineSpecificDebugData().NodeToTransitionIndex.Add(SourceTransitionNode, TransitionIndex);
				AnimBlueprintClass->GetAnimBlueprintDebugData().TransitionGraphToNodeMap.Add(SourceTransitionNode->BoundGraph, SourceTransitionNode);

				if (SourceTransitionNode->CustomTransitionGraph != NULL)
				{
					AnimBlueprintClass->GetAnimBlueprintDebugData().TransitionBlendGraphToNodeMap.Add(SourceTransitionNode->CustomTransitionGraph, SourceTransitionNode);
				}

				return TransitionIndex;
			}
		}

		void Validate()
		{
			FBakedAnimationStateMachine& BakedMachine = GetMachine();

			// Make sure there is a valid entry point
			if (BakedMachine.InitialState == INDEX_NONE)
			{
				MessageLog.Warning(*LOCTEXT("NoEntryNode", "There was no entry state connection in @@").ToString(), StateMachineInstance);
				BakedMachine.InitialState = 0;
			}
			else
			{
				// Make sure the entry node is a state and not a conduit
				if (BakedMachine.States[BakedMachine.InitialState].bIsAConduit)
				{
					UEdGraphNode* StateNode = GetMachineSpecificDebugData().FindNodeFromStateIndex(BakedMachine.InitialState);
					MessageLog.Error(*LOCTEXT("BadStateEntryNode", "A conduit (@@) cannot be used as the entry node for a state machine").ToString(), StateNode);
				}
			}
		}
	};
	
	if (StateMachineInstance->EditorStateMachineGraph == NULL)
	{
		MessageLog.Error(*LOCTEXT("BadStateMachineNoGraph", "@@ does not have a corresponding graph").ToString(), StateMachineInstance);
		return;
	}

	TMap<UAnimGraphNode_TransitionResult*, int32> AlreadyMergedTransitionList;

	const int32 MachineIndex = NewAnimBlueprintClass->BakedStateMachines.Num();
	new (NewAnimBlueprintClass->BakedStateMachines) FBakedAnimationStateMachine();
	FMachineCreator Oven(MessageLog, StateMachineInstance, MachineIndex, NewAnimBlueprintClass);

	// Map of states that contain a single player node (from state root node index to associated sequence player)
	TMap<int32, UObject*> SimplePlayerStatesMap;

	// Process all the states/transitions
	for (auto StateNodeIt = StateMachineInstance->EditorStateMachineGraph->Nodes.CreateIterator(); StateNodeIt; ++StateNodeIt)
	{
		UEdGraphNode* Node = *StateNodeIt;

		if (UAnimStateEntryNode* EntryNode = Cast<UAnimStateEntryNode>(Node))
		{
			// Handle the state graph entry
			FBakedAnimationStateMachine& BakedMachine = Oven.GetMachine();
			if (BakedMachine.InitialState != INDEX_NONE)
			{
				MessageLog.Error(*LOCTEXT("TooManyStateMachineEntryNodes", "Found an extra entry node @@").ToString(), EntryNode);
			}
			else if (UAnimStateNodeBase* StartState = Cast<UAnimStateNodeBase>(EntryNode->GetOutputNode()))
			{
				BakedMachine.InitialState = Oven.FindOrAddState(StartState);
			}
			else
			{
				MessageLog.Warning(*LOCTEXT("NoConnection", "Entry node @@ is not connected to state").ToString(), EntryNode);
			}
		}
		else if (UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(Node))
		{
			TransitionNode->ValidateNodeDuringCompilation(MessageLog);

			const int32 TransitionIndex = Oven.FindOrAddTransition(TransitionNode);
			FAnimationTransitionBetweenStates& BakedTransition = Oven.GetMachine().Transitions[TransitionIndex];

			BakedTransition.CrossfadeDuration = TransitionNode->CrossfadeDuration;
			BakedTransition.StartNotify = FindOrAddNotify(TransitionNode->TransitionStart);
			BakedTransition.EndNotify = FindOrAddNotify(TransitionNode->TransitionEnd);
			BakedTransition.InterruptNotify = FindOrAddNotify(TransitionNode->TransitionInterrupt);
			BakedTransition.CrossfadeMode = TransitionNode->CrossfadeMode;
			BakedTransition.LogicType = TransitionNode->LogicType;

			UAnimStateNodeBase* PreviousState = TransitionNode->GetPreviousState();
			UAnimStateNodeBase* NextState = TransitionNode->GetNextState();

			if ((PreviousState != NULL) && (NextState != NULL))
			{
				const int32 PreviousStateIndex = Oven.FindOrAddState(PreviousState);
				const int32 NextStateIndex = Oven.FindOrAddState(NextState);

				if (TransitionNode->Bidirectional)
				{
					MessageLog.Warning(*LOCTEXT("BidirectionalTransWarning", "Bidirectional transitions aren't supported yet @@").ToString(), TransitionNode);
				}

				BakedTransition.PreviousState = PreviousStateIndex;
				BakedTransition.NextState = NextStateIndex;
			}
			else
			{
				MessageLog.Warning(*LOCTEXT("BogusTransition", "@@ is incomplete, without a previous and next state").ToString(), TransitionNode);
				BakedTransition.PreviousState = INDEX_NONE;
				BakedTransition.NextState = INDEX_NONE;
			}
		}
		else if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			StateNode->ValidateNodeDuringCompilation(MessageLog);

			const int32 StateIndex = Oven.FindOrAddState(StateNode);
			FBakedAnimationState& BakedState = Oven.GetMachine().States[StateIndex];

			if (StateNode->BoundGraph != NULL)
			{
				BakedState.StateName = StateNode->BoundGraph->GetFName();
				BakedState.StartNotify = FindOrAddNotify(StateNode->StateEntered);
				BakedState.EndNotify = FindOrAddNotify(StateNode->StateLeft);
				BakedState.FullyBlendedNotify = FindOrAddNotify(StateNode->StateFullyBlended);
				BakedState.bIsAConduit = false;

				// Process the inner graph of this state
				if (UAnimGraphNode_StateResult* AnimGraphResultNode = CastChecked<UAnimationStateGraph>(StateNode->BoundGraph)->GetResultNode())
				{
					BakedState.StateRootNodeIndex = ExpandGraphAndProcessNodes(StateNode->BoundGraph, AnimGraphResultNode);

					// See if the state consists of a single sequence player node, and remember the index if so
					for (UEdGraphPin* TestPin : AnimGraphResultNode->Pins)
					{
						if ((TestPin->Direction == EGPD_Input) && (TestPin->LinkedTo.Num() == 1))
						{
							if (UAnimGraphNode_SequencePlayer* SequencePlayer = Cast<UAnimGraphNode_SequencePlayer>(TestPin->LinkedTo[0]->GetOwningNode()))
							{
								SimplePlayerStatesMap.Add(BakedState.StateRootNodeIndex, MessageLog.FindSourceObject(SequencePlayer));
							}
						}
					}
				}
				else
				{
					BakedState.StateRootNodeIndex = INDEX_NONE;
					MessageLog.Error(*LOCTEXT("StateWithNoResult", "@@ has no result node").ToString(), StateNode);
				}
			}
			else
			{
				BakedState.StateName = NAME_None;
				MessageLog.Error(*LOCTEXT("StateWithBadGraph", "@@ has no bound graph").ToString(), StateNode);
			}

			// If this check fires, then something in the machine has changed causing the states array to not
			// be a separate allocation, and a state machine inside of this one caused stuff to shift around
			checkSlow(&BakedState == &(Oven.GetMachine().States[StateIndex]));
		}
		else if (UAnimStateConduitNode* ConduitNode = Cast<UAnimStateConduitNode>(Node))
		{
			ConduitNode->ValidateNodeDuringCompilation(MessageLog);

			const int32 StateIndex = Oven.FindOrAddState(ConduitNode);
			FBakedAnimationState& BakedState = Oven.GetMachine().States[StateIndex];

			BakedState.StateName = ConduitNode->BoundGraph ? ConduitNode->BoundGraph->GetFName() : TEXT("OLD CONDUIT");
			BakedState.bIsAConduit = true;
			
			if (ConduitNode->BoundGraph != NULL)
			{
				if (UAnimGraphNode_TransitionResult* EntryRuleResultNode = CastChecked<UAnimationTransitionGraph>(ConduitNode->BoundGraph)->GetResultNode())
				{
					BakedState.EntryRuleNodeIndex = ExpandGraphAndProcessNodes(ConduitNode->BoundGraph, EntryRuleResultNode);
				}
			}

			// If this check fires, then something in the machine has changed causing the states array to not
			// be a separate allocation, and a state machine inside of this one caused stuff to shift around
			checkSlow(&BakedState == &(Oven.GetMachine().States[StateIndex]));
		}
	}

	// Process transitions after all the states because getters within custom graphs may want to
	// reference back to other states, which are only valid if they have already been baked
	for (auto StateNodeIt = Oven.StateIndexTable.CreateIterator(); StateNodeIt; ++StateNodeIt)
	{
		UAnimStateNodeBase* StateNode = StateNodeIt.Key();
		const int32 StateIndex = StateNodeIt.Value();

		FBakedAnimationState& BakedState = Oven.GetMachine().States[StateIndex];

		// Handle all the transitions out of this node
		TArray<class UAnimStateTransitionNode*> TransitionList;
		StateNode->GetTransitionList(/*out*/ TransitionList, /*bWantSortedList=*/ true);

		for (auto TransitionIt = TransitionList.CreateIterator(); TransitionIt; ++TransitionIt)
		{
			UAnimStateTransitionNode* TransitionNode = *TransitionIt;
			const int32 TransitionIndex = Oven.FindOrAddTransition(TransitionNode);

			FBakedStateExitTransition& Rule = *new (BakedState.Transitions) FBakedStateExitTransition();
			Rule.bDesiredTransitionReturnValue = (TransitionNode->GetPreviousState() == StateNode);
			Rule.TransitionIndex = TransitionIndex;
			
			if (UAnimGraphNode_TransitionResult* TransitionResultNode = CastChecked<UAnimationTransitionGraph>(TransitionNode->BoundGraph)->GetResultNode())
			{
				if (int32* pIndex = AlreadyMergedTransitionList.Find(TransitionResultNode))
				{
					Rule.CanTakeDelegateIndex = *pIndex;
				}
				else
				{
					Rule.CanTakeDelegateIndex = ExpandGraphAndProcessNodes(TransitionNode->BoundGraph, TransitionResultNode, TransitionNode);
					AlreadyMergedTransitionList.Add(TransitionResultNode, Rule.CanTakeDelegateIndex);
				}
			}
			else
			{
				Rule.CanTakeDelegateIndex = INDEX_NONE;
				MessageLog.Error(*LOCTEXT("TransitionWithNoResult", "@@ has no result node").ToString(), TransitionNode);
			}

			// Handle automatic time remaining rules
			Rule.StateSequencePlayerToQueryIndex = INDEX_NONE;
			if (TransitionNode->bAutomaticRuleBasedOnSequencePlayerInState)
			{
				if (UObject* SourceSequencePlayer = SimplePlayerStatesMap.FindRef(BakedState.StateRootNodeIndex))
				{
					//@TODO: This should be easier than it is... (the nodes are cloned so the pointers don't natrually match)
					for (const auto& Pair : AllocatedAnimNodeIndices)
					{
						if (MessageLog.FindSourceObject(Pair.Key) == SourceSequencePlayer)
						{
							Rule.StateSequencePlayerToQueryIndex = Pair.Value;
						}
					}
				}

				if (Rule.StateSequencePlayerToQueryIndex == INDEX_NONE)
				{
					MessageLog.Error(*LOCTEXT("CantAutomaticallyDefineTransitionRule", "@@ must contain a single sequence player in order to use bAutomaticRuleBasedOnSequencePlayerInState on @@").ToString(), StateNode, TransitionNode);
				}
			}

			// Handle custom transition graphs
			Rule.CustomResultNodeIndex = INDEX_NONE;
			if (UAnimationCustomTransitionGraph* CustomTransitionGraph = Cast<UAnimationCustomTransitionGraph>(TransitionNode->CustomTransitionGraph))
			{
				TArray<UEdGraphNode*> ClonedNodes;
				if (CustomTransitionGraph->GetResultNode())
				{
					Rule.CustomResultNodeIndex = ExpandGraphAndProcessNodes(TransitionNode->CustomTransitionGraph, CustomTransitionGraph->GetResultNode(), NULL, &ClonedNodes);
				}

				// Find all the pose evaluators used in this transition, save handles to them because we need to populate some pose data before executing
				TArray<UAnimGraphNode_TransitionPoseEvaluator*> TransitionPoseList;
				for (auto ClonedNodesIt = ClonedNodes.CreateIterator(); ClonedNodesIt; ++ClonedNodesIt)
				{
					UEdGraphNode* Node = *ClonedNodesIt;
					if (UAnimGraphNode_TransitionPoseEvaluator* TypedNode = Cast<UAnimGraphNode_TransitionPoseEvaluator>(Node))
					{
						TransitionPoseList.Add(TypedNode);
					}
				}

				Rule.PoseEvaluatorLinks.Empty(TransitionPoseList.Num());

				for (auto TransitionPoseListIt = TransitionPoseList.CreateIterator(); TransitionPoseListIt; ++TransitionPoseListIt)
				{
					UAnimGraphNode_TransitionPoseEvaluator* TransitionPoseNode = *TransitionPoseListIt;
					Rule.PoseEvaluatorLinks.Add( GetAllocationIndexOfNode(TransitionPoseNode) );
				}
			}
		}
	}

	Oven.Validate();
}

void FAnimBlueprintCompiler::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	Super::CopyTermDefaultsToDefaultObject(DefaultObject);

	if (bIsDerivedAnimBlueprint)
	{
		// If we are a derived animation graph; apply any stored overrides.
		// Restore values from the root blueprint to catch values where the override has been removed.
		UAnimBlueprint* RootAnimBP = UAnimBlueprint::FindRootAnimBlueprint(AnimBlueprint);
		UAnimBlueprintGeneratedClass* RootAnimClass = RootAnimBP->GetAnimBlueprintGeneratedClass();
		UObject* RootDefaultObject = RootAnimClass->GetDefaultObject();

		for (TFieldIterator<UProperty> It(RootAnimClass) ; It; ++It)
		{
			UProperty* RootProp = *It;

			if (UStructProperty* RootStructProp = Cast<UStructProperty>(RootProp))
			{
				if (RootStructProp->Struct->IsChildOf(FAnimNode_Base::StaticStruct()))
				{
					UStructProperty* ChildStructProp = FindField<UStructProperty>(NewAnimBlueprintClass, *RootStructProp->GetName());
					check(ChildStructProp);
					uint8* SourcePtr = RootStructProp->ContainerPtrToValuePtr<uint8>(RootDefaultObject);
					uint8* DestPtr = ChildStructProp->ContainerPtrToValuePtr<uint8>(DefaultObject);
					check(SourcePtr && DestPtr);
					RootStructProp->CopyCompleteValue(DestPtr, SourcePtr);
				}
			}
		}

		// Patch the overridden values into the CDO

		TArray<FAnimParentNodeAssetOverride*> AssetOverrides;
		AnimBlueprint->GetAssetOverrides(AssetOverrides);
		for (FAnimParentNodeAssetOverride* Override : AssetOverrides)
		{
			if (Override->NewAsset)
			{
				FAnimNode_Base* BaseNode = NewAnimBlueprintClass->GetPropertyInstance<FAnimNode_Base>(DefaultObject, Override->ParentNodeGuid, EPropertySearchMode::Hierarchy);
				if (BaseNode)
				{
					BaseNode->OverrideAsset(Override->NewAsset);
				}
			}
		}

		return;
	}

	int32 LinkIndexCount = 0;
	TMap<UAnimGraphNode_Base*, int32> LinkIndexMap;
	TMap<UAnimGraphNode_Base*, uint8*> NodeBaseAddresses;

	// Initialize animation nodes from their templates
	for (TFieldIterator<UProperty> It(DefaultObject->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UProperty* TargetProperty = *It;

		if (UAnimGraphNode_Base* VisualAnimNode = AllocatedNodePropertiesToNodes.FindRef(TargetProperty))
		{
			const UStructProperty* SourceNodeProperty = VisualAnimNode->GetFNodeProperty();
			check(SourceNodeProperty != NULL);
			check(CastChecked<UStructProperty>(TargetProperty)->Struct == SourceNodeProperty->Struct);

			uint8* DestinationPtr = TargetProperty->ContainerPtrToValuePtr<uint8>(DefaultObject);
			uint8* SourcePtr = SourceNodeProperty->ContainerPtrToValuePtr<uint8>(VisualAnimNode);
			TargetProperty->CopyCompleteValue(DestinationPtr, SourcePtr);

			LinkIndexMap.Add(VisualAnimNode, LinkIndexCount);
			NodeBaseAddresses.Add(VisualAnimNode, DestinationPtr);
			++LinkIndexCount;
		}
	}

	// And wire up node links
	for (auto PoseLinkIt = ValidPoseLinkList.CreateIterator(); PoseLinkIt; ++PoseLinkIt)
	{
		FPoseLinkMappingRecord& Record = *PoseLinkIt;

		UAnimGraphNode_Base* LinkingNode = Record.GetLinkingNode();
		UAnimGraphNode_Base* LinkedNode = Record.GetLinkedNode();
		
		// @TODO this is quick solution for crash - if there were previous errors and some nodes were not added, they could still end here -
		// this check avoids that and since there are already errors, compilation won't be successful.
		// but I'd prefer stoping compilation earlier to avoid getting here in first place
		if (LinkIndexMap.Contains(LinkingNode) && LinkIndexMap.Contains(LinkedNode))
		{
			const int32 SourceNodeIndex = LinkIndexMap.FindChecked(LinkingNode);
			const int32 LinkedNodeIndex = LinkIndexMap.FindChecked(LinkedNode);
			uint8* DestinationPtr = NodeBaseAddresses.FindChecked(LinkingNode);

			Record.PatchLinkIndex(DestinationPtr, LinkedNodeIndex, SourceNodeIndex);
		}
	}   

	// And patch evaluation function entry names
	for (auto EvalLinkIt = ValidEvaluationHandlerList.CreateIterator(); EvalLinkIt; ++EvalLinkIt)
	{
		FEvaluationHandlerRecord& Record = *EvalLinkIt;
		Record.PatchFunctionNameInto(DefaultObject);
	}

	// And patch in constant values that don't need to be re-evaluated every frame
	for (auto LiteralLinkIt = ValidAnimNodePinConstants.CreateIterator(); LiteralLinkIt; ++LiteralLinkIt)
	{
		FEffectiveConstantRecord& ConstantRecord = *LiteralLinkIt;

		//const FString ArrayClause = (ConstantRecord.ArrayIndex != INDEX_NONE) ? FString::Printf(TEXT("[%d]"), ConstantRecord.ArrayIndex) : FString();
		//const FString ValueClause = ConstantRecord.LiteralSourcePin->GetDefaultAsString();
		//MessageLog.Note(*FString::Printf(TEXT("Want to set %s.%s%s to %s"), *ConstantRecord.NodeVariableProperty->GetName(), *ConstantRecord.ConstantProperty->GetName(), *ArrayClause, *ValueClause));

		if (!ConstantRecord.Apply(DefaultObject))
		{
			MessageLog.Error(TEXT("ICE: Failed to push literal value from @@ into CDO"), ConstantRecord.LiteralSourcePin);
		}
	}
}

// Merges in any all ubergraph pages into the gathering ubergraph
void FAnimBlueprintCompiler::MergeUbergraphPagesIn(UEdGraph* Ubergraph)
{
	Super::MergeUbergraphPagesIn(Ubergraph);

	if (bIsDerivedAnimBlueprint)
	{
		// Skip any work related to an anim graph, it's all done by the parent class
	}
	else
	{
		// Move all animation graph nodes and associated pure logic chains into the consolidated event graph
		for (int32 i = 0; i < Blueprint->FunctionGraphs.Num(); ++i)
		{
			UEdGraph* SourceGraph = Blueprint->FunctionGraphs[i];

			if (SourceGraph->Schema->IsChildOf(UAnimationGraphSchema::StaticClass()))
			{
				// Merge all the animation nodes, contents, etc... into the ubergraph
				UEdGraph* ClonedGraph = FEdGraphUtilities::CloneGraph(SourceGraph, NULL, &MessageLog, true);
				const bool bIsLoading = Blueprint->bIsRegeneratingOnLoad || IsAsyncLoading();
				ClonedGraph->MoveNodesToAnotherGraph(ConsolidatedEventGraph, bIsLoading);
			}
		}

		// Compile the animation graph
		ProcessAllAnimationNodes();
	}
}

void FAnimBlueprintCompiler::ProcessOneFunctionGraph(UEdGraph* SourceGraph, bool bInternalFunction)
{
	if (SourceGraph->Schema->IsChildOf(UAnimationGraphSchema::StaticClass()))
	{
		// Animation graph
		// Do nothing, as this graph has already been processed
	}
	else if (SourceGraph->Schema->IsChildOf(UAnimationStateMachineSchema::StaticClass()))
	{
		// Animation state machine
		// Do nothing, as this graph has already been processed

		//@TODO: These should all have been moved to be child graphs by now
		//ensure(false);
	}
	else
	{
		// Let the regular K2 compiler handle this one
		Super::ProcessOneFunctionGraph(SourceGraph, bInternalFunction);
	}
}

void FAnimBlueprintCompiler::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if( TargetUClass && !((UObject*)TargetUClass)->IsA(UAnimBlueprintGeneratedClass::StaticClass()) )
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = NULL;
	}
}

void FAnimBlueprintCompiler::SpawnNewClass(const FString& NewClassName)
{
	NewAnimBlueprintClass = FindObject<UAnimBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if (NewAnimBlueprintClass == NULL)
	{
		NewAnimBlueprintClass = NewObject<UAnimBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer::Create(NewAnimBlueprintClass);
	}
	NewClass = NewAnimBlueprintClass;
}

void FAnimBlueprintCompiler::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO)
{
	Super::CleanAndSanitizeClass(ClassToClean, OldCDO);

	// Make sure our typed pointer is set
	check(ClassToClean == NewClass);
	NewAnimBlueprintClass = CastChecked<UAnimBlueprintGeneratedClass>((UObject*)NewClass);

	NewAnimBlueprintClass->AnimBlueprintDebugData = FAnimBlueprintDebugData();

	// Reset the baked data
	//@TODO: Move this into PurgeClass
	NewAnimBlueprintClass->BakedStateMachines.Empty();
	NewAnimBlueprintClass->AnimNotifies.Empty();

	NewAnimBlueprintClass->RootAnimNodeIndex = INDEX_NONE;
	NewAnimBlueprintClass->RootAnimNodeProperty = NULL;
	NewAnimBlueprintClass->AnimNodeProperties.Empty();

	// Copy over runtime data from the blueprint to the class
	NewAnimBlueprintClass->TargetSkeleton = AnimBlueprint->TargetSkeleton;

	UAnimBlueprint* RootAnimBP = UAnimBlueprint::FindRootAnimBlueprint(AnimBlueprint);
	bIsDerivedAnimBlueprint = RootAnimBP != NULL;

	// Copy up data from a parent anim blueprint
	if (bIsDerivedAnimBlueprint)
	{
		UAnimBlueprintGeneratedClass* RootAnimClass = CastChecked<UAnimBlueprintGeneratedClass>(RootAnimBP->GeneratedClass);

		NewAnimBlueprintClass->BakedStateMachines.Append(RootAnimClass->BakedStateMachines);
		NewAnimBlueprintClass->AnimNotifies.Append(RootAnimClass->AnimNotifies);
		NewAnimBlueprintClass->RootAnimNodeIndex = RootAnimClass->RootAnimNodeIndex;
	}
}


void FAnimBlueprintCompiler::CreateFunctionList()
{
	// (These will now be processed after uber graph merge)

	// Build the list of functions and do preprocessing on all of them
	Super::CreateFunctionList();
}

void FAnimBlueprintCompiler::ProcessTransitionGetter(UK2Node_TransitionRuleGetter* Getter, UAnimStateTransitionNode* TransitionNode)
{
	// Get common elements for multiple getters
	UEdGraphPin* OutputPin = Getter->GetOutputPin();

	UEdGraphPin* SourceTimePin = NULL;
	UAnimationAsset* AnimAsset= NULL;

	if (UAnimGraphNode_Base* SourcePlayerNode = Getter->AssociatedAnimAssetPlayerNode)
	{
		// This check should never fail as the source state is always processed first before handling it's rules
		UAnimGraphNode_Base* TrueSourceNode = MessageLog.FindSourceObjectTypeChecked<UAnimGraphNode_Base>(SourcePlayerNode);
		UAnimGraphNode_Base* UndertypedPlayerNode = SourceNodeToProcessedNodeMap.FindRef(TrueSourceNode);

		if (UndertypedPlayerNode == NULL)
		{
			MessageLog.Error(TEXT("ICE: Player node @@ was not processed prior to handling a transition getter @@ that used it"), SourcePlayerNode, Getter);
			return;
		}

		// Make sure the node is still relevant
		UEdGraph* PlayerGraph = UndertypedPlayerNode->GetGraph();
		if (!PlayerGraph->Nodes.Contains(UndertypedPlayerNode))
		{
			MessageLog.Error(TEXT("@@ is not associated with a node in @@; please delete and recreate it"), Getter, PlayerGraph);
		}

		// Make sure the referenced AnimAsset player has been allocated
		const int32 PlayerNodeIndex = GetAllocationIndexOfNode(UndertypedPlayerNode);
		if (PlayerNodeIndex == INDEX_NONE)
		{
			MessageLog.Error(*LOCTEXT("BadAnimAssetNodeUsedInGetter", "@@ doesn't have a valid associated AnimAsset node.  Delete and recreate it").ToString(), Getter);
		}

		// Grab the AnimAsset, and time pin if needed
		UScriptStruct* TimePropertyInStructType = NULL;
		const TCHAR* TimePropertyName = NULL;
		if (UndertypedPlayerNode->DoesSupportTimeForTransitionGetter())
		{
			AnimAsset = UndertypedPlayerNode->GetAnimationAsset();
			TimePropertyInStructType = UndertypedPlayerNode->GetTimePropertyStruct();
			TimePropertyName = UndertypedPlayerNode->GetTimePropertyName();
		}
		else
		{
			MessageLog.Error(TEXT("@@ is associated with @@, which is an unexpected type"), Getter, UndertypedPlayerNode);
		}

		bool bNeedTimePin = false;

		// Determine if we need to read the current time variable from the specified sequence player
		switch (Getter->GetterType)
		{
		case ETransitionGetter::AnimationAsset_GetCurrentTime:
		case ETransitionGetter::AnimationAsset_GetCurrentTimeFraction:
		case ETransitionGetter::AnimationAsset_GetTimeFromEnd:
		case ETransitionGetter::AnimationAsset_GetTimeFromEndFraction:
			bNeedTimePin = true;
			break;
		default:
			bNeedTimePin = false;
			break;
		}

		if (bNeedTimePin && (PlayerNodeIndex != INDEX_NONE) && (TimePropertyName != NULL) && (TimePropertyInStructType != NULL))
		{
			UProperty* NodeProperty = AllocatedPropertiesByIndex.FindChecked(PlayerNodeIndex);

			// Create a struct member read node to grab the current position of the sequence player node
			UK2Node_StructMemberGet* TimeReadNode = SpawnIntermediateNode<UK2Node_StructMemberGet>(Getter, ConsolidatedEventGraph);
			TimeReadNode->VariableReference.SetSelfMember(NodeProperty->GetFName());
			TimeReadNode->StructType = TimePropertyInStructType;

			TimeReadNode->AllocatePinsForSingleMemberGet(TimePropertyName);
			SourceTimePin = TimeReadNode->FindPinChecked(TimePropertyName);
		}
	}

	// Expand it out
	UK2Node_CallFunction* GetterHelper = NULL;
	switch (Getter->GetterType)
	{
	case ETransitionGetter::AnimationAsset_GetCurrentTime:
		if (SourceTimePin != NULL)
		{
			// Move all the connections over
			for (int32 LinkIndex = 0; LinkIndex < OutputPin->LinkedTo.Num(); ++LinkIndex)
			{
				UEdGraphPin* TimeConsumerPin = OutputPin->LinkedTo[LinkIndex];
				TimeConsumerPin->MakeLinkTo(SourceTimePin);
			}
			OutputPin->BreakAllPinLinks();
		}
		else
		{
			MessageLog.Error(TEXT("@@ is not associated with any AnimAsset players"), Getter);
		}
		break;
	case ETransitionGetter::AnimationAsset_GetLength:
		if (AnimAsset != NULL)
		{
			GetterHelper = SpawnCallAnimInstanceFunction(Getter, TEXT("GetAnimAssetPlayerLength"));
			GetterHelper->FindPinChecked(TEXT("AnimAsset"))->DefaultObject = AnimAsset;
		}
		else
		{
			MessageLog.Error(TEXT("@@ is not associated with any AnimAsset players"), Getter);
		}
		break;
	case ETransitionGetter::AnimationAsset_GetCurrentTimeFraction:
		if ((AnimAsset != NULL) && (SourceTimePin != NULL))
		{
			GetterHelper = SpawnCallAnimInstanceFunction(Getter, TEXT("GetAnimAssetPlayerTimeFraction"));
			GetterHelper->FindPinChecked(TEXT("AnimAsset"))->DefaultObject = AnimAsset;
			GetterHelper->FindPinChecked(TEXT("CurrentTime"))->MakeLinkTo(SourceTimePin);
		}
		else
		{
			MessageLog.Error(TEXT("@@ is not associated with any AnimAsset players"), Getter);
		}
		break;
	case ETransitionGetter::AnimationAsset_GetTimeFromEnd:
		if ((AnimAsset != NULL) && (SourceTimePin != NULL))
		{
			GetterHelper = SpawnCallAnimInstanceFunction(Getter, TEXT("GetAnimAssetPlayerTimeFromEnd"));
			GetterHelper->FindPinChecked(TEXT("AnimAsset"))->DefaultObject = AnimAsset;
			GetterHelper->FindPinChecked(TEXT("CurrentTime"))->MakeLinkTo(SourceTimePin);
		}
		else
		{
			MessageLog.Error(TEXT("@@ is not associated with any AnimAsset players"), Getter);
		}
		break;
	case ETransitionGetter::AnimationAsset_GetTimeFromEndFraction:
		if ((AnimAsset != NULL) && (SourceTimePin != NULL))
		{
			GetterHelper = SpawnCallAnimInstanceFunction(Getter, TEXT("GetAnimAssetPlayerTimeFromEndFraction"));
			GetterHelper->FindPinChecked(TEXT("AnimAsset"))->DefaultObject = AnimAsset;
			GetterHelper->FindPinChecked(TEXT("CurrentTime"))->MakeLinkTo(SourceTimePin);
		}
		else
		{
			MessageLog.Error(TEXT("@@ is not associated with any AnimAsset players"), Getter);
		}
		break;

	case ETransitionGetter::CurrentTransitionDuration:
		{
			check(TransitionNode);
			const FString TransitionDurationStr = FString::Printf(TEXT("%f"), TransitionNode->CrossfadeDuration);

			// Move all the connections over to the literal value
			//@TODO: Hovering over the output pin won't display any value in the debugger
			for (int32 LinkIndex = 0; LinkIndex < OutputPin->LinkedTo.Num(); ++LinkIndex)
			{
				UEdGraphPin* ConsumerPin = OutputPin->LinkedTo[LinkIndex];
				ConsumerPin->DefaultValue = TransitionDurationStr;
			}
			OutputPin->BreakAllPinLinks();
		}
		break;

	case ETransitionGetter::ArbitraryState_GetBlendWeight:
		{
			if (Getter->AssociatedStateNode)
			{
				if (UAnimStateNode* SourceStateNode = MessageLog.FindSourceObjectTypeChecked<UAnimStateNode>(Getter->AssociatedStateNode))
				{
					if (FStateMachineDebugData* DebugData = NewAnimBlueprintClass->GetAnimBlueprintDebugData().StateMachineDebugData.Find(SourceStateNode->GetGraph()))
					{
						if (int32* pStateIndex = DebugData->NodeToStateIndex.Find(SourceStateNode))
						{
							const int32 StateIndex = *pStateIndex;
							//const int32 MachineIndex = DebugData->MachineIndex;

							// This check should never fail as all animation nodes should be processed before getters are
							UAnimGraphNode_Base* CompiledMachineInstanceNode = SourceNodeToProcessedNodeMap.FindChecked(DebugData->MachineInstanceNode.Get());
							const int32 MachinePropertyIndex = AllocatedAnimNodeIndices.FindChecked(CompiledMachineInstanceNode);

							GetterHelper = SpawnCallAnimInstanceFunction(Getter, TEXT("GetStateWeight"));
							GetterHelper->FindPinChecked(TEXT("MachineIndex"))->DefaultValue = FString::FromInt(MachinePropertyIndex);
							GetterHelper->FindPinChecked(TEXT("StateIndex"))->DefaultValue = FString::FromInt(StateIndex);
						}
					}
				}
			}

			if (GetterHelper == NULL)
			{
				MessageLog.Error(TEXT("@@ is not associated with a valid state"), Getter);
			}
		}
		break;

	case ETransitionGetter::CurrentState_ElapsedTime:
		{
			check(TransitionNode);
			if (UAnimStateNode* SourceStateNode = MessageLog.FindSourceObjectTypeChecked<UAnimStateNode>(TransitionNode->GetPreviousState()))
			{
				if (FStateMachineDebugData* DebugData = NewAnimBlueprintClass->GetAnimBlueprintDebugData().StateMachineDebugData.Find(SourceStateNode->GetGraph()))
				{
					// This check should never fail as all animation nodes should be processed before getters are
					UAnimGraphNode_Base* CompiledMachineInstanceNode = SourceNodeToProcessedNodeMap.FindChecked(DebugData->MachineInstanceNode.Get());
					const int32 MachinePropertyIndex = AllocatedAnimNodeIndices.FindChecked(CompiledMachineInstanceNode);

					GetterHelper = SpawnCallAnimInstanceFunction(Getter, TEXT("GetCurrentStateElapsedTime"));
					GetterHelper->FindPinChecked(TEXT("MachineIndex"))->DefaultValue = FString::FromInt(MachinePropertyIndex);
				}
			}
			if (GetterHelper == NULL)
			{
				MessageLog.Error(TEXT("@@ is not associated with a valid state"), Getter);
			}
		}
		break;

	case ETransitionGetter::CurrentState_GetBlendWeight:
		{
			check(TransitionNode);
			if (UAnimStateNode* SourceStateNode = MessageLog.FindSourceObjectTypeChecked<UAnimStateNode>(TransitionNode->GetPreviousState()))
			{
				{
					if (FStateMachineDebugData* DebugData = NewAnimBlueprintClass->GetAnimBlueprintDebugData().StateMachineDebugData.Find(SourceStateNode->GetGraph()))
					{
						if (int32* pStateIndex = DebugData->NodeToStateIndex.Find(SourceStateNode))
						{
							const int32 StateIndex = *pStateIndex;
							//const int32 MachineIndex = DebugData->MachineIndex;

							// This check should never fail as all animation nodes should be processed before getters are
							UAnimGraphNode_Base* CompiledMachineInstanceNode = SourceNodeToProcessedNodeMap.FindChecked(DebugData->MachineInstanceNode.Get());
							const int32 MachinePropertyIndex = AllocatedAnimNodeIndices.FindChecked(CompiledMachineInstanceNode);

							GetterHelper = SpawnCallAnimInstanceFunction(Getter, TEXT("GetStateWeight"));
							GetterHelper->FindPinChecked(TEXT("MachineIndex"))->DefaultValue = FString::FromInt(MachinePropertyIndex);
							GetterHelper->FindPinChecked(TEXT("StateIndex"))->DefaultValue = FString::FromInt(StateIndex);
						}
					}
				}
			}
			if (GetterHelper == NULL)
			{
				MessageLog.Error(TEXT("@@ is not associated with a valid state"), Getter);
			}
		}
		break;

	default:
		MessageLog.Error(TEXT("Unrecognized getter type on @@"), Getter);
		break;
	}

	// Finish wiring up a call function if needed
	if (GetterHelper != NULL)
	{
		check(GetterHelper->IsNodePure());

		UEdGraphPin* NewReturnPin = GetterHelper->FindPinChecked(TEXT("ReturnValue"));
		MessageLog.NotifyIntermediateObjectCreation(NewReturnPin, OutputPin);

		NewReturnPin->CopyPersistentDataFromOldPin(*OutputPin);
	}

	// Remove the getter from the equation
	Getter->BreakAllNodeLinks();
}

int32 FAnimBlueprintCompiler::FindOrAddNotify(FAnimNotifyEvent& Notify)
{
	if ((Notify.NotifyName == NAME_None) && (Notify.Notify == NULL) && (Notify.NotifyStateClass == NULL))
	{
		// Non event, don't add it
		return INDEX_NONE;
	}

	int32 NewIndex = INDEX_NONE;
	for (int32 NotifyIdx = 0; NotifyIdx < NewAnimBlueprintClass->AnimNotifies.Num(); NotifyIdx++)
	{
		if( (NewAnimBlueprintClass->AnimNotifies[NotifyIdx].NotifyName == Notify.NotifyName) 
			&& (NewAnimBlueprintClass->AnimNotifies[NotifyIdx].Notify == Notify.Notify) 
			&& (NewAnimBlueprintClass->AnimNotifies[NotifyIdx].NotifyStateClass == Notify.NotifyStateClass) 
			)
		{
			NewIndex = NotifyIdx;
			break;
		}
	}

	if (NewIndex == INDEX_NONE)
	{
		NewIndex = NewAnimBlueprintClass->AnimNotifies.Add(Notify);
	}
	return NewIndex;
}

void FAnimBlueprintCompiler::PostCompileDiagnostics()
{
	FKismetCompilerContext::PostCompileDiagnostics();

	if (!bIsDerivedAnimBlueprint)
	{
		// Run thru all nodes and make sure they like the final results
		for (auto NodeIt = AllocatedAnimNodeIndices.CreateConstIterator(); NodeIt; ++NodeIt)
		{
			if (UAnimGraphNode_Base* Node = NodeIt.Key())
			{
				Node->ValidateAnimNodePostCompile(MessageLog, NewAnimBlueprintClass, NodeIt.Value());
			}
		}

		//
		bool bDisplayAnimDebug = false;
		if (!Blueprint->bIsRegeneratingOnLoad)
		{
			GConfig->GetBool(TEXT("Kismet"), TEXT("CompileDisplaysAnimBlueprintBackend"), /*out*/ bDisplayAnimDebug, GEngineIni);

			if (bDisplayAnimDebug)
			{
				DumpAnimDebugData();
			}
		}
	}
}

void FAnimBlueprintCompiler::DumpAnimDebugData()
{
	// List all compiled-down nodes and their sources
	if (NewAnimBlueprintClass->RootAnimNodeProperty == NULL)
	{
		return;
	}

	int32 RootIndex = INDEX_NONE;
	NewAnimBlueprintClass->AnimNodeProperties.Find(NewAnimBlueprintClass->RootAnimNodeProperty, /*out*/ RootIndex);
	
	uint8* CDOBase = (uint8*)NewAnimBlueprintClass->ClassDefaultObject;

	MessageLog.Note(*FString::Printf(TEXT("Anim Root is #%d"), RootIndex));
	for (int32 Index = 0; Index < NewAnimBlueprintClass->AnimNodeProperties.Num(); ++Index)
	{
		UStructProperty* NodeProperty = NewAnimBlueprintClass->AnimNodeProperties[Index];
		FString PropertyName = NodeProperty->GetName();
		FString PropertyType = NodeProperty->Struct->GetName();
		FString RootSuffix = (Index == RootIndex) ? TEXT(" <--- ROOT") : TEXT("");

		// Print out the node
		MessageLog.Note(*FString::Printf(TEXT("[%d] @@ [prop %s]%s"), Index, *PropertyName, *RootSuffix), AllocatedNodePropertiesToNodes.FindRef(NodeProperty));

		// Print out all the node links
		for (TFieldIterator<UProperty> PropIt(NodeProperty->Struct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			UProperty* ChildProp = *PropIt;
			if (UStructProperty* ChildStructProp = Cast<UStructProperty>(ChildProp))
			{
				if (ChildStructProp->Struct->IsChildOf(FPoseLinkBase::StaticStruct()))
				{
					uint8* ChildPropertyPtr =  ChildStructProp->ContainerPtrToValuePtr<uint8>(NodeProperty->ContainerPtrToValuePtr<uint8>(CDOBase));
					FPoseLinkBase* ChildPoseLink = (FPoseLinkBase*)ChildPropertyPtr;

					if (ChildPoseLink->LinkID != INDEX_NONE)
					{
						UStructProperty* LinkedProperty = NewAnimBlueprintClass->AnimNodeProperties[ChildPoseLink->LinkID];
						MessageLog.Note(*FString::Printf(TEXT("   Linked via %s to [#%d] @@"), *ChildStructProp->GetName(), ChildPoseLink->LinkID), AllocatedNodePropertiesToNodes.FindRef(LinkedProperty));
					}
					else
					{
						MessageLog.Note(*FString::Printf(TEXT("   Linked via %s to <no connection>"), *ChildStructProp->GetName()));
					}
				}
			}
		}
	}

	const int32 foo = NewAnimBlueprintClass->AnimNodeProperties.Num() - 1;

	MessageLog.Note(TEXT("State machine info:"));
	for (int32 MachineIndex = 0; MachineIndex < NewAnimBlueprintClass->BakedStateMachines.Num(); ++MachineIndex)
	{
		FBakedAnimationStateMachine& Machine = NewAnimBlueprintClass->BakedStateMachines[MachineIndex];
	
		MessageLog.Note(*FString::Printf(TEXT("Machine %s starts at state #%d (%s) and has %d states, %d transitions"),
			*(Machine.MachineName.ToString()),
			Machine.InitialState,
			*(Machine.States[Machine.InitialState].StateName.ToString()),
			Machine.States.Num(),
			Machine.Transitions.Num()));

		for (int32 StateIndex = 0; StateIndex < Machine.States.Num(); ++StateIndex)
		{
			FBakedAnimationState& SingleState = Machine.States[StateIndex];
			
			MessageLog.Note(*FString::Printf(TEXT("  State #%d is named %s, with %d exit transitions; linked to graph #%d"),
				StateIndex,
				*(SingleState.StateName.ToString()),
				SingleState.Transitions.Num(),
				foo - SingleState.StateRootNodeIndex));

			for (int32 RuleIndex = 0; RuleIndex < SingleState.Transitions.Num(); ++RuleIndex)
			{
				FBakedStateExitTransition& ExitTransition = SingleState.Transitions[RuleIndex];

				int32 TargetStateIndex = Machine.Transitions[ExitTransition.TransitionIndex].NextState;

				MessageLog.Note(*FString::Printf(TEXT("    Exit trans #%d to %s uses global trans %d, wanting %s, linked to delegate #%d "),
					RuleIndex,
					*Machine.States[TargetStateIndex].StateName.ToString(),
					ExitTransition.TransitionIndex,
					ExitTransition.bDesiredTransitionReturnValue ? TEXT("TRUE") : TEXT("FALSE"),
					foo - ExitTransition.CanTakeDelegateIndex));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
