// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TracePath.h"
#include "ScriptPerfData.h"

/**  Execution node flags */
namespace EScriptExecutionNodeFlags
{
	enum Type
	{
		None						= 0x00000000,	// No Flags
		Class						= 0x00000001,	// Class
		Instance					= 0x00000002,	// Instance
		Event						= 0x00000004,	// Event
		CustomEvent					= 0x00000008,	// Custom Event
		InheritedEvent				= 0x00000010,	// Inherited Event
		FunctionCall				= 0x00000020,	// Function Call
		ParentFunctionCall			= 0x00000040,	// Parent Function Call
		MacroCall					= 0x00000080,	// Macro Call
		MacroNode					= 0x00000100,	// Macro Node
		ConditionalBranch			= 0x00000200,	// Node has multiple exit pins using a jump
		SequentialBranch			= 0x00000400,	// Node has multiple exit pins ran in sequence
		Node						= 0x00000800,	// Node timing
		ExecPin						= 0x00001000,	// Exec pin dummy node
		PureNode					= 0x00002000,	// Pure node (no exec pins)
		EventPin					= 0x00004000,	// Delegate pin entry point
		TunnelInstance				= 0x00008000,	// Tunnel instance
		TunnelEntryPin				= 0x00010000,	// Internal tunnel entry pin
		TunnelExitPin				= 0x00020000,	// Internal tunnel exit pin
		TunnelEntryPinInstance		= 0x00040000,	// Tunnel entry pin instance
		TunnelExitPinInstance		= 0x00080000,	// Tunnel exit pin instance
		PureChain					= 0x00100000,	// Pure node call chain
		CyclicLinkage				= 0x00200000,	// Marks execution path as cyclic.
		InvalidTrace				= 0x00400000,	// Indicates that node doesn't contain a valid script trace.
		// Groups
		CallSite					= FunctionCall|ParentFunctionCall|MacroCall|TunnelInstance,
		BranchNode					= ConditionalBranch|SequentialBranch,
		TunnelInstancePin			= TunnelEntryPinInstance|TunnelExitPinInstance,
		TunnelPin					= TunnelEntryPin|TunnelExitPin,
		TunnelBoundary				= TunnelInstancePin|TunnelPin,
		TunnelEntry					= TunnelEntryPin|TunnelEntryPinInstance,
		TunnelExit					= TunnelExitPin|TunnelExitPinInstance,
		PureStats					= PureNode|PureChain,
		Container					= Class|Instance|Event|CustomEvent|ExecPin|PureChain
	};
}

//////////////////////////////////////////////////////////////////////////
// Stat Container Type

namespace EScriptStatContainerType
{
	enum Type
	{
		Standard = 0,
		Container,
		SequentialBranch,
		NewExecutionPath,
		PureNode
	};
}

//////////////////////////////////////////////////////////////////////////
// FScriptNodeExecLinkage

class KISMET_API FScriptNodeExecLinkage
{
public:

	struct FLinearExecPath
	{
		FLinearExecPath(TSharedPtr<class FScriptExecutionNode> NodeIn, const FTracePath TracePathIn, const bool bIncludeChildrenIn = false)
			: TracePath(TracePathIn)
			, LinkedNode(NodeIn)
			, bIncludeChildren(bIncludeChildrenIn)
		{
		}

		FTracePath TracePath;
		TSharedPtr<class FScriptExecutionNode> LinkedNode;
		bool bIncludeChildren;
	};

	/** Returns the linked nodes map */
	TMap<int32, TSharedPtr<class FScriptExecutionNode>>& GetLinkedNodes() { return LinkedNodes; }

	/** Returns the number of linked nodes */
	int32 GetNumLinkedNodes() const { return LinkedNodes.Num(); }

	/** Add linked node */
	void AddLinkedNode(const int32 PinScriptOffset, TSharedPtr<class FScriptExecutionNode> LinkedNode);

	/** Returns linked node by matching script offset */
	TSharedPtr<class FScriptExecutionNode> GetLinkedNodeByScriptOffset(const int32 PinScriptOffset);

	/** Returns the child nodes map */
	TArray<TSharedPtr<class FScriptExecutionNode>>& GetChildNodes() { return ChildNodes; }

	/** Returns the number of children */
	int32 GetNumChildren() const { return ChildNodes.Num(); }

	/** Returns the child node for the specified index */
	TSharedPtr<class FScriptExecutionNode> GetChildByIndex(const int32 ChildIndex) { return ChildNodes[ChildIndex]; }

