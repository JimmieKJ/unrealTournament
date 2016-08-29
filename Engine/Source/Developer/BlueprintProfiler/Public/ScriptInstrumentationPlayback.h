// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/Kismet/Public/Profiler/TracePath.h"

//////////////////////////////////////////////////////////////////////////
// FBlueprintExecutionTrace

struct FBlueprintExecutionTrace
{
	EScriptInstrumentation::Type TraceType;
	FTracePath TracePath;
	FName InstanceName;
	FName FunctionName;
	TWeakPtr<FScriptExecutionNode> ProfilerNode;
	int32 Offset;
	double ObservationTime;
};

//////////////////////////////////////////////////////////////////////////
// FBlueprintExecutionContext

class FBlueprintExecutionContext : public TSharedFromThis<FBlueprintExecutionContext>
{
public:

	FBlueprintExecutionContext()
		: bIsBlueprintMapped(false)
		, ExecutionTraceHistory(256)
	{
	}

	/** Initialise the context from the blueprint path name */
	bool InitialiseContext(const FString& BlueprintPath);

	/** Returns if the blueprint context is fully formed */
	BLUEPRINTPROFILER_API bool IsContextValid() const { return Blueprint.IsValid() && BlueprintClass.IsValid(); }

	/** Returns if the blueprint is mapped */
	bool IsBlueprintMapped() const { return bIsBlueprintMapped; }

	/** Removes the blueprint mapping */
	void RemoveMapping();

	/** Returns the blueprint this context represents */
	TWeakObjectPtr<UBlueprint> GetBlueprint() const { return Blueprint; }

	/** Returns the blueprint class this context represents */
	TWeakObjectPtr<UBlueprintGeneratedClass> GetBlueprintClass() const { return BlueprintClass; }

	/** Returns the blueprint exec node for this context */
	TSharedPtr<class FScriptExecutionBlueprint> GetBlueprintExecNode() const { return BlueprintNode; }

	/** Returns the blueprint trace history */
	const TSimpleRingBuffer<FBlueprintExecutionTrace>& GetTraceHistory() const { return ExecutionTraceHistory; }

	/** Returns a new trace history event */
	FBlueprintExecutionTrace& AddNewTraceHistoryEvent() { return ExecutionTraceHistory.WriteNewElementInitialized(); }

	/** Returns if an event is mapped. */
	bool IsEventMapped(const FName EventName) const;

	/** Add event execution node */
	void AddEventNode(TSharedPtr<class FBlueprintFunctionContext> FunctionContext, TSharedPtr<FScriptExecutionNode> EventExecNode);

	/** Register a function context for an event */
	void RegisterEventContext(const FName EventName, TSharedPtr<FBlueprintFunctionContext> FunctionContext);

	/** Returns true if an execution node exists for the instance */
	BLUEPRINTPROFILER_API bool HasProfilerDataForInstance(const FName InstanceName) const;

	/** Maps the instance returning the system name for instance */
	BLUEPRINTPROFILER_API FName MapBlueprintInstance(const FString& InstancePath);

	/** Returns the instance if mapped */
	BLUEPRINTPROFILER_API TSharedPtr<class FScriptExecutionInstance> GetInstanceExecNode(const FName InstanceName);

	/** Remaps PIE actor instance paths to editor actor instances */
	BLUEPRINTPROFILER_API FName RemapInstancePath(const FName InstanceName) const;

	/** Returns the function context containing the event */
	TSharedPtr<class FBlueprintFunctionContext> GetFunctionContextForEventChecked(const FName ScopedEventName) const;

	/** Returns the function context matching the supplied name providing it exists */
	TSharedPtr<class FBlueprintFunctionContext> GetFunctionContext(const FName FunctionNameIn) const;

	/** Returns the function context matching the supplied graph providing it exists */
	TSharedPtr<FBlueprintFunctionContext> GetFunctionContextFromGraph(const UEdGraph* Graph) const;

	/** Adds a manually created function context */
	void AddFunctionContext(const FName FunctionName, TSharedPtr<class FBlueprintFunctionContext> FunctionContext)
	{
		FunctionContexts.Add(FunctionName) = FunctionContext; 
	}

	/** Returns if this context has an execution node for the specified graph pin */
	BLUEPRINTPROFILER_API bool HasProfilerDataForPin(const UEdGraphPin* GraphPin) const;

