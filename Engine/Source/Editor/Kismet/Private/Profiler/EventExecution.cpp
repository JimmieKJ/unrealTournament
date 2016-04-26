// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "EventExecution.h"
#include "ScriptPerfData.h"

//////////////////////////////////////////////////////////////////////////
// FScriptNodeExecLinkage

void FScriptNodeExecLinkage::AddPureNode(const int32 PinScriptOffset, TSharedPtr<class FScriptExecutionNode> PureNode)
{
	TSharedPtr<class FScriptExecutionNode>& Result = PureNodes.FindOrAdd(PinScriptOffset);
	Result = PureNode;
}

void FScriptNodeExecLinkage::AddLinkedNode(const int32 PinScriptOffset, TSharedPtr<class FScriptExecutionNode> LinkedNode)
{
	TSharedPtr<class FScriptExecutionNode>& Result = LinkedNodes.FindOrAdd(PinScriptOffset);
	Result = LinkedNode;
}

//////////////////////////////////////////////////////////////////////////
// FScriptNodePerfData

bool FScriptNodePerfData::HasPerfDataForInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath) const
{
	const TMap<const uint32, TSharedPtr<FScriptPerfData>>* InstanceMapPtr = InstanceInputPinToPerfDataMap.Find(InstanceName);
	if (InstanceMapPtr)
	{
		return InstanceMapPtr->Contains(TracePath);
	}

	return false;
}