	/** Add child node */
	void AddChildNode(TSharedPtr<class FScriptExecutionNode> ChildNode) { ChildNodes.Add(ChildNode); }

protected:

	/** Linked nodes */
	TMap<int32, TSharedPtr<class FScriptExecutionNode>> LinkedNodes;
	/** Child nodes */
	TArray<TSharedPtr<class FScriptExecutionNode>> ChildNodes;
};

//////////////////////////////////////////////////////////////////////////
// FScriptNodePerfData

class KISMET_API FScriptNodePerfData
{
public:

	FScriptNodePerfData(const int32 SampleFreqIn = 1)
		: SampleFrequency(SampleFreqIn)
	{
	}

	/** Returns a TSet containing all valid instance names */
	void GetValidInstanceNames(TSet<FName>& ValidInstances) const;

	/** Test whether or not perf data is available for the given instance/trace path */
	bool HasPerfDataForInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath) const;

	/** Get or add perf data for instance and tracepath */
	TSharedPtr<class FScriptPerfData> GetOrAddPerfDataByInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath);

	/** Get perf data for instance and tracepath */
	TSharedPtr<class FScriptPerfData> GetPerfDataByInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath);

	/** Get all instance perf data for the trace path, excluding the global blueprint data */
	void GetInstancePerfDataByTracePath(const FTracePath& TracePath, TArray<TSharedPtr<FScriptPerfData>>& ResultsOut);

	/** Get or add global blueprint perf data for the trace path */
	TSharedPtr<FScriptPerfData> GetOrAddBlueprintPerfDataByTracePath(const FTracePath& TracePath);

	/** Get global blueprint perf data for the trace path */
	TSharedPtr<FScriptPerfData> GetBlueprintPerfDataByTracePath(const FTracePath& TracePath);

	/** Get global blueprint perf data for all trace paths */
	virtual void GetBlueprintPerfDataForAllTracePaths(FScriptPerfData& OutPerfData);

	/** Set the sample frequency */
	void SetSampleFrequency(const int32 NewSampleFrequency) { SampleFrequency = NewSampleFrequency; }

protected:

	/** Returns performance data type */
	virtual const EScriptPerfDataType GetPerfDataType() const { return EScriptPerfDataType::Node; }

protected:
	
	/** Expected sample frequency for this perf data container */
	int32 SampleFrequency;
	/** FScriptExeutionPath hash to perf data */
	TMap<FName, TMap<const uint32, TSharedPtr<FScriptPerfData>>> InstanceInputPinToPerfDataMap;

};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionNodeParams

struct KISMET_API FScriptExecNodeParams
{
	/** Node name */
	FName NodeName;
	/** Owning graph name */
	FName OwningGraphName;
	/** Node flags to describe the source graph node type */
	uint32 NodeFlags;
	/** The expected sample frequency that constitutes a singles execution of this node */
	int32 SampleFrequency;
	/** Oberved object */
	TWeakObjectPtr<const UObject> ObservedObject;
	/** Observed pin */
	FEdGraphPinReference ObservedPin;
	/** Display name for widget UI */
	FText DisplayName;
	/** Tooltip for widget UI */
	FText Tooltip;
	/** Icon color for widget UI */
	FLinearColor IconColor;
	/** Icon for widget UI */
	FSlateBrush* Icon;
};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionHottestPathParams

struct FScriptExecutionHottestPathParams
{
	FScriptExecutionHottestPathParams(const FName InstanceNameIn, const float EventTimingIn)
		: InstanceName(InstanceNameIn)
		, EventTiming(EventTimingIn)
		, TimeSoFar(0.0)
	{
	}

	/** Instance name */
	const FName InstanceName;
	/** Event total timing */
	const double EventTiming;
	/** Event time so far */
	double TimeSoFar;
	/** Current tracepath */
	FTracePath TracePath;
};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionNode

class KISMET_API FScriptExecutionNode : public FScriptNodeExecLinkage, public FScriptNodePerfData, public TSharedFromThis<FScriptExecutionNode>
{
public:

	FScriptExecutionNode();
	FScriptExecutionNode(const FScriptExecNodeParams& InitParams);
    virtual ~FScriptExecutionNode() {}

	bool operator == (const FScriptExecutionNode& NodeIn) const;

	/** Get the node's name */
	FName GetName() const { return NodeName; }

	/** Returns the owning graph name */
	FName GetGraphName() const { return OwningGraphName; }