	/** Returns if this context has an execution node for the specified graph pin, otherwise it falls back to a owning node lookup */
	BLUEPRINTPROFILER_API TSharedPtr<FScriptExecutionNode> GetProfilerDataForPin(const UEdGraphPin* GraphPin);

	/** Returns if this context has an execution node for the specified graph node */
	BLUEPRINTPROFILER_API bool HasProfilerDataForNode(const UEdGraphNode* GraphNode) const;

	/** Returns existing or creates a new execution node for the specified graphnode */
	BLUEPRINTPROFILER_API TSharedPtr<FScriptExecutionNode> GetProfilerDataForNode(const UEdGraphNode* GraphNode);

	/** Updates cascading stats for the blueprint, this merges instance data upwards to the blueprint */
	void UpdateConnectedStats();

	/** Scoped function name extraction that results in ClassName::FunctionName for lookups */
	FName GetScopedFunctionNameFromGraph(const UEdGraph* Graph) const;

	/** Basic function name creation that allows for UFunction lookups in the correct class */
	FName GetFunctionNameFromGraph(const UEdGraph* Graph) const;

	/** Registers a pure node using the pin connected to it for later lookup */
	void RegisterPurePinNode(const UEdGraphPin* PurePin, TSharedPtr<FScriptExecutionNode> PureExecNode) { PureNodeMap.Add(PurePin) = PureExecNode; }

	/** Locates a pure node using the pin connected to it. */
	TSharedPtr<FScriptExecutionNode> FindPurePinNode(const UEdGraphPin* PurePin);

private:

	/** Creates and returns a new function context, returns existing if present  */
	template<typename FunctionType> TSharedPtr<FunctionType> CreateFunctionContext(const FName FunctionName, UEdGraph* Graph);

	/** Walks the blueprint and maps events and functions ready for profiling data */
	bool MapBlueprintExecution();

	/** Corrects the instance name if it is a PIE instance and sets the object weakptr to the editor actor, returns true if this is a new instance */
	bool ResolveInstance(FName& InstanceNameInOut, TWeakObjectPtr<const UObject>& ObjectInOut);

private:

	/** Indicates the blueprint has been mapped */
	bool bIsBlueprintMapped;
	/** Blueprint the context represents */
	TWeakObjectPtr<UBlueprint> Blueprint;
	/** Blueprint Generated class for the context */
	TWeakObjectPtr<UBlueprintGeneratedClass> BlueprintClass;
	/** Root level execution node for this blueprint */
	TSharedPtr<FScriptExecutionBlueprint> BlueprintNode;
	/** UFunction contexts for this blueprint */
	TMap<FName, TSharedPtr<FBlueprintFunctionContext>> FunctionContexts;
	/** UFunction context lookup by event name */
	TMap<FName, TSharedPtr<FBlueprintFunctionContext>> EventFunctionContexts;
	/** PIE instance remap container */
	TMap<FName, FName> PIEInstanceNameMap;
	/** Editor instances */
	TMap<FName, TWeakObjectPtr<const UObject>> EditorActorInstances;
	/** PIE instances */
	TMap<FName, TWeakObjectPtr<const UObject>> PIEActorInstances;
	/** Pin to pure node map, used to lookup pure nodes across function boundaries */
	TMap<FEdGraphPinReference, TSharedPtr<FScriptExecutionNode>> PureNodeMap;
	/** Event Trace History */
	TSimpleRingBuffer<FBlueprintExecutionTrace> ExecutionTraceHistory;
};

//////////////////////////////////////////////////////////////////////////
// FBlueprintFunctionContext

class FBlueprintFunctionContext : public TSharedFromThis<FBlueprintFunctionContext>
{
public:

	virtual ~FBlueprintFunctionContext() {}

	/** Initialise the function context from the graph */
	virtual void InitialiseContextFromGraph(TSharedPtr<FBlueprintExecutionContext> BlueprintContext, const FName FunctionNameIn, UEdGraph* Graph);

	/** Discover and add any tunnels */
	void DiscoverTunnels(UEdGraph* Graph, TMap<UK2Node_Tunnel*, TSharedPtr<FBlueprintFunctionContext>>& DiscoveredTunnels);

	/** Map Function */
	virtual void MapFunction();

