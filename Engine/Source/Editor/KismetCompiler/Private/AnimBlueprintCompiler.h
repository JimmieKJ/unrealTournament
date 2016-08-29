// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetCompiler.h"
#include "AnimGraphDefinitions.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationGraphSchema.h"
#include "K2Node_TransitionRuleGetter.h"
#include "AnimGraphNode_Base.h"
//
// Forward declarations.
//
class UAnimGraphNode_SaveCachedPose;
class UAnimGraphNode_UseCachedPose;
class UAnimGraphNode_SubInput;
class UAnimGraphNode_SubInstance;

class UStructProperty;
class UBlueprintGeneratedClass;
struct FPoseLinkMappingRecord;

//////////////////////////////////////////////////////////////////////////
// FAnimBlueprintCompiler

class KISMETCOMPILER_API FAnimBlueprintCompiler : public FKismetCompilerContext
{
protected:
	typedef FKismetCompilerContext Super;
public:
	FAnimBlueprintCompiler(UAnimBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions, TArray<UObject*>* InObjLoaded);
	virtual ~FAnimBlueprintCompiler();

	virtual void PostCompile() override;

protected:
	// Implementation of FKismetCompilerContext interface
	virtual UEdGraphSchema_K2* CreateSchema() override;
	virtual void MergeUbergraphPagesIn(UEdGraph* Ubergraph) override;
	virtual void ProcessOneFunctionGraph(UEdGraph* SourceGraph, bool bInternalFunction = false) override;
	virtual void CreateFunctionList() override;
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject) override;
	virtual void PostCompileDiagnostics() override;
	virtual void EnsureProperGeneratedClass(UClass*& TargetClass) override;
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO) override;
	virtual void FinishCompilingClass(UClass* Class) override;
	// End of FKismetCompilerContext interface

protected:
	typedef TArray<UEdGraphPin*> UEdGraphPinArray;