	/** Add to the node's flags */
	void AddFlags(const uint32 NewFlags) { NodeFlags |= NewFlags; }

	/** Remove from the node's flags */
	void RemoveFlags(const uint32 NewFlags) { NodeFlags &= ~NewFlags; }

	/** Does the node contain these flags */
	bool HasFlags(const uint32 Flags) const { return (NodeFlags & Flags) != 0U; }

	/** Returns the current node flags */
	uint32 GetFlags() const { return NodeFlags; }

	/** Returns if this exec event represents a change in class/blueprint */
	bool IsClass() const { return (NodeFlags & EScriptExecutionNodeFlags::Class) != 0U; }

	/** Returns if this exec event represents a change in instance */
	bool IsInstance() const { return (NodeFlags & EScriptExecutionNodeFlags::Instance) != 0U; }

	/** Returns if this exec event represents the start of an event execution path */
	bool IsEvent() const { return (NodeFlags & EScriptExecutionNodeFlags::Event) != 0U; }

	/** Returns if this exec event represents the start of a custon event execution path */
	bool IsCustomEvent() const { return (NodeFlags & EScriptExecutionNodeFlags::CustomEvent) != 0U; }

	/** Returns if this exec event represents the start of a delegate pin event execution path */
	bool IsEventPin() const { return (NodeFlags & EScriptExecutionNodeFlags::EventPin) != 0U; }

	/** Returns if this event is a function callsite event */
	bool IsFunctionCallSite() const { return (NodeFlags & EScriptExecutionNodeFlags::FunctionCall) != 0U; }

	/** Returns if this event is a parent function callsite event */
	bool IsParentFunctionCallSite() const { return (NodeFlags & EScriptExecutionNodeFlags::ParentFunctionCall) != 0U; }

	/** Returns if this event is a macro callsite event */
	bool IsMacroCallSite() const { return (NodeFlags & EScriptExecutionNodeFlags::MacroCall) != 0U; }

	/** Returns if this event happened inside a macro instance */
	bool IsMacroNode() const { return (NodeFlags & EScriptExecutionNodeFlags::MacroNode) != 0U; }

	/** Returns if this node is also a pure node (i.e. no exec input pin) */
	bool IsPureNode() const { return (NodeFlags & EScriptExecutionNodeFlags::PureNode) != 0U; }

	/** Returns if this node is a pure chain node */
	bool IsPureChain() const { return (NodeFlags & EScriptExecutionNodeFlags::PureChain) != 0U; }

	/** Returns if this event potentially has multiple exit sites */
	bool IsBranch() const { return (NodeFlags & EScriptExecutionNodeFlags::BranchNode) != 0U; }

	/** Returns if this event represents a tunnel Boundary */
	bool IsTunnelBoundary() const { return (NodeFlags & EScriptExecutionNodeFlags::TunnelBoundary) != 0U; }
	
	/** Returns if this event represents a tunnel instance */
	bool IsTunnelInstance() const { return (NodeFlags & EScriptExecutionNodeFlags::TunnelInstance) != 0U; }

	/** Returns if this event represents a tunnel entry */
	bool IsTunnelEntry() const { return (NodeFlags & EScriptExecutionNodeFlags::TunnelEntryPinInstance) != 0U; }

	/** Returns if this event represents a tunnel exit */
	bool IsTunnelExit() const { return (NodeFlags & EScriptExecutionNodeFlags::TunnelExit) != 0U; }

	/** Returns if this exec node can be safely cyclicly linked */
	bool CanLinkAsCyclicNode() const { return (NodeFlags & EScriptExecutionNodeFlags::CallSite) != 0U; }

	/** Returns true if this node is cyclicly linked */
	bool HasCyclicLinkage() const { return (NodeFlags & EScriptExecutionNodeFlags::CyclicLinkage) != 0U; }

	/** Gets the observed object context */
	const UObject* GetObservedObject() { return ObservedObject.Get(); }

	/** Gets a typed observed object context */
	template<typename CastType> const CastType* GetTypedObservedObject() { return Cast<CastType>(ObservedObject.Get()); }

	/** Gets the observed object context */
	const UEdGraphPin* GetObservedPin() const { return ObservedPin.Get(); }

	/** Navigate to object */
	virtual void NavigateToObject() const;

	/** Returns the display name for widget UI */
	const FText& GetDisplayName() const { return DisplayName; }