	/** Returns if the function context is fully formed */
	bool IsContextValid() const { return Function.IsValid() && BlueprintClass.IsValid(); }

	/** Returns if the function context is inherited */
	bool IsInheritedContext() const { return bIsInheritedContext; }

	/** Returns the graph node at the specified script offset */
	const UEdGraphNode* GetNodeFromCodeLocation(const int32 ScriptOffset);

	/** Returns the tunnel node at the specified script offset if present */
	const UEdGraphNode* GetTunnelNodeFromCodeLocation(const int32 ScriptOffset);

	/** Returns the tunnel node at associated with the graph node */
	const UEdGraphNode* GetTunnelNodeFromGraphNode(const UEdGraphNode* TrueSourceNode) const;

	/** Returns if the specified node is inside a tunnel */
	bool IsNodeFromTunnelGraph(const UEdGraphNode* Node) const;

	/** Returns the pin at the specified script offset */
	const UEdGraphPin* GetPinFromCodeLocation(const int32 ScriptOffset);

	/** Returns the script offset for the specified pin */
	const int32 GetCodeLocationFromPin(const UEdGraphPin* Pin) const;

	/** Returns all registered script offsets for the specified pin */
	void GetAllCodeLocationsFromPin(const UEdGraphPin* Pin, TArray<int32>& OutCodeLocations) const;

	/** Returns the pure node script code range for the specified node */
	FInt32Range GetPureNodeScriptCodeRange(const UEdGraphNode* Node) const;

	/** Returns true if this function context contains an execution node matching the node name, please note this fails in tunnel contexts */
	bool HasProfilerDataForNode(const FName NodeName) const;

	/** Returns true if this or any child function context contains an entry for the graphnode */
	bool HasProfilerDataForGraphNode(const UEdGraphNode* GraphNode) const;

	/** Returns the execution node representing the node name */
	TSharedPtr<FScriptExecutionNode> GetProfilerDataForNode(const FName NodeName) const;

	/** Returns the execution node representing the node name, asserting on fail */
	TSharedPtr<FScriptExecutionNode> GetProfilerDataForNodeChecked(const FName NodeName) const;

	/** Returns the execution node representing the graph node */
	TSharedPtr<FScriptExecutionNode> GetProfilerDataForGraphNode(const UEdGraphNode* Node) const;

	/** Returns the execution node representing the node name */
	template<typename ExecNodeType> TSharedPtr<ExecNodeType> GetTypedProfilerDataForNode(const FName NodeName);

	/** Returns the execution node representing the object */
	template<typename ExecNodeType> TSharedPtr<ExecNodeType> GetTypedProfilerDataForGraphNode(const UEdGraphNode* Node);

	/** Returns true if the execution node and function context can be located and initialised from the script offset */
	virtual bool GetProfilerContextFromScriptOffset(const int32 ScriptOffset, TSharedPtr<FScriptExecutionNode>& ExecNode, TSharedPtr<FBlueprintFunctionContext>& FunctionContext);

	/** Adds all function entry points to node as children */
	void AddCallSiteEntryPointsToNode(TSharedPtr<FScriptExecutionNode> CallingNode) const;

	/** Returns the function name for this context */
	FName GetFunctionName() const { return FunctionName; }

	/** Returns the graph name for this context */
	FName GetGraphName() const { return GraphName; }

	/** Returns the scoped event name for this context */
	FName GetScopedEventName(const FName EventName) const;

	/** Returns the UFunction for this context */
	TWeakObjectPtr<UFunction> GetUFunction() { return Function; }

	/** Returns the BlueprintClass for this context */
	TWeakObjectPtr<UBlueprintGeneratedClass> GetBlueprintClass() { return BlueprintClass; }

	/** Returns a valid pin name, creating one based on pin characteristics if not available */
	static FName GetPinName(const UEdGraphPin* Pin);

	/** Returns a fully qualified pin name including owning node id */
	static FName GetUniquePinName(const UEdGraphPin* Pin);

	/** Returns a tunnel instance Boundary name */
	static FName GetTunnelBoundaryName(const UEdGraphPin* Pin);

	/** Looks for matching pin in the supplied node, handles pins with no name. */
	static UEdGraphPin* FindMatchingPin(const UEdGraphNode* NodeToSearch, const UEdGraphPin* PinToFind, const bool bIgnoreDirection = false);

