// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TracePath.h"

/**  Execution node flags */
namespace EScriptExecutionNodeFlags
{
	enum Type
	{
		None				= 0x00000000,	// No Flags
		Class				= 0x00000001,	// Class
		Instance			= 0x00000002,	// Instance
		Event				= 0x00000004,	// Event
		CustomEvent			= 0x00000008,	// Custom Event
		Container			= 0x00000007,	// Container type - Class, Instance or Event.
		FunctionCall		= 0x00000010,	// Function Call
		MacroCall			= 0x00000020,	// Macro Call
		CallSite			= 0x00000030,	// Function / Macro call site
		MacroNode			= 0x00000100,	// Macro Node
		ConditionalBranch	= 0x00000200,	// Node has multiple exit pins using a jump
		SequentialBranch	= 0x00000400,	// Node has multiple exit pins ran in sequence
		BranchNode			= 0x00000600,	// BranchNode
		Node				= 0x00000800,	// Node timing
		ExecPin				= 0x00001000,	// Exec pin dummy node
		PureNode			= 0x00002000	// Pure node (no exec pins)
	};
}

/** Stat Container Type */
namespace EScriptStatContainerType
{
	enum Type
	{
		Standard = 0,
		Container,
		SequentialBranch,
		NewExecutionPath
	};
}

//////////////////////////////////////////////////////////////////////////
// FScriptNodeExecLinkage

class KISMET_API FScriptNodeExecLinkage
{
public:

	struct FLinearExecPath
	{
		FLinearExecPath(TSharedPtr<class FScriptExecutionNode> NodeIn, const FTracePath TracePathIn)
			: TracePath(TracePathIn)
			, LinkedNode(NodeIn)
		{
		}

		FTracePath TracePath;
		TSharedPtr<class FScriptExecutionNode> LinkedNode;
	};

	/** Returns the parent node */
	TSharedPtr<class FScriptExecutionNode> GetParentNode() { return ParentNode; }

	/** Sets the parent node */
	void SetParentNode(TSharedPtr<class FScriptExecutionNode> InParentNode) { ParentNode = InParentNode; }

	/** Returns the pure nodes map */
	TMap<int32, TSharedPtr<class FScriptExecutionNode>>& GetPureNodes() { return PureNodes; }

	/** Returns the number of pure nodes */
	int32 GetNumPureNodes() const { return PureNodes.Num(); }

	/** Add pure node */
	void AddPureNode(const int32 PinScriptOffset, TSharedPtr<class FScriptExecutionNode> PureNode);

	/** Returns the linked nodes map */
	TMap<int32, TSharedPtr<class FScriptExecutionNode>>& GetLinkedNodes() { return LinkedNodes; }

	/** Returns the number of linked nodes */
	int32 GetNumLinkedNodes() const { return LinkedNodes.Num(); }

	/** Add linked node */
	void AddLinkedNode(const int32 PinScriptOffset, TSharedPtr<class FScriptExecutionNode> LinkedNode);

	/** Returns the child nodes map */
	TArray<TSharedPtr<class FScriptExecutionNode>>& GetChildNodes() { return ChildNodes; }

	/** Returns the number of children */
	int32 GetNumChildren() const { return ChildNodes.Num(); }

	/** Returns the child node for the specified index */
	TSharedPtr<class FScriptExecutionNode> GetChildByIndex(const int32 ChildIndex) { return ChildNodes[ChildIndex]; }

	/** Add child node */
	void AddChildNode(TSharedPtr<class FScriptExecutionNode> ChildNode) { ChildNodes.Add(ChildNode); }

protected:

	/** Parent node */
	TSharedPtr<class FScriptExecutionNode> ParentNode;
	/** Pure nodes */
	TMap<int32, TSharedPtr<class FScriptExecutionNode>> PureNodes;
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

	/** Test whether or not perf data is available for the given instance/trace path */
	bool HasPerfDataForInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath) const;

	/** Get/add perf data for instance and tracepath */
	TSharedPtr<class FScriptPerfData> GetPerfDataByInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath);

	/** Get all instance perf data for the trace path, excluding the global blueprint data */
	void GetInstancePerfDataByTracePath(const FTracePath& TracePath, TArray<TSharedPtr<FScriptPerfData>>& ResultsOut);

	/** Get global blueprint perf data for the trace path */
	TSharedPtr<FScriptPerfData> GetBlueprintPerfDataByTracePath(const FTracePath& TracePath);

protected:

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
	/** Oberved object */
	TWeakObjectPtr<const UObject> ObservedObject;
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
// FScriptExecutionNode