protected:
	// Wireup record for a single anim node property (which might be an array)
	struct FAnimNodeSinglePropertyHandler
	{
		TMap<int32, UEdGraphPin*> ArrayPins;

		UEdGraphPin* SinglePin;

		// If this is valid we are a 'simple property copy', avoiding BP thunk
		FName SimpleCopyPropertyName;

		// If this is a sub-struct property, this will be something other than NAME_None
		FName SubStructPropertyName;

		// Any operation we want to perform post-copy on the destination data
		EPostCopyOperation Operation;

		// Whether this node chain only ever accesses members
		bool bHasOnlyMemberAccess;

		// If the anim instance is the container target instead of the node.
		bool bInstanceIsTarget;

		FAnimNodeSinglePropertyHandler()
			: SinglePin(NULL)
			, SimpleCopyPropertyName(NAME_None)
			, SubStructPropertyName(NAME_None)
			, Operation(EPostCopyOperation::None)
			, bHasOnlyMemberAccess(false)
			, bInstanceIsTarget(false)
		{
		}
	};

	// Record for a property that was exposed as a pin, but wasn't wired up (just a literal)
	struct FEffectiveConstantRecord
	{
	public:
		// The node variable that the handler is in
		class UStructProperty* NodeVariableProperty;

		// The property within the struct to set
		class UProperty* ConstantProperty;

		// The array index if ConstantProperty is an array property, or INDEX_NONE otherwise
		int32 ArrayIndex;

		// The pin to pull the DefaultValue/DefaultObject from
		UEdGraphPin* LiteralSourcePin;

		FEffectiveConstantRecord()
			: NodeVariableProperty(NULL)
			, ConstantProperty(NULL)
			, ArrayIndex(INDEX_NONE)
			, LiteralSourcePin(NULL)
		{
		}

		FEffectiveConstantRecord(UStructProperty* ContainingNodeProperty, UEdGraphPin* SourcePin, UProperty* SourcePinProperty, int32 SourceArrayIndex)
			: NodeVariableProperty(ContainingNodeProperty)
			, ConstantProperty(SourcePinProperty)
			, ArrayIndex(SourceArrayIndex)
			, LiteralSourcePin(SourcePin)
		{
		}

		bool Apply(UObject* Object);
	};

	struct FEvaluationHandlerRecord
	{
	public:

		// The instance property to use if we aren't accessing data through the node variable
		UProperty* InstanceProperty;

		// The node variable that the handler is in
		UStructProperty* NodeVariableProperty;

		// The specific evaluation handler inside the specified node
		UStructProperty* EvaluationHandlerProperty;

		// Set of properties serviced by this handler (Map from property name to the record for that property)
		TMap<FName, FAnimNodeSinglePropertyHandler> ServicedProperties;

		// The resulting function name
		FName HandlerFunctionName;

	public:

		FEvaluationHandlerRecord()
			: InstanceProperty(nullptr)
			, NodeVariableProperty(nullptr)
			, EvaluationHandlerProperty(nullptr)
			, HandlerFunctionName(NAME_None)
		{}

		bool OnlyUsesCopyRecords() const
		{
			for(TMap<FName, FAnimNodeSinglePropertyHandler>::TConstIterator It(ServicedProperties); It; ++It)
			{
				const FAnimNodeSinglePropertyHandler& AnimNodeSinglePropertyHandler = It.Value();
				if(AnimNodeSinglePropertyHandler.SimpleCopyPropertyName == NAME_None)
				{
					return false;
				}
			}

			return true;
		}

		bool IsValid() const
		{
			return NodeVariableProperty != nullptr && EvaluationHandlerProperty != nullptr;
		}

		void PatchFunctionNameAndCopyRecordsInto(UObject* TargetObject) const;

		void RegisterPin(UEdGraphPin* SourcePin, UProperty* AssociatedProperty, int32 AssociatedPropertyArrayIndex);

		UStructProperty* GetHandlerNodeProperty() const { return NodeVariableProperty; }

	private:

		bool CheckForVariableGet(FAnimNodeSinglePropertyHandler& Handler, UEdGraphPin* SourcePin);

		bool CheckForLogicalNot(FAnimNodeSinglePropertyHandler& Handler, UEdGraphPin* SourcePin);

		bool CheckForStructMemberAccess(FAnimNodeSinglePropertyHandler& Handler, UEdGraphPin* SourcePin);

		bool CheckForMemberOnlyAccess(FAnimNodeSinglePropertyHandler& Handler, UEdGraphPin* SourcePin);
	};

	// State machines may get processed before their inner graphs, so their node index needs to be patched up later
	// This structure records pending fixups.
	struct FStateRootNodeIndexFixup
	{
	public:
		int32 MachineIndex;
		int32 StateIndex;
		UAnimGraphNode_StateResult* ResultNode;

	public:
		FStateRootNodeIndexFixup(int32 InMachineIndex, int32 InStateIndex, UAnimGraphNode_StateResult* InResultNode)
			: MachineIndex(InMachineIndex)
			, StateIndex(InStateIndex)
			, ResultNode(InResultNode)
		{
		}
	};

protected:
	UAnimBlueprintGeneratedClass* NewAnimBlueprintClass;
	UAnimBlueprint* AnimBlueprint;

	UAnimationGraphSchema* AnimSchema;

	// Map of allocated v3 nodes that are members of the class
	TMap<class UAnimGraphNode_Base*, UProperty*> AllocatedAnimNodes;
	TMap<UProperty*, class UAnimGraphNode_Base*> AllocatedNodePropertiesToNodes;
	TMap<int32, UProperty*> AllocatedPropertiesByIndex;

	// Map of true source objects (user edited ones) to the cloned ones that are actually compiled
	TMap<class UAnimGraphNode_Base*, UAnimGraphNode_Base*> SourceNodeToProcessedNodeMap;

	// Index of the nodes (must match up with the runtime discovery process of nodes, which runs thru the property chain)
	int32 AllocateNodeIndexCounter;
	TMap<class UAnimGraphNode_Base*, int32> AllocatedAnimNodeIndices;

	// Map from pose link LinkID address
	//@TODO: Bad structure for a list of these
	TArray<FPoseLinkMappingRecord> ValidPoseLinkList;

	// List of successfully created evaluation handlers
	TArray<FEvaluationHandlerRecord> ValidEvaluationHandlerList;

	// List of animation node literals (values exposed as pins but never wired up) that need to be pushed into the CDO
	TArray<FEffectiveConstantRecord> ValidAnimNodePinConstants;

	// Map of cache name to encountered save cached pose nodes
	TMap<FString, UAnimGraphNode_SaveCachedPose*> SaveCachedPoseNodes;

	// List of getter node's we've found so the auto-wire can be deferred till after state machine compilation
	TArray<class UK2Node_AnimGetter*> FoundGetterNodes;

	// Set of used handler function names
	TSet<FName> HandlerFunctionNames;

	// True if any parent class is also generated from an animation blueprint
	bool bIsDerivedAnimBlueprint;