	/** Utility function that returns the graph from the supplied node, handling tunnel boundaries to return the inner graph */
	static UEdGraph* GetGraphFromNode(const UEdGraphNode* GraphNode, const bool bAllowNonTunnel = true);

	/** Utility function that returns the graph name from the node argument */
	static FName GetGraphNameFromNode(const UEdGraphNode* GraphNode);

	/** Utility function that returns the tunnel instance function name */
	static FName GetTunnelInstanceFunctionName(const UEdGraphNode* GraphNode);

	/** Utility to get the scoped function name from the graph node */
	FName GetScopedFunctionNameFromNode(const UEdGraphNode* GraphNode) const;

	/** Utility function that returns customizations for a pin node (e.g. Node Color/Icon) */
	static void GetPinCustomizations(const UEdGraphPin* Pin, FScriptExecNodeParams& PinParams);

	/** Utility function that returns customizations for a node (e.g. Node Color/Icon) */
	void GetNodeCustomizations(FScriptExecNodeParams& ParamsInOut) const;

protected:

	friend FBlueprintExecutionContext;
	friend class FBlueprintTunnelInstanceContext;

	/** Utility to create execution node */
	virtual TSharedPtr<FScriptExecutionNode> CreateExecutionNode(FScriptExecNodeParams& InitParams);

	/** Utility to create typed execution node */
	template<typename ExecNodeType> TSharedPtr<ExecNodeType> CreateTypedExecutionNode(FScriptExecNodeParams& InitParams);

	/** Adds and links in a new function entry point */
	void AddEntryPoint(TSharedPtr<FScriptExecutionNode> EntryPoint);

	/** Adds a new function exit point */
	void AddExitPoint(TSharedPtr<FScriptExecutionNode> ExitPoint);

	/** Creates an event for delegate pin entry points */
	void CreateDelegatePinEvents(TSharedPtr<FBlueprintExecutionContext> BlueprintContextIn, const TMap<FName, FEdGraphPinReference>& PinEvents);

	/** Processes and detects any cyclic links making the linkage safe for traversal */
	bool DetectCyclicLinks(TSharedPtr<FScriptExecutionNode> ExecNode, TSet<TSharedPtr<FScriptExecutionNode>>& Filter);

	/** Add child function context */
	void AddChildFunctionContext(const FName FunctionNameIn, TSharedPtr<FBlueprintFunctionContext> ChildContext);

	// --Execution mapping functionality

	/** Examines graphs nodes and determines some characterists to aid in creation of the matching executable node */
	void DetermineGraphNodeCharacteristics(const UEdGraphNode* GraphNode, TArray<UEdGraphPin*>& InputPins, TArray<UEdGraphPin*>& ExecPins, FScriptExecNodeParams& NodeParams);

	/** Maps each blueprint node following execution wires */
	virtual TSharedPtr<FScriptExecutionNode> MapNodeExecution(UEdGraphNode* NodeToMap);

	/** Maps pure node chains following the input pure wires */
	virtual TSharedPtr<FScriptExecutionNode> MapPureNodeExecution(const UEdGraphPin* PinToMap);

	/** Returns the pure chain root node for the argument execution node, creating one if not existing */
	TSharedPtr<FScriptExecutionNode> FindOrCreatePureChainRoot(TSharedPtr<FScriptExecutionNode> ExecNode);

	/** Maps input pin execution */
	virtual void MapInputPins(TSharedPtr<FScriptExecutionNode> ExecNode, const TArray<UEdGraphPin*>& Pins);

	/** Maps pin execution */
	void MapExecPins(TSharedPtr<FScriptExecutionNode> ExecNode, const TArray<UEdGraphPin*>& Pins);

	// --Tunnel mapping functionality

	/** Maps tunnel boundries */
	virtual TSharedPtr<FScriptExecutionNode> MapTunnelBoundary(const UEdGraphPin* TunnelPin);

	/** Get tunnel boundary node */
	virtual TSharedPtr<FScriptExecutionNode> GetTunnelBoundaryNodeChecked(const UEdGraphPin* TunnelPin);

	/** Maps the tunnel point into the instanced graph, creating the instanced graph if not already existing. */
	void MapTunnelInstance(UK2Node_Tunnel* TunnelInstance);

	/** Maps execution pin tunnel entry */
	void MapTunnelExits(TSharedPtr<FScriptExecutionTunnelEntry> TunnelEntryPoint);

protected:

	/** The graph name */
	FName GraphName;
	/** The function name */
	FName FunctionName;
	/** Indicates this is an inherited function context */
	bool bIsInheritedContext;
	/** The Owning Blueprint context */
	TWeakPtr<FBlueprintExecutionContext> BlueprintContext;
	/** The UFunction this context represents */
	TWeakObjectPtr<UFunction> Function;
	/** The blueprint that owns this function graph */
	TWeakObjectPtr<UBlueprint> OwningBlueprint;
	/** The blueprint generated class for the owning blueprint */
	TWeakObjectPtr<UBlueprintGeneratedClass> BlueprintClass;
	/** ScriptCode offset to node cache */
	TMap<int32, TWeakObjectPtr<const UEdGraphNode>> ScriptOffsetToNodes;
	/** ScriptCode offset to tunnel node cache */
	TMap<TWeakObjectPtr<const UEdGraphNode>, TWeakObjectPtr<const UEdGraphNode>> NodeToTunnelNode;
	/** ScriptCode offset to pin cache */
	TMap<int32, FEdGraphPinReference> ScriptOffsetToPins;
	/** Execution nodes containing profiling data */
	TMap<FName, TSharedPtr<FScriptExecutionNode>> ExecutionNodes;
	/** Other function contexts in use by this context */
	TMap<FName, TWeakPtr<FBlueprintFunctionContext>> ChildFunctionContexts;
	/** Graph entry points */
	TArray<TSharedPtr<FScriptExecutionNode>> EntryPoints;
	/** Graph exit points */
	TArray<TSharedPtr<FScriptExecutionNode>> ExitPoints;
};

//////////////////////////////////////////////////////////////////////////
// FBlueprintTunnelInstanceContext

class FBlueprintTunnelInstanceContext : public FBlueprintFunctionContext
{
public:

	// ~FBlueprintFunctionContext Begin
	virtual void InitialiseContextFromGraph(TSharedPtr<FBlueprintExecutionContext> BlueprintContext, const FName TunnelInstanceName, UEdGraph* TunnelGraph);
	virtual void MapFunction() override {}
	// ~FBlueprintFunctionContext End

	/** Maps a tunnel function context */
	void MapTunnelContext(TSharedPtr<FBlueprintFunctionContext> CallingFunctionContext, UK2Node_Tunnel* TunnelInstance);

	/** Sets the tunnel context parent context */
	void SetParentContext(TSharedPtr<FBlueprintFunctionContext> ParentContextIn);

private:

	friend FBlueprintExecutionContext;

	// ~FBlueprintFunctionContext Begin
	virtual TSharedPtr<FScriptExecutionNode> MapNodeExecution(UEdGraphNode* NodeToMap) override;
	virtual void MapInputPins(TSharedPtr<FScriptExecutionNode> ExecNode, const TArray<UEdGraphPin*>& Pins) override;
	virtual TSharedPtr<FScriptExecutionNode> MapPureNodeExecution(const UEdGraphPin* LinkedPin) override;
	virtual TSharedPtr<FScriptExecutionNode> GetTunnelBoundaryNodeChecked(const UEdGraphPin* TunnelPin) override;
	virtual TSharedPtr<FScriptExecutionNode> MapTunnelBoundary(const UEdGraphPin* TunnelPin) override;
	// ~FBlueprintFunctionContext End
	
	/** Maps input and output pins in a tunnel graph */
	void MapTunnelIO(TMap<UEdGraphPin*, UEdGraphPin*>& PurePinsOut);

	/** Maps pure chain connections once everything else is in place */
	void MapTunnelPureIO(const TMap<UEdGraphPin*, UEdGraphPin*>& PurePinsOut);

	/** Returns true if the node is an internal part of this tunnel */
	bool IsTunnelNodeInternal(const UEdGraphNode* TunnelNode);

	/** Returns true if the pin is part of this tunnel, unless its a tunnel site to another tunnel */
	bool IsPinFromThisTunnel(const UEdGraphPin* TunnelPin) const;

	/** Discovers already mapped exit sites */
	void DiscoverExitSites(TSharedPtr<FScriptExecutionNode> MappedNode);

	/** Looks up a function context using the nodes graph */
	TSharedPtr<FBlueprintFunctionContext> FindContextByNodeGraph(const UEdGraphNode* GraphNode);

private:

	/** Tunnel context that are contained by this tunnel context */
	TWeakPtr<FBlueprintTunnelInstanceContext> ParentTunnel;
	/** The tunnel instance this context represents */
	TWeakObjectPtr<UK2Node_Tunnel> TunnelInstanceNode;
	/** The tunnel pure pin link nodes */
	TMap<FEdGraphPinReference, TSharedPtr<FScriptExecutionNode>> PureLinkNodes;
	/** Staging entry point */
	TSharedPtr<FScriptExecutionTunnelEntry> StagingEntryPoint;

};

//////////////////////////////////////////////////////////////////////////
// FScriptEventPlayback

class FScriptEventPlayback : public TSharedFromThis<FScriptEventPlayback>
{
public:

	struct NodeSignalHelper
	{
		/** Easy validation */
		bool IsValid() const { return ImpureNode.IsValid() && FunctionContext.IsValid(); }

		/** Node signal properties */
		TSharedPtr<FScriptExecutionNode> ImpureNode;
		TMap<int32, TSharedPtr<FScriptExecutionNode>> PureNodes;
		TSharedPtr<FBlueprintFunctionContext> FunctionContext;
		TArray<class FScriptInstrumentedEvent> AverageEvents;
		TArray<class FScriptInstrumentedEvent> InclusiveEvents;
		TArray<FTracePath> AverageTracePaths;
		TArray<FTracePath> InputTracePaths;
		TArray<FTracePath> InclusiveTracePaths;
	};

	struct TunnelEventHelper
	{
		double TunnelEntryTime;
		FTracePath EntryTracePath;
		TSharedPtr<FScriptExecutionTunnelEntry> TunnelEntryPoint;
	};

	struct PushState
	{
		PushState(const UEdGraphNode* NodeIn, const FTracePath& TracePathIn)
			: InstigatingNode(NodeIn)
			, TracePath(TracePathIn)
		{
		}

		const UEdGraphNode* InstigatingNode;
		FTracePath TracePath;
	};

	FScriptEventPlayback(TSharedPtr<FBlueprintExecutionContext> BlueprintContextIn, const FName InstanceNameIn)
		: InstanceName(InstanceNameIn)
		, CurrentFunctionName(NAME_None)
		, LatentLinkId(INDEX_NONE)
		, BlueprintContext(BlueprintContextIn)
	{
	}

	/** Returns if current processing state is suspended */
	bool IsSuspended() const { return LatentLinkId != INDEX_NONE; }

	/** Returns the current latent action UUID */
	const int32 GetLatentLinkId() const { return LatentLinkId; }

	/** Processes the event and cleans up any errant signals */
	bool Process(const TArray<class FScriptInstrumentedEvent>& Events, const int32 Start, const int32 Stop);

	/** Sets the blueprint context for this batch */
	void SetBlueprintContext(TSharedPtr<FBlueprintExecutionContext> BlueprintContextIn) { BlueprintContext = BlueprintContextIn; }

private:

	/** Process tunel boundries */
	void ProcessTunnelBoundary(NodeSignalHelper& CurrentNodeData, const FScriptInstrumentedEvent& CurrSignal);

	/** Process execution sequence */
	void ProcessExecutionSequence(NodeSignalHelper& CurrentNodeData, const FScriptInstrumentedEvent& CurrSignal);

	/** Add to trace history */
	void AddToTraceHistory(const TSharedPtr<FScriptExecutionNode> ProfilerNode, const FScriptInstrumentedEvent& TraceSignal);

private:

	/** Instance name */
	FName InstanceName;
	/** Function name */
	FName CurrentFunctionName;
	/** Event name */
	FName EventName;
	/** Latent Link Id for suspended Event */
	int32 LatentLinkId;
	/** Current tracepath state */
	FTracePath TracePath;
	/** Current tracepath stack */
	TArray<FTracePath> TraceStack;
	/** Current tunnel tracepath stack */
	TArray<FTracePath> TunnelTraceStack;
	/** Active tunnels */
	TMap<FName, TunnelEventHelper> ActiveTunnels;
	/** Event Timings */
	TMultiMap<FName, double> EventTimings;
	/** Blueprint exec context */
	TSharedPtr<FBlueprintExecutionContext> BlueprintContext;

};