TSharedPtr<FScriptPerfData> FScriptNodePerfData::GetPerfDataByInstanceAndTracePath(FName InstanceName, const FTracePath& TracePath)
{ 
	TMap<const uint32, TSharedPtr<FScriptPerfData>>& InstanceMap = InstanceInputPinToPerfDataMap.FindOrAdd(InstanceName);
	TSharedPtr<FScriptPerfData>& Result = InstanceMap.FindOrAdd(TracePath);
	if (!Result.IsValid())
	{
		Result = MakeShareable<FScriptPerfData>(new FScriptPerfData);
	}
	return Result;
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

TSharedPtr<FScriptPerfData> FScriptNodePerfData::GetBlueprintPerfDataByTracePath(const FTracePath& TracePath)
{
	TMap<const uint32, TSharedPtr<FScriptPerfData>>& BlueprintMap = InstanceInputPinToPerfDataMap.FindOrAdd(NAME_None);
	TSharedPtr<FScriptPerfData>& Result = BlueprintMap.FindOrAdd(TracePath);
	if (!Result.IsValid())
	{
		Result = MakeShareable<FScriptPerfData>(new FScriptPerfData);
	}
	return Result;
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionNode

FScriptExecutionNode::FScriptExecutionNode()
	: NodeFlags(EScriptExecutionNodeFlags::None)
	, bExpansionState(false)
{
}

FScriptExecutionNode::FScriptExecutionNode(const FScriptExecNodeParams& InitParams)
	: NodeName(InitParams.NodeName)
	, OwningGraphName(InitParams.OwningGraphName)
	, NodeFlags(InitParams.NodeFlags)
	, ObservedObject(InitParams.ObservedObject)
	, DisplayName(InitParams.DisplayName)
	, Tooltip(InitParams.Tooltip)
	, IconColor(InitParams.IconColor)
	, Icon(InitParams.Icon)
	, bExpansionState(false)
{
}

bool FScriptExecutionNode::operator == (const FScriptExecutionNode& NodeIn) const
{
	return NodeName == NodeIn.NodeName;
}

void FScriptExecutionNode::GetLinearExecutionPath(TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath)
{
	FExecNodeFilter Filter;
	GetLinearExecutionPath_Internal(Filter, LinearExecutionNodes, TracePath);
}

void FScriptExecutionNode::GetLinearExecutionPath_Internal(FExecNodeFilter& Filter, TArray<FLinearExecPath>& LinearExecutionNodes, const FTracePath& TracePath)
{
	if (!Filter.Contains(ObservedObject))
	{
		Filter.Add(ObservedObject);
		LinearExecutionNodes.Add(FLinearExecPath(AsShared(), TracePath));
		if (GetNumLinkedNodes() == 1)
		{
			FTracePath NewTracePath(TracePath);
			for (auto NodeIter : LinkedNodes)
			{
				NewTracePath.AddExitPin(NodeIter.Key);
				NodeIter.Value->GetLinearExecutionPath_Internal(Filter, LinearExecutionNodes, NewTracePath);
			}
		}
	}
}

EScriptStatContainerType::Type FScriptExecutionNode::GetStatisticContainerType() const
{
	EScriptStatContainerType::Type Result = EScriptStatContainerType::Standard;
	if (HasFlags(EScriptExecutionNodeFlags::Instance|EScriptExecutionNodeFlags::Event|EScriptExecutionNodeFlags::CallSite))
	{
		Result = EScriptStatContainerType::Container;
	}
	else if (HasFlags(EScriptExecutionNodeFlags::SequentialBranch))
	{
		Result = EScriptStatContainerType::SequentialBranch;
	}
	else if (HasFlags(EScriptExecutionNodeFlags::ExecPin))
	{
		Result = EScriptStatContainerType::NewExecutionPath;
	}
	return Result;
}

void FScriptExecutionNode::RefreshStats(const FTracePath& TracePath)
{
	FExecNodeFilter StatFilter;
	RefreshStats_Internal(TracePath, StatFilter);
}

void FScriptExecutionNode::RefreshStats_Internal(const FTracePath& TracePath, FExecNodeFilter& VisitedStats)
{
	if (!VisitedStats.Find(ObservedObject))
	{
		// Update visited stats
		if (!HasFlags(EScriptExecutionNodeFlags::Container))
		{
			VisitedStats.Add(ObservedObject);
		}
		// Process stat update
		bool bRefreshChildStats = false;
		bool bRefreshLinkStats = true;
		TArray<TSharedPtr<FScriptPerfData>> BlueprintPooledStats;
		// Update stats based on type
		switch(GetStatisticContainerType())
		{
			case EScriptStatContainerType::Container:
			{
				// Only consider stat data on this path
				GetInstancePerfDataByTracePath(TracePath, BlueprintPooledStats);
				bRefreshChildStats = true;
				break;
			}
			case EScriptStatContainerType::Standard:
			case EScriptStatContainerType::SequentialBranch:
			{
				// Only consider stat data on this path
				GetInstancePerfDataByTracePath(TracePath, BlueprintPooledStats);
				break;
			}
			case EScriptStatContainerType::NewExecutionPath:
			{
				TSet<FName> AllInstances;
				// Refresh child stats and copy as branch stats - this is a dummy entry
				for (auto ChildIter : ChildNodes)
				{
					// Fill out instance data that can be missing because its a dummy node, or the code below can fail.
					for (auto InstanceIter : ChildIter->InstanceInputPinToPerfDataMap)
					{
						if (InstanceIter.Key != NAME_None)
						{
							AllInstances.Add(InstanceIter.Key);
						}
					}
					ChildIter->RefreshStats_Internal(TracePath, VisitedStats);
				}
				for (auto InstanceIter : AllInstances)
				{
					if (InstanceIter != NAME_None)
					{
						TSharedPtr<FScriptPerfData> InstancePerfData = GetPerfDataByInstanceAndTracePath(InstanceIter, TracePath);
						InstancePerfData->Reset();
						BlueprintPooledStats.Add(InstancePerfData);

						for (auto ChildIter : ChildNodes)
						{
							TSharedPtr<FScriptPerfData> ChildPerfData = ChildIter->GetPerfDataByInstanceAndTracePath(InstanceIter, TracePath);
							InstancePerfData->AddBranchData(*ChildPerfData.Get());
						}
					}
				}
				// Update Blueprint stats as branches
				TSharedPtr<FScriptPerfData> BlueprintData = GetBlueprintPerfDataByTracePath(TracePath);
				BlueprintData->Reset();

				for (auto BlueprintChildDataIter : BlueprintPooledStats)
				{
					BlueprintData->AddBranchData(*BlueprintChildDataIter.Get());
				}
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
				ChildIter->RefreshStats_Internal(TracePath, VisitedStats);
			}
		}
		// Refresh All links
		if (bRefreshLinkStats)
		{
			if (!IsPureNode())
			{
				TMap<int32, TSharedPtr<FScriptExecutionNode>> AllPureNodes;
				GetAllPureNodes(AllPureNodes);

				AllPureNodes.KeySort(TLess<int32>());

				FTracePath PureTracePath(TracePath);
				for (auto PureIter : AllPureNodes)
				{
					PureTracePath.AddExitPin(PureIter.Key);
					PureIter.Value->RefreshStats(PureTracePath);
				}
			}

			for (auto LinkIter : LinkedNodes)
			{
				FTracePath LinkTracePath(TracePath);
				LinkTracePath.AddExitPin(LinkIter.Key);
				LinkIter.Value->RefreshStats_Internal(LinkTracePath, VisitedStats);
			}
		}
		// Update the owning blueprint stats
		if (BlueprintPooledStats.Num() > 0)
		{
			TSharedPtr<FScriptPerfData> BlueprintData = GetBlueprintPerfDataByTracePath(TracePath);
			BlueprintData->Reset();

			for (auto BlueprintChildDataIter : BlueprintPooledStats)
			{
				BlueprintData->AddData(*BlueprintChildDataIter.Get());
			}
		}
	}
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
}

void FScriptExecutionNode::GetAllPureNodes_Internal(TMap<int32, TSharedPtr<class FScriptExecutionNode>>& PureNodesOut, const FInt32Range& ScriptCodeRange)
{
	for (auto PureIter : PureNodes)
	{
		PureIter.Value->GetAllPureNodes_Internal(PureNodesOut, ScriptCodeRange);

		if (ScriptCodeRange.Contains(PureIter.Key))
		{
			PureNodesOut.Add(PureIter.Key, PureIter.Value);
		}
	}
}

void FScriptExecutionNode::NavigateToObject() const
{
	if (ObservedObject.IsValid())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ObservedObject.Get());
	}
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionBlueprint

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
	// Update instance stats
	for (auto InstanceIter : Instances)
	{
		TSharedPtr<FScriptPerfData> InstancePerfData = InstanceIter->GetPerfDataByInstanceAndTracePath(InstanceIter->GetName(), TracePath);
		InstancePerfData->Reset();
		// Update all top level instance stats now the events are up to date.
		for (auto InstanceEventIter : InstanceIter->GetChildNodes())
		{
			TSharedPtr<FScriptPerfData> InstanceEventPerfData = InstanceEventIter->GetPerfDataByInstanceAndTracePath(InstanceIter->GetName(), TracePath);
			InstancePerfData->AddData(*InstanceEventPerfData.Get());
		}
		// Add for consolidation at the bottom.
		InstanceData.Add(InstancePerfData);
	}
	// Finally... update the root stats for the owning blueprint.
	if (InstanceData.Num() > 0)
	{
		TSharedPtr<FScriptPerfData> BlueprintData = GetBlueprintPerfDataByTracePath(TracePath);
		BlueprintData->Reset();

		for (auto InstanceIter : InstanceData)
		{
			BlueprintData->AddData(*InstanceIter.Get());
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
	if (ObservedObject.IsValid())
	{
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(ObservedObject.Get()))
		{
			TArray<FAssetData> BlueprintAssets;
			BlueprintAssets.Add(Blueprint);
			GEditor->SyncBrowserToObjects(BlueprintAssets);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FScriptExecutionInstance

void FScriptExecutionInstance::NavigateToObject() const
{
	if (ObservedObject.IsValid())
	{
		if (const AActor* Actor = Cast<AActor>(ObservedObject.Get()))
		{
			if (const FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
			{
				if (const UWorld* PIEWorld = PIEWorldContext->World())
				{
					for (auto LevelIter : PIEWorld->GetLevels())
					{
						if (AActor* PIEActor = Cast<AActor>(FindObject<UObject>(LevelIter, *Actor->GetName())))
						{
							GEditor->SelectNone(false, false);
							GEditor->SelectActor(const_cast<AActor*>(PIEActor), true, true, true);
							break;
						}
					}
				}
			}
			else
			{
				GEditor->SelectNone(false, false);
				GEditor->SelectActor(const_cast<AActor*>(Actor), true, true, true);
			}
		}
	}
}