	/** Returns the tooltip for widget UI */
	const FText& GetToolTipText() const { return Tooltip; }

	/** Sets the tooltip for widget UI */
	void SetToolTipText(const FText InTooltip) { Tooltip = InTooltip; }

	/** Returns the icon color for widget UI */
	const FLinearColor& GetIconColor() const { return IconColor; }

	/** Sets the icon color for widget UI */
	void SetIconColor(FLinearColor InIconColor) { IconColor = InIconColor; }

	/** Returns the icon for widget UI */
	const FSlateBrush* GetIcon() const { return Icon; }

	/** Returns the current expansion state for widget UI */
	bool IsExpanded() const { return bExpansionState; }

	/** Sets the current expansion state for widget UI */
	void SetExpanded(bool bIsExpanded) { bExpansionState = bIsExpanded; }

	/** Returns the pure chain node associated with this exec node (if one exists) */
	virtual TSharedPtr<FScriptExecutionNode> GetPureChainNode();

	/** Returns pure node script code range */
	FInt32Range GetPureNodeScriptCodeRange() const { return PureNodeScriptCodeRange; }

	/** Sets the pure node script code range */
	void SetPureNodeScriptCodeRange(FInt32Range InScriptCodeRange) { PureNodeScriptCodeRange = InScriptCodeRange; }

	/** Return the linear execution path from this node */
	virtual void GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath, const bool bIncludeChildren = false);

	/** Get statistic container type */
	virtual EScriptStatContainerType::Type GetStatisticContainerType() const;

	/** Refresh Stats */
	virtual void RefreshStats(const FTracePath& TracePath);

	/** Calculate heat level stats */
	virtual void CalculateHeatLevelStats(TSharedPtr<FScriptHeatLevelMetrics> HeatLevelMetrics);

	/** Calculate Hottest Path Stats */
	virtual float CalculateHottestPathStats(FScriptExecutionHottestPathParams HotPathParams);

	/** Get all exec nodes */
	virtual void GetAllExecNodes(TMap<FName, TSharedPtr<FScriptExecutionNode>>& ExecNodesOut);

	/** Get all pure nodes associated with the given trace path */
	void GetAllPureNodes(TMap<int32, TSharedPtr<FScriptExecutionNode>>& PureNodesOut);

	/** Creates and returns the slate icon widget */
	virtual TSharedRef<SWidget> GetIconWidget(const uint32 TracePath = 0U);

	/** Creates and returns the slate hyperlink widget */
	virtual TSharedRef<SWidget> GetHyperlinkWidget(const uint32 TracePath = 0U);

protected:

	/** Returns if the observed object is valid */
	virtual bool IsObservedObjectValid() const;

	/** Returns Tunnel Linear Execution Trace */
	void MapTunnelLinearExecution(FTracePath& TraceInOut) const;

	/** Get all pure nodes - private implementation */
	void GetAllPureNodes_Internal(TMap<int32, TSharedPtr<FScriptExecutionNode>>& PureNodesOut, const FInt32Range& ScriptCodeRange);

	// FScriptNodePerfData
	virtual const EScriptPerfDataType GetPerfDataType() const override;
	// ~FScriptNodePerfData

protected:

	/** Node name */
	FName NodeName;
	/** Owning graph name */
	FName OwningGraphName;
	/** Node flags to describe the source graph node type */
	uint32 NodeFlags;
	/** Observed object */
	TWeakObjectPtr<const UObject> ObservedObject;
	/** Observed pin */
	FEdGraphPinReference ObservedPin;
	/** Display name for widget UI */
	FText DisplayName;
	/** Tooltip for widget UI */
	FText Tooltip;
	/** Icon color for widget UI */
	FLinearColor IconColor;
	/** Icon for widget UI */
	FSlateBrush* Icon;
	/** Expansion state */
	bool bExpansionState;
	/** Script code range for pure node linkage */
	FInt32Range PureNodeScriptCodeRange;
};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionTunnelInstance

class KISMET_API FScriptExecutionTunnelInstance : public FScriptExecutionNode
{
public:

	FScriptExecutionTunnelInstance(const FScriptExecNodeParams& InitParams)
		: FScriptExecutionNode(InitParams)
	{
	}

	// FScriptNodePerfData
	virtual void GetBlueprintPerfDataForAllTracePaths(FScriptPerfData& OutPerfData) override;
	// ~FScriptNodePerfData

	/** Adds an entry site at the specified script offset */
	void AddEntrySite(const int32 ScriptOffset, TSharedPtr<class FScriptExecutionTunnelEntry> EntrySite);

