// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintProfilerSettings.h"
#include "EventExecution.h"
#include "SHyperlink.h"

// Debugging defines
#define STRICT_PERFDATA_CREATION 0		// Asserts when script perf data is created from an incorrect tracepath.
#define DISPLAY_NODENAMES_IN_TITLES 0 	// Displays full node names in the stat widget titles for debugging.
#define DISPLAY_NODENAMES_ON_TOOLTIPS 0 // Displays full node names in the stat widget tooltips for debugging.

// Editor profiler defines
DECLARE_STATS_GROUP(TEXT("BlueprintProfiler"), STATGROUP_BlueprintProfiler, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Calculate Hottest Path"), STAT_CalculateHottestPathCost, STATGROUP_BlueprintProfiler);
DECLARE_CYCLE_STAT(TEXT("Calculate Heat Levels"), STAT_CalculateHeatLevelsCost, STATGROUP_BlueprintProfiler);

#define LOCTEXT_NAMESPACE "BlueprintEventUI"

//////////////////////////////////////////////////////////////////////////
// FScriptNodeExecLinkage

void FScriptNodeExecLinkage::AddLinkedNode(const int32 PinScriptOffset, TSharedPtr<class FScriptExecutionNode> LinkedNode)
{
	TSharedPtr<class FScriptExecutionNode>& Result = LinkedNodes.FindOrAdd(PinScriptOffset);
	Result = LinkedNode;
}

TSharedPtr<FScriptExecutionNode> FScriptNodeExecLinkage::GetLinkedNodeByScriptOffset(const int32 PinScriptOffset)
{
	TSharedPtr<FScriptExecutionNode> Result;
	if (TSharedPtr<FScriptExecutionNode>* SearchResult = LinkedNodes.Find(PinScriptOffset))
	{
		Result = *SearchResult;
	}
	return Result;
}

//////////////////////////////////////////////////////////////////////////
// FScriptNodePerfData

void FScriptNodePerfData::GetValidInstanceNames(TSet<FName>& ValidInstances) const
{
	for (auto InstanceIter : InstanceInputPinToPerfDataMap)
	{
		if (InstanceIter.Key != NAME_None)
		{
			ValidInstances.Add(InstanceIter.Key);
		}
	}
}

bool FScriptNodePerfData::HasPerfDataForInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath) const
{
	const TMap<const uint32, TSharedPtr<FScriptPerfData>>* InstanceMapPtr = InstanceInputPinToPerfDataMap.Find(InstanceName);
	if (InstanceMapPtr)
	{
		return InstanceMapPtr->Contains(TracePath);
	}

	return false;
}

TSharedPtr<FScriptPerfData> FScriptNodePerfData::GetOrAddPerfDataByInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath)
{ 
	TMap<const uint32, TSharedPtr<FScriptPerfData>>& InstanceMap = InstanceInputPinToPerfDataMap.FindOrAdd(InstanceName);
	TSharedPtr<FScriptPerfData>& Result = InstanceMap.FindOrAdd(TracePath);
	if (!Result.IsValid())
	{
		Result = MakeShareable<FScriptPerfData>(new FScriptPerfData(GetPerfDataType(), SampleFrequency));
	}
	return Result;
}

TSharedPtr<FScriptPerfData> FScriptNodePerfData::GetPerfDataByInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath)
{ 
	TMap<const uint32, TSharedPtr<FScriptPerfData>>& InstanceMap = InstanceInputPinToPerfDataMap.FindOrAdd(InstanceName);
	#if STRICT_PERFDATA_CREATION
	TSharedPtr<FScriptPerfData>* Result = InstanceMap.Find(TracePath);
	check (Result != nullptr);
	return *Result;
	#else
	TSharedPtr<FScriptPerfData>& Result = InstanceMap.FindOrAdd(TracePath);
	if (!Result.IsValid())
	{
		Result = MakeShareable<FScriptPerfData>(new FScriptPerfData(GetPerfDataType(), SampleFrequency));
	}
	return Result;
	#endif
}

void FScriptNodePerfData::GetInstancePerfDataByTracePath(const FTracePath& TracePath, TArray<TSharedPtr<FScriptPerfData>>& ResultsOut)
{
	for (auto InstanceIter : InstanceInputPinToPerfDataMap)
	{
		if (InstanceIter.Key != NAME_None)
		{
			TSharedPtr<FScriptPerfData> Result = InstanceIter.Value.FindOrAdd(TracePath);
			if (Result.IsValid())
			{
				ResultsOut.Add(Result);
			}
		}
	}
}

TSharedPtr<FScriptPerfData> FScriptNodePerfData::GetOrAddBlueprintPerfDataByTracePath(const FTracePath& TracePath)
{
	TMap<const uint32, TSharedPtr<FScriptPerfData>>& BlueprintMap = InstanceInputPinToPerfDataMap.FindOrAdd(NAME_None);
	TSharedPtr<FScriptPerfData>& Result = BlueprintMap.FindOrAdd(TracePath);
	if (!Result.IsValid())
	{
		Result = MakeShareable<FScriptPerfData>(new FScriptPerfData(GetPerfDataType(), SampleFrequency));
	}
	return Result;
}