private:
	int32 FindOrAddNotify(FAnimNotifyEvent& Notify);

	UK2Node_CallFunction* SpawnCallAnimInstanceFunction(UEdGraphNode* SourceNode, FName FunctionName);

	// Creates an evaluation handler for an FExposedValue property in an animation node
	void CreateEvaluationHandlerStruct(UAnimGraphNode_Base* VisualAnimNode, FEvaluationHandlerRecord& Record);
	void CreateEvaluationHandlerInstance(UAnimGraphNode_Base* VisualAnimNode, FEvaluationHandlerRecord& Record);

	// Prunes any nodes that aren't reachable via a pose link
	void PruneIsolatedAnimationNodes(const TArray<UAnimGraphNode_Base*>& RootSet, TArray<UAnimGraphNode_Base*>& GraphNodes);

	// Compiles one animation node
	void ProcessAnimationNode(UAnimGraphNode_Base* VisualAnimNode);

	// Compiles one state machine
	void ProcessStateMachine(UAnimGraphNode_StateMachineBase* StateMachineInstance);

	// Compiles one use cached pose instance
	void ProcessUseCachedPose(UAnimGraphNode_UseCachedPose* UseCachedPose);

	// Compiles one sub instance node
	void ProcessSubInstance(UAnimGraphNode_SubInstance* SubInstance);

	// Traverses subinstance links looking for slot names and state machine names, returning their count in a name map
	typedef TMap<FName, int32> NameToCountMap;
	void GetDuplicatedSlotAndStateNames(UAnimGraphNode_SubInstance* InSubInstance, NameToCountMap& OutStateMachineNameToCountMap, NameToCountMap& OutSlotNameToCountMap);

	// Compiles an entire animation graph
	void ProcessAllAnimationNodes();

	// Convert transition getters into a function call/etc...
	void ProcessTransitionGetter(class UK2Node_TransitionRuleGetter* Getter, UAnimStateTransitionNode* TransitionNode);

	//
	void ProcessAnimationNodesGivenRoot(TArray<UAnimGraphNode_Base*>& AnimNodeList, const TArray<UAnimGraphNode_Base*>& RootSet);

	// Builds the update order list for saved pose nodes in this blueprint
	void BuildCachedPoseNodeUpdateOrder();

	// Traverses a graph to collect save pose nodes starting at InRootNode, then processes each node
	void CachePoseNodeOrdering_StartNewTraversal(UAnimGraphNode_Base* InRootNode, TArray<UAnimGraphNode_SaveCachedPose*> &OrderedSavePoseNodes, TArray<UAnimGraphNode_Base*> VisitedRootNodes);

	// Traverses a graph to collect save pose nodes starting at InAnimGraphNode, does NOT process saved pose nodes afterwards
	void CachePoseNodeOrdering_TraverseInternal(UAnimGraphNode_Base* InAnimGraphNode, TArray<UAnimGraphNode_SaveCachedPose*> &OrderedSavePoseNodes);

	// Gets all anim graph nodes that are piped into the provided node (traverses input pins)
	void GetLinkedAnimNodes(UAnimGraphNode_Base* InGraphNode, TArray<UAnimGraphNode_Base*> &LinkedAnimNodes);

	// Automatically fill in parameters for the specified Getter node
	void AutoWireAnimGetter(class UK2Node_AnimGetter* Getter, UAnimStateTransitionNode* InTransitionNode);

	// This function does the following steps:
	//   Clones the nodes in the specified source graph
	//   Merges them into the ConsolidatedEventGraph
	//   Processes any animation nodes
	//   Returns the index of the processed cloned version of SourceRootNode
	//	 If supplied, will also return an array of all cloned nodes
	int32 ExpandGraphAndProcessNodes(UEdGraph* SourceGraph, UAnimGraphNode_Base* SourceRootNode, UAnimStateTransitionNode* TransitionNode = NULL, TArray<UEdGraphNode*>* ClonedNodes = NULL);

	// Dumps compiler diagnostic information
	void DumpAnimDebugData();

	// Returns the allocation index of the specified node, processing it if it was pending
	int32 GetAllocationIndexOfNode(UAnimGraphNode_Base* VisualAnimNode);
};