	/** Adds an entry site at the specified script offset */
	void AddExitSite(const int32 ScriptOffset, TSharedPtr<class FScriptExecutionTunnelExit> ExitSite) { ExitSites.FindOrAdd(ScriptOffset) = ExitSite; }

	/** Copies tunnel Boundary sites  */
	void CopyBoundarySites(TSharedPtr<FScriptExecutionTunnelInstance> OtherInstance);

	/** Find a tunnel Boundary node by script offset */
	TSharedPtr<FScriptExecutionNode> FindBoundarySite(const int32 ScriptOffset);

private:

	/** List of entry sites by script offset */
	TMap<int32, TSharedPtr<FScriptExecutionTunnelEntry>> EntrySites;
	/** List of exit sites by script offset */
	TMap<int32, TSharedPtr<FScriptExecutionTunnelExit>> ExitSites;

};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionTunnelEntry

class KISMET_API FScriptExecutionTunnelEntry : public FScriptExecutionNode
{
public:

	FScriptExecutionTunnelEntry(const FScriptExecNodeParams& InitParams)
		: FScriptExecutionNode(InitParams)
		, TunnelEntryCount(0)
	{
	}

	// FScriptExecutionNode
	virtual void GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath, const bool bIncludeChildren = false) override;
	virtual void RefreshStats(const FTracePath& TracePath) override;
	// ~FScriptExecutionNode

	/** Add tunnel timing */
	void AddTunnelTiming(const FName InstanceName, const FTracePath& TracePath, const FTracePath& InternalTracePath, const int32 ExitScriptOffset, const double Time);

	/** Returns the tunnel instance type */
	bool IsComplexTunnel() const { return LinkedNodes.Num() > 1; }

	/** Returns the tunnel instance exec node */
	TSharedPtr<FScriptExecutionTunnelInstance> GetTunnelInstance() const { return TunnelInstance; }

	/** Sets the tunnel instance exec node */
	void SetTunnelInstance(TSharedPtr<FScriptExecutionTunnelInstance> TunnelInstanceIn) { TunnelInstance = TunnelInstanceIn; }

	/** Returns the exit point associated with the exit script offset */
	TSharedPtr<FScriptExecutionTunnelExit> GetExitSite(const int32 ScriptOffset) const;

	/** Returns true if the supplied node is inside this tunnel */
	bool IsInternalBoundary(TSharedPtr<FScriptExecutionNode> PotentialBoundaryNode) const;

	/** Increment tunnel entry count */
	void IncrementTunnelEntryCount() { TunnelEntryCount++; }

private:

	/** The tunnel entry count */
	int32 TunnelEntryCount;
	/** The tunnel instance this node represents */
	TSharedPtr<FScriptExecutionTunnelInstance> TunnelInstance;

};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionTunnelExit

struct FTunnelExitInstanceData
{
	FTunnelExitInstanceData()
		: InstanceIcon(nullptr)
	{
	}

	FTunnelExitInstanceData(const FSlateBrush* PinIconIn, UEdGraphPin* InstancePinIn)
		: InstanceIcon(PinIconIn)
		, InstancePin(InstancePinIn)
	{
	}

	/** Instance specific icon */
	const FSlateBrush* InstanceIcon;
	/** Instance specific pin */
	FEdGraphPinReference InstancePin;
};

class KISMET_API FScriptExecutionTunnelExit : public FScriptExecutionNode
{
public:

	FScriptExecutionTunnelExit(const FScriptExecNodeParams& InitParams)
		: FScriptExecutionNode(InitParams)
	{
	}

	// FScriptExecutionNode
	virtual void GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath, const bool bIncludeChildren = false) override;
	virtual void RefreshStats(const FTracePath& TracePath) override;
	virtual TSharedRef<SWidget> GetIconWidget(const uint32 TracePath = 0U) override;
	virtual TSharedRef<SWidget> GetHyperlinkWidget(const uint32 TracePath = 0U) override;
	// ~FScriptExecutionNode

	/** Navigate to exit site using tracepath */
	virtual void NavigateToExitSite(const uint32 TracePath) const;

	/** Add Instance exit pin */
	void AddInstanceExitSite(const uint32 TracePath, UEdGraphPin* InstanceExitPin);

	/** Returns the external tunnel pin */
	UEdGraphPin* GetExternalPin() const { return ExternalPin.Get(); }

	/** Sets the external tunnel pin */
	void SetExternalPin(UEdGraphPin* ExternalPinIn) { ExternalPin = ExternalPinIn; }

