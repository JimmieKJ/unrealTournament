// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalLODOutlinerPrivatePCH.h"
#include "HLODOutliner.h"
#include "LevelEditor.h"
#include "LODLevelItem.h"
#include "HierarchicalLODUtilities.h"
#include "ScopedTransaction.h"
#include "SlateBasics.h"
#include "TreeItemID.h"

#define LOCTEXT_NAMESPACE "LODLevelItem"

HLODOutliner::FLODLevelItem::FLODLevelItem(const uint32 InLODIndex)
	: LODLevelIndex(InLODIndex), ID(nullptr)
{
	Type = ITreeItem::HierarchicalLODLevel;
	ID.SetCachedHash(GetTypeHash(FString("LODLevel - ") + FString::FromInt(LODLevelIndex)));
}

bool HLODOutliner::FLODLevelItem::CanInteract() const
{
	return true;
}

void HLODOutliner::FLODLevelItem::GenerateContextMenu(FMenuBuilder& MenuBuilder, SHLODOutliner& Outliner)
{

}

FString HLODOutliner::FLODLevelItem::GetDisplayString() const
{
	return FString("LODLevel - ") + FString::FromInt(LODLevelIndex);
}

HLODOutliner::FTreeItemID HLODOutliner::FLODLevelItem::GetID()
{
	return ID;
}

void HLODOutliner::FLODLevelItem::PopulateDragDropPayload(FDragDropPayload& Payload) const
{
	// Nothing to populate
}

HLODOutliner::FDragValidationInfo HLODOutliner::FLODLevelItem::ValidateDrop(FDragDropPayload& DraggedObjects) const
{
	FLODLevelDropTarget Target(LODLevelIndex);
	return Target.ValidateDrop(DraggedObjects);
}

void HLODOutliner::FLODLevelItem::OnDrop(FDragDropPayload& DraggedObjects, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	FLODLevelDropTarget Target(LODLevelIndex);		
	Target.OnDrop(DraggedObjects, ValidationInfo, DroppedOnWidget);

	// Expand this HLOD level item
	bIsExpanded = true;
}

HLODOutliner::FDragValidationInfo HLODOutliner::FLODLevelDropTarget::ValidateDrop(FDragDropPayload& DraggedObjects) const
{
	if (DraggedObjects.StaticMeshActors.IsSet() && DraggedObjects.StaticMeshActors->Num() > 0)
	{
		const int32 NumStaticMeshActors = DraggedObjects.StaticMeshActors->Num();	
		bool bSameLevelInstance = true;
		ULevel* Level = nullptr;
		for (auto Actor : DraggedObjects.StaticMeshActors.GetValue())
		{
			if (Level == nullptr)
			{
				Level = Actor->GetLevel();
			}
			else if (Level != Actor->GetLevel())
			{
				bSameLevelInstance = false;
			}
		}

		if (!bSameLevelInstance)
		{
			return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("NotInSameLevelAsset", "Static Mesh Actors not in the same level asset (streaming level)"));
		}

		return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_CompatibleNewCluster, LOCTEXT("CreateNewCluster", "Create new Cluster"));
	}
	else if (DraggedObjects.LODActors.IsSet() && DraggedObjects.LODActors->Num() > 0)
	{
		const int32 NumLODActors = DraggedObjects.LODActors->Num();

		if (NumLODActors > 1)
		{
			// Check if all the dragged LOD actors fall within the same LOD level
			auto LODActors = DraggedObjects.LODActors.GetValue();
			int32 LevelIndex = -1;
			bool bSameLODLevel = true;
			bool bSameLevelInstance = true;
			bool bValidOperation = true;
			ULevel* Level = nullptr;
			for (auto Actor : LODActors)
			{
				ALODActor* LODActor = Cast<ALODActor>(Actor.Get());
				if (LevelIndex == -1)
				{
					LevelIndex = LODActor->LODLevel;
				}
				else if (LevelIndex != LODActor->LODLevel)
				{
					bSameLODLevel = false;
				}

				if (FHierarchicalLODUtilities::GetParentLODActor(LODActor))
				{
					bValidOperation = false;
				}

				if (Level == nullptr)
				{
					Level = LODActor->GetLevel();
				}
				else if (Level != LODActor->GetLevel())
				{
					bSameLevelInstance = false;
				}
			}

			if (!bValidOperation)
			{
				return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("OneOrMultipleAlreadyInHLODLevel", "One or multiple LODActors are already part of a cluster in this HLOD level"));
			}

			if (!bSameLODLevel)
			{
				return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("NotInSameLODLevel", "LODActors are not all in the same HLOD level"));
			}

			if (!bSameLevelInstance)
			{
				return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("NotInSameLevelAsset", "LODActors not in the same level asset (streaming level)"));
			}

			if (bSameLevelInstance && bSameLODLevel && LevelIndex < (int32)(LODLevelIndex + 1))
			{
				return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleNewCluster, LOCTEXT("CreateNewCluster", "Create new Cluster"));
			}
			
		}
	}

	return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("NotImplemented", "Not implemented"));
}