typedef TSet<TWeakObjectPtr<const UObject>> FExecNodeFilter;

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

	/** Returns if this exec event represents a change in class/blueprint */
	bool IsClass() const { return (NodeFlags & EScriptExecutionNodeFlags::Class) != 0U; }

	/** Returns if this exec event represents a change in instance */
	bool IsInstance() const { return (NodeFlags & EScriptExecutionNodeFlags::Instance) != 0U; }

	/** Returns if this exec event represents the start of an event execution path */
	bool IsEvent() const { return (NodeFlags & EScriptExecutionNodeFlags::Event) != 0U; }

	/** Returns if this exec event represents the start of a custon event execution path */
	bool IsCustomEvent() const { return (NodeFlags & EScriptExecutionNodeFlags::CustomEvent) != 0U; }
	
	/** Returns if this event is a function callsite event */
	bool IsFunctionCallSite() const { return (NodeFlags & EScriptExecutionNodeFlags::FunctionCall) != 0U; }

	/** Returns if this event is a macro callsite event */
	bool IsMacroCallSite() const { return (NodeFlags & EScriptExecutionNodeFlags::MacroCall) != 0U; }

	/** Returns if this event happened inside a macro instance */
	bool IsMacroNode() const { return (NodeFlags & EScriptExecutionNodeFlags::MacroNode) != 0U; }

	/** Returns if this node is also a pure node (i.e. no exec input pin) */
	bool IsPureNode() const { return (NodeFlags & EScriptExecutionNodeFlags::PureNode) != 0U; }

	/** Returns if this event potentially multiple exit sites */
	bool IsBranch() const { return (NodeFlags & EScriptExecutionNodeFlags::BranchNode) != 0U; }

	/** Gets the observed object context */
	const UObject* GetObservedObject() { return ObservedObject.Get(); }

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

	/** Returns the profiler heat color */
	FLinearColor GetHeatColor(const FTracePath& TracePath) const;

	/** Returns the current expansion state for widget UI */
	bool IsExpanded() const { return bExpansionState; }

	/** Sets the current expansion state for widget UI */
	void SetExpanded(bool bIsExpanded) { bExpansionState = bIsExpanded; }

	/** Sets the pure node script code range */
	void SetPureNodeScriptCodeRange(FInt32Range InScriptCodeRange) { PureNodeScriptCodeRange = InScriptCodeRange; }

	/** Return the linear execution path from this node */
	void GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath);

	/** Get statistic container type */
	virtual EScriptStatContainerType::Type GetStatisticContainerType() const;

	/** Refresh Stats */
	virtual void RefreshStats(const FTracePath& TracePath);

	/** Get all exec nodes */
	virtual void GetAllExecNodes(TMap<FName, TSharedPtr<FScriptExecutionNode>>& ExecNodesOut);

	/** Get all pure nodes associated with the given trace path */
	virtual void GetAllPureNodes(TMap<int32, TSharedPtr<FScriptExecutionNode>>& PureNodesOut);

protected:

	/** Get all pure nodes - private implementation */
	void GetAllPureNodes_Internal(TMap<int32, TSharedPtr<FScriptExecutionNode>>& PureNodesOut, const FInt32Range& ScriptCodeRange);

	/** Get linear execution path - private implementation */
	void GetLinearExecutionPath_Internal(FExecNodeFilter& Filter, TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath);

	/** Refresh Stats - private implementation */
	void RefreshStats_Internal(const FTracePath& TracePath, FExecNodeFilter& VisitedStats);

protected:

	/** Node name */
	FName NodeName;
	/** Owning graph name */
	FName OwningGraphName;
	/** Node flags to describe the source graph node type */
	uint32 NodeFlags;
	/** Oberved object */
	TWeakObjectPtr<const UObject> ObservedObject;
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
// FScriptExecutionInstance

class KISMET_API FScriptExecutionInstance : public FScriptExecutionNode
{
public:

	FScriptExecutionInstance(const FScriptExecNodeParams& InitParams)
		: FScriptExecutionNode(InitParams)
	{
	}

	// FScriptExecutionNode
	virtual void NavigateToObject() const override;
	// ~FScriptExecutionNode
};

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionBlueprint

class KISMET_API FScriptExecutionBlueprint : public FScriptExecutionNode
{
public:

	FScriptExecutionBlueprint(const FScriptExecNodeParams& InitParams)
		: FScriptExecutionNode(InitParams)
	{
	}

	/** Adds new blueprint instance node */
	void AddInstance(TSharedPtr<FScriptExecutionNode> Instance) { Instances.Add(Instance); }

	/** Returns the current number of instance nodes */
	int32 GetInstanceCount() { return Instances.Num(); }

	/** Returns the instance node specified by Index */
	TSharedPtr<FScriptExecutionNode> GetInstanceByIndex(const int32 Index) { return Instances[Index]; }

	/** Returns the instance that matches the supplied name if present */
	TSharedPtr<FScriptExecutionNode> GetInstanceByName(FName InstanceName);

	// FScriptExecutionNode
	virtual EScriptStatContainerType::Type GetStatisticContainerType() const override { return EScriptStatContainerType::Container; }
	virtual void RefreshStats(const FTracePath& TracePath) override;
	virtual void GetAllExecNodes(TMap<FName, TSharedPtr<FScriptExecutionNode>>& ExecNodesOut) override;
	virtual void NavigateToObject() const override;
	// ~FScriptExecutionNode

private:

	/** Exec nodes representing all instances based on this blueprint */
	TArray<TSharedPtr<FScriptExecutionNode>> Instances;

};