private:

	/** The internal tunnel pin this node represents */
	FEdGraphPinReference ExternalPin;
	/** Instance specific exit sites */
	TMap<const uint32, FTunnelExitInstanceData> InstanceExitSiteInfo;

};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionPureChainNode

class KISMET_API FScriptExecutionPureChainNode : public FScriptExecutionNode
{
public:

	FScriptExecutionPureChainNode(const FScriptExecNodeParams& InitParams)
		: FScriptExecutionNode(InitParams)
	{
	}

	// FScriptExecutionNode
	virtual TSharedPtr<FScriptExecutionNode> GetPureChainNode() override { return AsShared(); }
	virtual void GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath, const bool bIncludeChildren = false) override;
	virtual void RefreshStats(const FTracePath& TracePath) override;
	virtual float CalculateHottestPathStats(FScriptExecutionHottestPathParams HotPathParams) override;
	virtual void CalculateHeatLevelStats(TSharedPtr<FScriptHeatLevelMetrics> HeatLevelMetrics);
	// ~FScriptExecutionNode

};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionInstance

class KISMET_API FScriptExecutionInstance : public FScriptExecutionNode
{
public:

	FScriptExecutionInstance(const FScriptExecNodeParams& InitParams)
		: FScriptExecutionNode(InitParams)
		, bPIEInstanceDestroyed(false)
	{
	}

	// FScriptExecutionNode
	virtual void NavigateToObject() const override;
	virtual TSharedRef<SWidget> GetIconWidget(const uint32 TracePath = 0U) override;
	virtual TSharedRef<SWidget> GetHyperlinkWidget(const uint32 TracePath = 0U) override;
	virtual bool IsObservedObjectValid() const override;
	// ~FScriptExecutionNode

	/** Refresh PIE Instance */
	void RefreshPIEInstance() const;

	/** Returns the active object instance - PIE Instance if in PIE, otherwise editor instance */
	UObject* GetActiveObject() const;

private:

	/** Return the current instance icon color */
	FSlateColor GetInstanceIconColor() const;

	/** Return the current instance icon color */
	FText GetInstanceTooltip() const;

private:

	/** PIE instance */
	mutable TWeakObjectPtr<const UObject> PIEInstance;
	/** Indicates the PIE instance has been detected as destroyed, so we don't keep refreshing */
	mutable bool bPIEInstanceDestroyed;

};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionBlueprint

class KISMET_API FScriptExecutionBlueprint : public FScriptExecutionNode
{
public:

	FScriptExecutionBlueprint(const FScriptExecNodeParams& InitParams);

	/** Adds new blueprint instance node */
	void AddInstance(TSharedPtr<FScriptExecutionNode> Instance) { Instances.Add(Instance); }

	/** Returns the current number of instance nodes */
	int32 GetInstanceCount() { return Instances.Num(); }

	/** Returns the instance node specified by Index */
	TSharedPtr<FScriptExecutionNode> GetInstanceByIndex(const int32 Index) { return Instances[Index]; }

	/** Returns the instance that matches the supplied name if present */
	TSharedPtr<FScriptExecutionNode> GetInstanceByName(FName InstanceName);

	/** Sorts all contained events */
	void SortEvents();
	
	/** Returns the heat level metrics object that's used for updating heat levels */
	TSharedPtr<FScriptHeatLevelMetrics> GetHeatLevelMetrics() const { return HeatLevelMetrics; }

	// FScriptExecutionNode
	virtual EScriptStatContainerType::Type GetStatisticContainerType() const override { return EScriptStatContainerType::Container; }
	virtual void RefreshStats(const FTracePath& TracePath) override;
	virtual void GetAllExecNodes(TMap<FName, TSharedPtr<FScriptExecutionNode>>& ExecNodesOut) override;
	virtual void NavigateToObject() const override;
	// ~FScriptExecutionNode

protected:
	/** Updates internal heat level metrics data */
	void UpdateHeatLevelMetrics(TSharedPtr<FScriptPerfData> BlueprintData);

private:

	/** Exec nodes representing all instances based on this blueprint */
	TArray<TSharedPtr<FScriptExecutionNode>> Instances;
	/** Internal metrics data used for calculating perf data heat levels */
	TSharedPtr<FScriptHeatLevelMetrics> HeatLevelMetrics;
};