void HLODOutliner::FLODLevelDropTarget::OnDrop(FDragDropPayload& DraggedObjects, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	if (ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_CompatibleNewCluster || ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleNewCluster)
	{
		CreateNewCluster(DraggedObjects);		
	}	
}

void HLODOutliner::FLODLevelDropTarget::CreateNewCluster(FDragDropPayload &DraggedObjects)
{	
	// Outerworld in which the LODActors should be spawned/saved (this is to enable support for streaming levels)
	UWorld* OuterWorld = nullptr;
	if (DraggedObjects.StaticMeshActors.IsSet() && DraggedObjects.StaticMeshActors->Num() > 0)
	{
		OuterWorld = Cast<UWorld>(DraggedObjects.StaticMeshActors.GetValue()[0]->GetLevel()->GetOuter());
	}
	else if (DraggedObjects.LODActors.IsSet() && DraggedObjects.LODActors->Num() > 0)
	{
		OuterWorld = Cast<UWorld>(DraggedObjects.LODActors.GetValue()[0]->GetLevel()->GetOuter());
	}

	// Retrieve world settings from the InWorld instance, this is the instance the HLODOutliner is running on
	auto WorldSettings = DraggedObjects.OutlinerWorld->GetWorldSettings();

	const FScopedTransaction Transaction(LOCTEXT("UndoAction_CreateNewCluster", "Create new Cluster"));
	OuterWorld->Modify();

	if (WorldSettings->bEnableHierarchicalLODSystem)
	{
		ALODActor* NewCluster = FHierarchicalLODUtilities::CreateNewClusterActor(OuterWorld, LODLevelIndex, WorldSettings);

		if (NewCluster)
		{
			for (TWeakObjectPtr<AActor> StaticMeshActor : DraggedObjects.StaticMeshActors.GetValue())
			{
				AActor* InActor = StaticMeshActor.Get();
				ALODActor* CurrentParentActor = FHierarchicalLODUtilities::GetParentLODActor(InActor);
				if (CurrentParentActor)
				{
					CurrentParentActor->RemoveSubActor(InActor);

					if (!CurrentParentActor->HasValidSubActors())
					{
						FHierarchicalLODUtilities::DeleteLODActor(CurrentParentActor);
					}
				}

				NewCluster->AddSubActor(InActor);
			}

			for (TWeakObjectPtr<AActor> LODActor : DraggedObjects.LODActors.GetValue())
			{
				AActor* InActor = LODActor.Get();
				ALODActor* CurrentParentActor = FHierarchicalLODUtilities::GetParentLODActor(InActor);
				if (CurrentParentActor)
				{
					CurrentParentActor->RemoveSubActor(InActor);
					if (!CurrentParentActor->HasValidSubActors())
					{
						FHierarchicalLODUtilities::DeleteLODActor(CurrentParentActor);
					}
				}

				NewCluster->AddSubActor(InActor);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