TSharedPtr<FScriptPerfData> FScriptNodePerfData::GetBlueprintPerfDataByTracePath(const FTracePath& TracePath)
{
	#if STRICT_PERFDATA_CREATION
	TMap<const uint32, TSharedPtr<FScriptPerfData>>& BlueprintMap = InstanceInputPinToPerfDataMap.FindOrAdd(NAME_None);
	TSharedPtr<FScriptPerfData>* Result = BlueprintMap.Find(TracePath);
	check (Result != nullptr);
	return *Result;
	#else
	TMap<const uint32, TSharedPtr<FScriptPerfData>>& BlueprintMap = InstanceInputPinToPerfDataMap.FindOrAdd(NAME_None);
	TSharedPtr<FScriptPerfData>& Result = BlueprintMap.FindOrAdd(TracePath);
	if (!Result.IsValid())
	{
		Result = MakeShareable<FScriptPerfData>(new FScriptPerfData(GetPerfDataType(), SampleFrequency));
	}
	return Result;
	#endif
}

void FScriptNodePerfData::GetBlueprintPerfDataForAllTracePaths(FScriptPerfData& OutPerfData)
{
	OutPerfData.Reset();

	TMap<const uint32, TSharedPtr<FScriptPerfData>>& BlueprintMap = InstanceInputPinToPerfDataMap.FindOrAdd(NAME_None);
	for (auto MapIt = BlueprintMap.CreateIterator(); MapIt; ++MapIt)
	{
		TSharedPtr<FScriptPerfData> CurDataPtr = MapIt.Value();
		if (CurDataPtr.IsValid())
		{
			OutPerfData.AddData(*CurDataPtr);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionNode

FScriptExecutionNode::FScriptExecutionNode()
	: FScriptNodePerfData(1)
	, NodeFlags(EScriptExecutionNodeFlags::None)
	, bExpansionState(false)
{
}

FScriptExecutionNode::FScriptExecutionNode(const FScriptExecNodeParams& InitParams)
	: FScriptNodePerfData(InitParams.SampleFrequency)
	, NodeName(InitParams.NodeName)
	, OwningGraphName(InitParams.OwningGraphName)
	, NodeFlags(InitParams.NodeFlags)
	, ObservedObject(InitParams.ObservedObject)
	, ObservedPin(InitParams.ObservedPin)
	, DisplayName(InitParams.DisplayName)
#if DISPLAY_NODENAMES_ON_TOOLTIPS
	, Tooltip(FText::FromName(InitParams.NodeName))
#else
	, Tooltip(InitParams.Tooltip)
#endif
	, IconColor(InitParams.IconColor)
	, Icon(InitParams.Icon)
	, bExpansionState(false)
{
}

bool FScriptExecutionNode::operator == (const FScriptExecutionNode& NodeIn) const
{
	return NodeName == NodeIn.NodeName;
}

TSharedRef<SWidget> FScriptExecutionNode::GetIconWidget(const uint32 /*TracePath*/)
{
	return SNew(SImage)
			.Image(Icon)
			.ColorAndOpacity(IconColor);
}

TSharedRef<SWidget> FScriptExecutionNode::GetHyperlinkWidget(const uint32 /*TracePath*/)
{
	return SNew(SHyperlink)
		#if DISPLAY_NODENAMES_IN_TITLES
		.Text(FText::FromName(NodeName))
		#else
		.Text(DisplayName)
		#endif
		.Style(FEditorStyle::Get(), "BlueprintProfiler.Node.Hyperlink")
		.ToolTipText(Tooltip)
		.OnNavigate(this, &FScriptExecutionNode::NavigateToObject);
}

void FScriptExecutionNode::GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath, const bool bIncludeChildren)
{
	LinearExecutionNodes.Add(FLinearExecPath(AsShared(), TracePath));
	if (bIncludeChildren)
	{
		for (auto Child : ChildNodes)
		{
			FTracePath ChildTracePath(TracePath);
			Child->GetLinearExecutionPath(LinearExecutionNodes, ChildTracePath, bIncludeChildren);
		}
	}
	if (bIncludeChildren || GetNumLinkedNodes() == 1)
	{
		for (auto NodeIter : LinkedNodes)
		{
			if (HasFlags(EScriptExecutionNodeFlags::PureStats))
			{
				continue;
			}
			else
			{
				FTracePath NewTracePath(TracePath);
				if (NodeIter.Value->HasFlags(EScriptExecutionNodeFlags::EventPin))
				{
					NewTracePath.ResetPath();
				}
				if (NodeIter.Key != INDEX_NONE && !HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
				{
					NewTracePath.AddExitPin(NodeIter.Key);
				}
				NodeIter.Value->GetLinearExecutionPath(LinearExecutionNodes, NewTracePath, bIncludeChildren);
			}
		}
	}
}

void FScriptExecutionNode::MapTunnelLinearExecution(FTracePath& Trace) const
{
	if (HasFlags(EScriptExecutionNodeFlags::TunnelInstance))
	{
		for (auto ChildIter : ChildNodes)
		{
			ChildIter->MapTunnelLinearExecution(Trace);
		}
	}
	else if (GetNumLinkedNodes() == 1)
	{
		for (auto NodeIter : LinkedNodes)
		{
			if (!NodeIter.Value->HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
			{
				Trace.AddExitPin(NodeIter.Key);
			}
			NodeIter.Value->MapTunnelLinearExecution(Trace);
		}
	}
}

EScriptStatContainerType::Type FScriptExecutionNode::GetStatisticContainerType() const
{
	EScriptStatContainerType::Type Result = EScriptStatContainerType::Standard;
	if (IsPureNode())
	{
		Result = EScriptStatContainerType::PureNode;
	}
	else if (HasFlags(EScriptExecutionNodeFlags::ExecPin))
	{
		Result = EScriptStatContainerType::NewExecutionPath;
	}
	else if (HasFlags(EScriptExecutionNodeFlags::Container))
	{
		Result = EScriptStatContainerType::Container;
	}
	else if (HasFlags(EScriptExecutionNodeFlags::SequentialBranch))
	{
		Result = EScriptStatContainerType::SequentialBranch;
	}
	return Result;
}

void FScriptExecutionNode::RefreshStats(const FTracePath& TracePath)
{
	// Process stat update
	bool bRefreshChildStats = false;
	bool bRefreshLinkStats = true;
	TArray<TSharedPtr<FScriptPerfData>> BlueprintPooledStats;
	// Update stats based on type
	switch(GetStatisticContainerType())
	{
		case EScriptStatContainerType::Standard:
		case EScriptStatContainerType::Container:
		{
			GetInstancePerfDataByTracePath(TracePath, BlueprintPooledStats);
			bRefreshChildStats = true;
			break;
		}
		case EScriptStatContainerType::PureNode:
		{
			GetInstancePerfDataByTracePath(TracePath, BlueprintPooledStats);
			bRefreshChildStats = false;
			bRefreshLinkStats = false;
			break;
		}
		case EScriptStatContainerType::SequentialBranch:
		{
			GetInstancePerfDataByTracePath(TracePath, BlueprintPooledStats);
			break;
		}
		case EScriptStatContainerType::NewExecutionPath:
		{
			// Refresh child stats and copy as branch stats - this is a dummy entry
			TSet<FName> AllInstances;
			TArray<FLinearExecPath> ChildExecPaths;
			for (auto ChildIter : ChildNodes)
			{
				ChildIter->GetValidInstanceNames(AllInstances);
				ChildIter->RefreshStats(TracePath);
				ChildIter->GetLinearExecutionPath(ChildExecPaths, TracePath, false);
			}
			for (auto InstanceIter : AllInstances)
			{
				if (InstanceIter != NAME_None)
				{
					TSharedPtr<FScriptPerfData> InstancePerfData = GetPerfDataByInstanceAndTracePath(InstanceIter, TracePath);
					InstancePerfData->Reset();
					BlueprintPooledStats.Add(InstancePerfData);
					TArray<TSharedPtr<FScriptPerfData>> ChildDataSet;

					for (auto ChildIter : ChildExecPaths)
					{
						TSharedPtr<FScriptPerfData> ChildPerfData = ChildIter.LinkedNode->GetPerfDataByInstanceAndTracePath(InstanceIter, ChildIter.TracePath);
						if (ChildPerfData.IsValid())
						{
							ChildDataSet.Add(ChildPerfData);
						}
					}
					InstancePerfData->AccumulateDataSet(ChildDataSet);
				}
			}
			// Update Blueprint stats as branches
			TSharedPtr<FScriptPerfData> BlueprintData = GetOrAddBlueprintPerfDataByTracePath(TracePath);
			BlueprintData->InitialiseFromDataSet(BlueprintPooledStats);
			BlueprintPooledStats.Reset(0);
			bRefreshLinkStats = false;
			break;
		}
	}
	// Refresh Child Links
	if (bRefreshChildStats)
	{
		for (auto ChildIter : ChildNodes)
		{
			ChildIter->RefreshStats(TracePath);
		}
	}
	// Refresh All links
	if (bRefreshLinkStats)
	{
		for (auto LinkIter : LinkedNodes)
		{
			FTracePath LinkTracePath(TracePath);
			if (LinkIter.Value->HasFlags(EScriptExecutionNodeFlags::EventPin))
			{
				LinkTracePath.ResetPath();
			}
			if (!LinkIter.Value->HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
			{
				LinkTracePath.AddExitPin(LinkIter.Key);
			}
			LinkIter.Value->RefreshStats(LinkTracePath);
		}
	}
	// Update the owning blueprint stats
	if (BlueprintPooledStats.Num() > 0)
	{
		TSharedPtr<FScriptPerfData> BlueprintData = GetOrAddBlueprintPerfDataByTracePath(TracePath);
		BlueprintData->InitialiseFromDataSet(BlueprintPooledStats);
	}
}

void FScriptExecutionNode::CalculateHeatLevelStats(TSharedPtr<FScriptHeatLevelMetrics> HeatLevelMetrics)
{
	SCOPE_CYCLE_COUNTER(STAT_CalculateHeatLevelsCost);
	for (auto InstancePerfDataMapIter : InstanceInputPinToPerfDataMap)
	{
		for (auto TracePathPerfDataMapIter : InstancePerfDataMapIter.Value)
		{
			TSharedPtr<FScriptPerfData> PerfData = TracePathPerfDataMapIter.Value;
			if (PerfData.IsValid())
			{
				PerfData->SetHeatLevels(HeatLevelMetrics);
			}
		}
	}

	if (!IsPureNode())
	{
		for (auto ChildIter : ChildNodes)
		{
			ChildIter->CalculateHeatLevelStats(HeatLevelMetrics);
		}
	
		for (auto LinkIter : LinkedNodes)
		{
			LinkIter.Value->CalculateHeatLevelStats(HeatLevelMetrics);
		}
	}
}

float FScriptExecutionNode::CalculateHottestPathStats(FScriptExecutionHottestPathParams HotPathParams)
{
	// Grab local perf data
	TSharedPtr<FScriptPerfData> LocalPerfData = GetOrAddPerfDataByInstanceAndTracePath(HotPathParams.InstanceName, HotPathParams.TracePath);
	float AccumulatedTime = 0.f;

	if (!IsEvent())
	{
		// Subtract local inclusive cost from parent inclusive cost
		const double NodeTime = IsBranch() ? LocalPerfData->GetAverageTiming() : LocalPerfData->GetInclusiveTiming();
		HotPathParams.TimeSoFar += NodeTime;
		AccumulatedTime += NodeTime;
	}
	if (!IsPureNode())
	{
		// Update children
		for (auto ChildIter : ChildNodes)
		{
			AccumulatedTime += ChildIter->CalculateHottestPathStats(HotPathParams);
		}
		// Update Links
		for (auto LinkIter : LinkedNodes)
		{
			FScriptExecutionHottestPathParams LinkedHotPathParams(HotPathParams);
			if (!LinkIter.Value->HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
			{
				LinkedHotPathParams.TracePath.AddExitPin(LinkIter.Key);
			}
			AccumulatedTime += LinkIter.Value->CalculateHottestPathStats(LinkedHotPathParams);
		}
	}
	const float HottestPathValue = static_cast<float>(AccumulatedTime / HotPathParams.EventTiming);
	LocalPerfData->SetHottestPathHeatLevel(HottestPathValue);

	return AccumulatedTime;
}

void FScriptExecutionNode::GetAllExecNodes(TMap<FName, TSharedPtr<FScriptExecutionNode>>& ExecNodesOut)
{
	for (auto ChildIter : ChildNodes)
	{
		ChildIter->GetAllExecNodes(ExecNodesOut);
		ExecNodesOut.Add(ChildIter->GetName()) = ChildIter;
	}
	for (auto LinkIter : LinkedNodes)
	{
		LinkIter.Value->GetAllExecNodes(ExecNodesOut);
		ExecNodesOut.Add(LinkIter.Value->GetName()) = LinkIter.Value;
	}
}

void FScriptExecutionNode::GetAllPureNodes(TMap<int32, TSharedPtr<class FScriptExecutionNode>>& PureNodesOut)
{
	GetAllPureNodes_Internal(PureNodesOut, PureNodeScriptCodeRange);

	// Sort pure nodes by script offset (execution order).
	PureNodesOut.KeySort(TLess<int32>());
}

void FScriptExecutionNode::GetAllPureNodes_Internal(TMap<int32, TSharedPtr<class FScriptExecutionNode>>& PureNodesOut, const FInt32Range& ScriptCodeRange)
{
	for (auto LinkIter : LinkedNodes)
	{
		LinkIter.Value->GetAllPureNodes_Internal(PureNodesOut, ScriptCodeRange);

		if (LinkIter.Value->IsPureNode() && ScriptCodeRange.Contains(LinkIter.Key))
		{
			PureNodesOut.Add(LinkIter.Key, LinkIter.Value);
		}
	}
}

TSharedPtr<FScriptExecutionNode> FScriptExecutionNode::GetPureChainNode()
{
	TSharedPtr<FScriptExecutionNode> Result;

	for (auto ChildIter : ChildNodes)
	{
		if (ChildIter->IsPureChain())
		{
			Result = ChildIter;
		}
	}

	return Result;
}

void FScriptExecutionNode::NavigateToObject() const
{
	if (UEdGraphPin* Pin = ObservedPin.Get())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnPin(Pin);
	}
	else if (IsObservedObjectValid())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ObservedObject.Get());
	}
}

bool FScriptExecutionNode::IsObservedObjectValid() const
{
	return ObservedObject.IsValid() && !ObservedObject.Get()->IsPendingKill();
}

const EScriptPerfDataType FScriptExecutionNode::GetPerfDataType() const
{
	return IsEvent() ? EScriptPerfDataType::Event : EScriptPerfDataType::Node;
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionTunnelInstance

void FScriptExecutionTunnelInstance::GetBlueprintPerfDataForAllTracePaths(FScriptPerfData& OutPerfData)
{
	OutPerfData.Reset();

	for (auto EntrySite : EntrySites)
	{
		EntrySite.Value->GetBlueprintPerfDataForAllTracePaths(OutPerfData);
	}
}

void FScriptExecutionTunnelInstance::AddEntrySite(const int32 ScriptOffset, TSharedPtr<FScriptExecutionTunnelEntry> EntrySite)
{
	EntrySites.FindOrAdd(ScriptOffset) = EntrySite;
	EntrySite->SetTunnelInstance(SharedThis<FScriptExecutionTunnelInstance>(this));
}

void FScriptExecutionTunnelInstance::CopyBoundarySites(TSharedPtr<FScriptExecutionTunnelInstance> OtherInstance)
{
	for (auto EntrySite : EntrySites)
	{
		OtherInstance->EntrySites.Add(EntrySite.Key) = EntrySite.Value;
	}
	for (auto ExitSite : ExitSites)
	{
		OtherInstance->ExitSites.Add(ExitSite.Key) = ExitSite.Value;
	}
}

TSharedPtr<FScriptExecutionNode> FScriptExecutionTunnelInstance::FindBoundarySite(const int32 ScriptOffset)
{
	TSharedPtr<FScriptExecutionNode> Result;
	if (TSharedPtr<FScriptExecutionTunnelEntry>* EntryResult = EntrySites.Find(ScriptOffset))
	{
		Result = *EntryResult;
	}
	else if (TSharedPtr<FScriptExecutionTunnelExit>* ExitResult = ExitSites.Find(ScriptOffset))
	{
		Result = *ExitResult;
	}
	return Result;
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionTunnelEntry

void FScriptExecutionTunnelEntry::GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath, const bool bIncludeChildren)
{
	LinearExecutionNodes.Add(FLinearExecPath(AsShared(), TracePath));
	if (!IsComplexTunnel())
	{
		for (auto TunnelExit : LinkedNodes)
		{
			for (auto LinkedToExit : TunnelExit.Value->GetLinkedNodes())
			{
				FTracePath ExitLinkTracePath(TracePath);
				if (LinkedToExit.Key != INDEX_NONE && !LinkedToExit.Value->HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
				{
					ExitLinkTracePath.AddExitPin(TunnelExit.Key);
				}
				LinkedToExit.Value->GetLinearExecutionPath(LinearExecutionNodes, ExitLinkTracePath);
			}
		}
	}
}

void FScriptExecutionTunnelEntry::AddTunnelTiming(const FName InstanceName, const FTracePath& TracePath, const FTracePath& InternalTracePath, const int32 ExitScriptOffset, const double Time)
{
	TSharedPtr<FScriptPerfData> PerfData = GetPerfDataByInstanceAndTracePath(InstanceName, TracePath);
	PerfData->AddEventTiming(0.0);
	PerfData->AddInclusiveTiming(Time, false);
	PerfData->OverrideSampleCount(TunnelEntryCount);
	// Tick Entry points
	for (auto EntrySite : ChildNodes)
	{
		PerfData = EntrySite->GetPerfDataByInstanceAndTracePath(InstanceName, TracePath);
		PerfData->AddEventTiming(0.0);
		PerfData->OverrideSampleCount(TunnelEntryCount);
	}
	// Tick exit sites
	TSharedPtr<FScriptExecutionNode> TunnelExitNode = GetExitSite(ExitScriptOffset);
	if (TunnelExitNode.IsValid())
	{
		PerfData = TunnelExitNode->GetPerfDataByInstanceAndTracePath(InstanceName, TracePath);
		PerfData->AddEventTiming(0.0);
		PerfData->OverrideSampleCount(TunnelEntryCount);
		PerfData = TunnelExitNode->GetPerfDataByInstanceAndTracePath(InstanceName, InternalTracePath);
		PerfData->AddEventTiming(0.0);
	}
}

TSharedPtr<FScriptExecutionTunnelExit> FScriptExecutionTunnelEntry::GetExitSite(const int32 ScriptOffset) const
{
	TSharedPtr<FScriptExecutionTunnelExit> ExitSite;
	if (const TSharedPtr<FScriptExecutionNode>* SearchResult = LinkedNodes.Find(ScriptOffset))
	{
		if ((*SearchResult)->IsTunnelExit())
		{
			ExitSite = StaticCastSharedPtr<FScriptExecutionTunnelExit>(*SearchResult);
		}
	}
	return ExitSite;
}

void FScriptExecutionTunnelEntry::RefreshStats(const FTracePath& TracePath)
{
	// Process stat update
	TArray<TSharedPtr<FScriptPerfData>> BlueprintPooledStats;
	// Update stats based on type
	GetInstancePerfDataByTracePath(TracePath, BlueprintPooledStats);
	// Create new trace
	FTracePath TunnelTracePath(TracePath, SharedThis<FScriptExecutionTunnelEntry>(this));
	// Refresh child entry points
	for (auto ChildIter : ChildNodes)
	{
		ChildIter->RefreshStats(TunnelTracePath);
	}
	// Refresh exit points
	for (auto ExitPoint : LinkedNodes)
	{
		ExitPoint.Value->RefreshStats(TunnelTracePath);
	}
	// Update the owning blueprint stats
	if (BlueprintPooledStats.Num() > 0)
	{
		TSharedPtr<FScriptPerfData> BlueprintData = GetBlueprintPerfDataByTracePath(TracePath);
		BlueprintData->InitialiseFromDataSet(BlueprintPooledStats);
	}
}

bool FScriptExecutionTunnelEntry::IsInternalBoundary(TSharedPtr<FScriptExecutionNode> PotentialBoundaryNode) const
{
	bool bIsInternalBoundary = false;
	if (PotentialBoundaryNode->IsTunnelBoundary())
	{
		bIsInternalBoundary = PotentialBoundaryNode->HasFlags(EScriptExecutionNodeFlags::TunnelEntryPin);
		if (!PotentialBoundaryNode->IsTunnelEntry())
		{
			if (const int32* SearchResult = LinkedNodes.FindKey(PotentialBoundaryNode))
			{
				bIsInternalBoundary = true;
			}
		}
	}
	return bIsInternalBoundary;
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionTunnelExit

void FScriptExecutionTunnelExit::GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath, const bool bIncludeChildren)
{
	LinearExecutionNodes.Add(FLinearExecPath(AsShared(), TracePath));
}

void FScriptExecutionTunnelExit::RefreshStats(const FTracePath& TracePath)
{
	// Process stat update
	TArray<TSharedPtr<FScriptPerfData>> BlueprintPooledStats;
	// Update stats based on type
	GetInstancePerfDataByTracePath(TracePath, BlueprintPooledStats);
	// Refresh all valid tunnel links
	for (auto LinkIter : LinkedNodes)
	{
		// Don't refresh any linked tunnel exits, the owning entry point will do that.
		if (!LinkIter.Value->HasFlags(EScriptExecutionNodeFlags::TunnelExitPin))
		{
			FTracePath LinkTracePath(TracePath);
			LinkTracePath.ExitTunnel();
			if (LinkIter.Value->HasFlags(EScriptExecutionNodeFlags::EventPin))
			{
				LinkTracePath.ResetPath();
			}
			if (!LinkIter.Value->HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
			{
				LinkTracePath.AddExitPin(LinkIter.Key);
			}
			LinkIter.Value->RefreshStats(LinkTracePath);
		}
	}
	// Update the owning blueprint stats
	if (BlueprintPooledStats.Num() > 0)
	{
		TSharedPtr<FScriptPerfData> BlueprintData = GetOrAddBlueprintPerfDataByTracePath(TracePath);
		BlueprintData->InitialiseFromDataSet(BlueprintPooledStats);
	}
}

void FScriptExecutionTunnelExit::AddInstanceExitSite(const uint32 TracePath, UEdGraphPin* InstanceExitPin)
{
	const FSlateBrush* InstanceIcon = InstanceExitPin->LinkedTo.Num() ?	FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPPinConnected")) : 
																		FEditorStyle::GetBrush(TEXT("BlueprintProfiler.BPPinDisconnected"));
	InstanceExitSiteInfo.FindOrAdd(TracePath) = FTunnelExitInstanceData(InstanceIcon, InstanceExitPin);
}

TSharedRef<SWidget> FScriptExecutionTunnelExit::GetIconWidget(const uint32 TracePath)
{
	if (FTunnelExitInstanceData* SearchResult = InstanceExitSiteInfo.Find(TracePath))
	{
		return SNew(SImage)
				.Image(SearchResult->InstanceIcon)
				.ColorAndOpacity(IconColor);
	}
	else
	{
		return SNew(SImage)
				.Image(Icon)
				.ColorAndOpacity(IconColor);
	}
}

TSharedRef<SWidget> FScriptExecutionTunnelExit::GetHyperlinkWidget(const uint32 TracePath)
{
	return SNew(SHyperlink)
		#if DISPLAY_NODENAMES_IN_TITLES
		.Text(FText::FromName(NodeName))
		#else
		.Text(DisplayName)
		#endif
		.Style(FEditorStyle::Get(), "BlueprintProfiler.Node.Hyperlink")
		.ToolTipText(Tooltip)
		.OnNavigate(this, &FScriptExecutionTunnelExit::NavigateToExitSite, TracePath);
}

void FScriptExecutionTunnelExit::NavigateToExitSite(const uint32 TracePath) const
{
	UEdGraphPin* Pin = ObservedPin.Get();
	if (const FTunnelExitInstanceData* SearchResult = InstanceExitSiteInfo.Find(TracePath))
	{
		Pin = SearchResult->InstancePin.Get();
	}
	if (Pin)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Pin->GetOwningNode());
	}
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionPureChainNode

void FScriptExecutionPureChainNode::GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath, const bool bIncludeChildren)
{
	LinearExecutionNodes.Add(FLinearExecPath(AsShared(), TracePath));
	FTracePath NewTracePath(TracePath);
	if (GetNumLinkedNodes() == 1)
	{
		for (auto NodeIter : LinkedNodes)
		{
			if (HasFlags(EScriptExecutionNodeFlags::PureStats))
			{
				continue;
			}
			else
			{
				if (NodeIter.Key != INDEX_NONE && !HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
				{
					NewTracePath.AddExitPin(NodeIter.Key);
				}
				NodeIter.Value->GetLinearExecutionPath(LinearExecutionNodes, NewTracePath);
			}
		}
	}
}

void FScriptExecutionPureChainNode::RefreshStats(const FTracePath& TracePath)
{
	// Process stat update
	TArray<TSharedPtr<FScriptPerfData>> BlueprintPooledStats;
	// Refresh all pure nodes and accumulate for blueprint stat.
	if (HasFlags(EScriptExecutionNodeFlags::PureChain))
	{
		// Update linked pure nodes and find instances
		TSet<FName> ValidInstances;
		FTracePath PureTracePath(TracePath);
		TMap<int32, TSharedPtr<FScriptExecutionNode>> AllPureNodes;
		GetAllPureNodes(AllPureNodes);
		for (auto PureIter : AllPureNodes)
		{
			GetValidInstanceNames(ValidInstances);
			PureTracePath.AddExitPin(PureIter.Key);
			PureIter.Value->RefreshStats(PureTracePath);
		}
	}
	// Grab all instance stats to accumulate into the blueprint stat.
	GetInstancePerfDataByTracePath(TracePath, BlueprintPooledStats);
	// Update the owning blueprint stats
	if (BlueprintPooledStats.Num() > 0)
	{
		TSharedPtr<FScriptPerfData> BlueprintData = GetBlueprintPerfDataByTracePath(TracePath);
		BlueprintData->InitialiseFromDataSet(BlueprintPooledStats);
	}
}

float FScriptExecutionPureChainNode::CalculateHottestPathStats(FScriptExecutionHottestPathParams HotPathParams)
{
	float AccumulatedTime = 0.f;
	// Update Pure Links
	TMap<int32, TSharedPtr<FScriptExecutionNode>> AllPureNodes;
	GetAllPureNodes(AllPureNodes);
	for (auto PureIter : AllPureNodes)
	{
		FScriptExecutionHottestPathParams LinkedHotPathParams(HotPathParams);
		if (!PureIter.Value->HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
		{
			LinkedHotPathParams.TracePath.AddExitPin(PureIter.Key);
		}
		AccumulatedTime += PureIter.Value->CalculateHottestPathStats(LinkedHotPathParams);
	}
	return AccumulatedTime;
}

void FScriptExecutionPureChainNode::CalculateHeatLevelStats(TSharedPtr<FScriptHeatLevelMetrics> HeatLevelMetrics)
{
	SCOPE_CYCLE_COUNTER(STAT_CalculateHeatLevelsCost);
	TMap<int32, TSharedPtr<FScriptExecutionNode>> AllPureNodes;
	GetAllPureNodes(AllPureNodes);
	for (auto PureIter : AllPureNodes)
	{
		PureIter.Value->CalculateHeatLevelStats(HeatLevelMetrics);
	}
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionInstance

void FScriptExecutionInstance::NavigateToObject() const
{
	if (const FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
	{
		if (const AActor* PIEActor = Cast<AActor>(PIEInstance.Get()))
		{
			GEditor->SelectNone(false, false);
			GEditor->SelectActor(const_cast<AActor*>(PIEActor), true, true, true);
		}
	}
	else
	{
		if (const AActor* EditorActor = Cast<AActor>(ObservedObject.Get()))
		{
			GEditor->SelectNone(false, false);
			GEditor->SelectActor(const_cast<AActor*>(EditorActor), true, true, true);
		}
	}
}

bool FScriptExecutionInstance::IsObservedObjectValid() const
{
	if (const FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
	{
		if (PIEInstance.IsValid() && PIEInstance.Get()->IsPendingKill())
		{
			bPIEInstanceDestroyed = true;
		}
		return PIEInstance.IsValid();
	}
	else
	{
		return ObservedObject.IsValid() && !ObservedObject.Get()->IsPendingKill();
	}
}

UObject* FScriptExecutionInstance::GetActiveObject() const
{
	UObject* Result = nullptr;
	if (const FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
	{
		if (!PIEInstance.IsValid() && !bPIEInstanceDestroyed)
		{
			RefreshPIEInstance();
		}
		Result = const_cast<UObject*>(PIEInstance.Get());
	}
	else
	{
		Result = const_cast<UObject*>(ObservedObject.Get());
	}
	return Result;
}

void FScriptExecutionInstance::RefreshPIEInstance() const
{
	if (!bPIEInstanceDestroyed)
	{
		const FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
		UWorld* PIEWorld = PIEWorldContext ? PIEWorldContext->World() : nullptr;
		ULevel* CurrentLevel = PIEWorld ? PIEWorld->GetCurrentLevel() : nullptr;
		if (CurrentLevel && ObservedObject.IsValid())
		{
			PIEInstance = FindObject<UObject>(CurrentLevel, *ObservedObject.Get()->GetName());
		}
	}
}

TSharedRef<SWidget> FScriptExecutionInstance::GetIconWidget(const uint32 /*TracePath*/)
{
	return SNew(SImage)
			.Image(Icon)
			.ColorAndOpacity(this, &FScriptExecutionInstance::GetInstanceIconColor);
}

TSharedRef<SWidget> FScriptExecutionInstance::GetHyperlinkWidget(const uint32 /*TracePath*/)
{
	return SNew(SHyperlink)
		#if DISPLAY_NODENAMES_IN_TITLES
		.Text(FText::FromName(NodeName))
		#else
		.Text(DisplayName)
		#endif
		.Style(FEditorStyle::Get(), "BlueprintProfiler.Node.Hyperlink")
		.ToolTipText(this, &FScriptExecutionInstance::GetInstanceTooltip)
		.OnNavigate(this, &FScriptExecutionInstance::NavigateToObject);
}

FSlateColor FScriptExecutionInstance::GetInstanceIconColor() const
{
	return FSlateColor(IsObservedObjectValid() ? IconColor : FLinearColor(1.f, 1.f, 1.f, 0.3f));
}

FText FScriptExecutionInstance::GetInstanceTooltip() const
{
	return IsObservedObjectValid() ? Tooltip : LOCTEXT("ActorInstanceInvalid_Tooltip", "Actor instance no longer exists");
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionBlueprint

FScriptExecutionBlueprint::FScriptExecutionBlueprint(const FScriptExecNodeParams& InitParams)
	: FScriptExecutionNode(InitParams)
{
	HeatLevelMetrics = MakeShareable(new FScriptHeatLevelMetrics());
}

TSharedPtr<FScriptExecutionNode> FScriptExecutionBlueprint::GetInstanceByName(FName InstanceName)
{
	TSharedPtr<FScriptExecutionNode> Result;
	for (auto Iter : Instances)
	{
		if (Iter->GetName() == InstanceName)
		{
			Result = Iter;
			break;
		}
	}
	return Result;
}

void FScriptExecutionBlueprint::RefreshStats(const FTracePath& TracePath)
{
	TArray<TSharedPtr<FScriptPerfData>> InstanceData;
	// Update event stats
	for (auto BlueprintEventIter : ChildNodes)
	{
		// This crawls through and updates all instance stats and pools the results into the blueprint node stats
		// as an overall blueprint performance representation.
		BlueprintEventIter->RefreshStats(TracePath);
	}
	// Determine if we require the hottest path's updating
	const bool bUpdateHottestPaths = GetDefault<UBlueprintProfilerSettings>()->WireHeatMapDisplayMode != EBlueprintProfilerHeatMapDisplayMode::None;
	// Update instance stats
	for (auto InstanceIter : Instances)
	{
		TSharedPtr<FScriptPerfData> InstancePerfData = InstanceIter->GetOrAddPerfDataByInstanceAndTracePath(InstanceIter->GetName(), TracePath);
		InstancePerfData->Reset();
		// Update all top level instance stats now the events are up to date.
		for (auto InstanceEventIter : InstanceIter->GetChildNodes())
		{
			TSharedPtr<FScriptPerfData> InstanceEventPerfData = InstanceEventIter->GetOrAddPerfDataByInstanceAndTracePath(InstanceIter->GetName(), TracePath);
			InstancePerfData->AddData(*InstanceEventPerfData.Get());
			// Update the hottest path stats
			if (bUpdateHottestPaths)
			{
				SCOPE_CYCLE_COUNTER(STAT_CalculateHottestPathCost);
				FScriptExecutionHottestPathParams HotPathParams(InstanceIter->GetName(), InstanceEventPerfData->GetAverageTiming());
				InstanceEventIter->CalculateHottestPathStats(HotPathParams);
			}
		}
		// Add for consolidation at the bottom.
		InstanceData.Add(InstancePerfData);
	}
	// Finally... update the root stats for the owning blueprint.
	if (InstanceData.Num() > 0)
	{
		TSharedPtr<FScriptPerfData> BlueprintData = GetOrAddBlueprintPerfDataByTracePath(TracePath);
		BlueprintData->Reset();

		for (auto InstanceIter : InstanceData)
		{
			BlueprintData->AddData(*InstanceIter.Get());
		}
		
		// Now that all stats have been updated/accumulated, update heat level metrics data.
		UpdateHeatLevelMetrics(BlueprintData);

		// Finally, calculate updated heat levels based on the current heat level metrics data.
		CalculateHeatLevelStats(HeatLevelMetrics);
		for (auto InstanceIter : Instances)
		{
			InstanceIter->CalculateHeatLevelStats(HeatLevelMetrics);
		}
	}
}

void FScriptExecutionBlueprint::GetAllExecNodes(TMap<FName, TSharedPtr<FScriptExecutionNode>>& ExecNodesOut)
{
	for (auto BlueprintEventIter : ChildNodes)
	{
		BlueprintEventIter->GetAllExecNodes(ExecNodesOut);
		ExecNodesOut.Add(BlueprintEventIter->GetName()) = BlueprintEventIter;
	}
	for (auto InstanceIter : Instances)
	{
		InstanceIter->GetAllExecNodes(ExecNodesOut);
		ExecNodesOut.Add(InstanceIter->GetName()) = InstanceIter;
	}
	ExecNodesOut.Add(GetName()) = AsShared();
}

void FScriptExecutionBlueprint::NavigateToObject() const
{
	if (IsObservedObjectValid())
	{
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(ObservedObject.Get()))
		{
			TArray<FAssetData> BlueprintAssets;
			BlueprintAssets.Add(Blueprint);
			GEditor->SyncBrowserToObjects(BlueprintAssets);
		}
	}
}

void FScriptExecutionBlueprint::SortEvents()
{
	// Sort the events
	const auto SortEvents = [](const TSharedPtr<FScriptExecutionNode>& A, const TSharedPtr<FScriptExecutionNode>& B)
	{
		if (A->HasFlags(EScriptExecutionNodeFlags::InheritedEvent))
		{
			if (!B->HasFlags(EScriptExecutionNodeFlags::InheritedEvent))
			{
				return false;
			}
		}
		else if (B->HasFlags(EScriptExecutionNodeFlags::InheritedEvent))
		{
			return true;
		}
		return true;
	};
	ChildNodes.Sort(SortEvents);
}

void FScriptExecutionBlueprint::UpdateHeatLevelMetrics(TSharedPtr<FScriptPerfData> BlueprintData)
{
	SCOPE_CYCLE_COUNTER(STAT_CalculateHeatLevelsCost);
	const UBlueprintProfilerSettings* ProfilerSettings = GetDefault<UBlueprintProfilerSettings>();
	check(ProfilerSettings != nullptr);

	switch (ProfilerSettings->HeatLevelMetricsType)
	{
	case EBlueprintProfilerHeatLevelMetricsType::ClassRelative:
	{
		if (BlueprintData.IsValid())
		{
			const double AverageTiming = BlueprintData->GetAverageTiming();
			const double InclusiveTiming = BlueprintData->GetInclusiveTiming();
			const double MaxTiming = BlueprintData->GetMaxTiming();
			const double TotalTiming = BlueprintData->GetTotalTiming();

			HeatLevelMetrics->AveragePerformanceThreshold = AverageTiming > 0.0f ? 1.0f / AverageTiming : 0.0f;
			HeatLevelMetrics->InclusivePerformanceThreshold = InclusiveTiming > 0.0f ? 1.0f / InclusiveTiming : 0.0f;
			HeatLevelMetrics->MaxPerformanceThreshold = MaxTiming > 0.0f ? 1.0f / MaxTiming : 0.0f;
			HeatLevelMetrics->TotalTimePerformanceThreshold = TotalTiming > 0.0f ? 1.0f / TotalTiming : 0.0f;
		}
		else
		{
			HeatLevelMetrics->AveragePerformanceThreshold = 0.0f;
			HeatLevelMetrics->InclusivePerformanceThreshold = 0.0f;
			HeatLevelMetrics->MaxPerformanceThreshold = 0.0f;
			HeatLevelMetrics->TotalTimePerformanceThreshold = 0.0f;
		}

		HeatLevelMetrics->bUseTotalTimeWaterMark = false;
		HeatLevelMetrics->EventPerformanceThreshold = HeatLevelMetrics->AveragePerformanceThreshold;
	}
		break;

	case EBlueprintProfilerHeatLevelMetricsType::FrameRelative:
	{
		// @TODO (this type is not needed for MVP)
		HeatLevelMetrics->EventPerformanceThreshold = 0.0f;
		HeatLevelMetrics->AveragePerformanceThreshold = 0.0f;
		HeatLevelMetrics->InclusivePerformanceThreshold = 0.0f;
		HeatLevelMetrics->MaxPerformanceThreshold = 0.0f;
		HeatLevelMetrics->bUseTotalTimeWaterMark = false;
		HeatLevelMetrics->TotalTimePerformanceThreshold = 0.0f;
	}
		break;

	case EBlueprintProfilerHeatLevelMetricsType::CustomThresholds:
	default:
	{
		HeatLevelMetrics->EventPerformanceThreshold = ProfilerSettings->CustomEventPerformanceThreshold;
		HeatLevelMetrics->AveragePerformanceThreshold = ProfilerSettings->CustomAveragePerformanceThreshold;
		HeatLevelMetrics->InclusivePerformanceThreshold = ProfilerSettings->CustomInclusivePerformanceThreshold;
		HeatLevelMetrics->MaxPerformanceThreshold = ProfilerSettings->CustomMaxPerformanceThreshold;
		HeatLevelMetrics->bUseTotalTimeWaterMark = true;
		HeatLevelMetrics->TotalTimePerformanceThreshold = 0.0f;	// not used for this type
	}
		break;
	}
}

#undef LOCTEXT_NAMESPACE