// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalLODOutlinerPrivatePCH.h"
#include "LevelEditor.h"
#include "LODActorItem.h"
#include "Engine/LODActor.h"
#include "HLODOutliner.h"
#include "HierarchicalLODUtilities.h"
#include "ScopedTransaction.h"
#include "SlateBasics.h"
#include "TreeItemID.h"

#define LOCTEXT_NAMESPACE "LODActorItem"

HLODOutliner::FLODActorItem::FLODActorItem(ALODActor* InLODActor)
	: LODActor(InLODActor), ID(InLODActor)
{
	Type = ITreeItem::HierarchicalLODActor;
}

bool HLODOutliner::FLODActorItem::CanInteract() const
{
	return true;
}

void HLODOutliner::FLODActorItem::GenerateContextMenu(FMenuBuilder& MenuBuilder, SHLODOutliner& Outliner)
{
	auto SharedOutliner = StaticCastSharedRef<SHLODOutliner>(Outliner.AsShared());

	MenuBuilder.AddMenuEntry(LOCTEXT("SelectLODActor", "Select LOD Actor"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SHLODOutliner::SelectLODActor)));

	MenuBuilder.AddMenuEntry(LOCTEXT("SelectContainedActors", "Select Contained Actors"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SHLODOutliner::SelectContainedActors)));

	if (LODActor->IsDirty())
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("BuildLODActorMesh", "Build LOD Mesh"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SHLODOutliner::BuildLODActor)));
	}
	else
	{		
		MenuBuilder.AddMenuEntry(LOCTEXT("ForceView", "ForceView"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SHLODOutliner::ForceViewLODActor)));

		MenuBuilder.AddMenuEntry(LOCTEXT("BuildLODActorMesh", "Rebuild LOD Mesh"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SHLODOutliner::RebuildLODActor)));
	}

	MenuBuilder.AddMenuEntry(LOCTEXT("CreateHLODVolume", "Create Containing Hierarchical Volume"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SHLODOutliner::CreateHierarchicalVolumeForActor)));

	AActor* Actor = LODActor.Get();
	ALODActor* ParentActor = FHierarchicalLODUtilities::GetParentLODActor(Actor);
	if (ParentActor && Parent.Pin()->GetTreeItemType() == TreeItemType::HierarchicalLODActor)
	{		
		MenuBuilder.AddMenuEntry(LOCTEXT("RemoveChildFromCluster", "Remove from cluster"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SHLODOutliner::RemoveLODActorFromCluster)));
	}
	else
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("DeleteCluster", "Delete Cluster"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(&Outliner, &SHLODOutliner::DeleteCluster)));
	}
}

FString HLODOutliner::FLODActorItem::GetDisplayString() const
{
	if (ALODActor* ActorPtr = LODActor.Get())
	{
		return ActorPtr->GetName() + ((ActorPtr->SubActors.Num() < 2) ? FString(" (Insufficient Sub Actors)") : ((ActorPtr->IsDirty()) ? FString(" (Not built)") : FString()));
	}

	return FString();
}

HLODOutliner::FTreeItemID HLODOutliner::FLODActorItem::GetID()
{
	return ID;
}

FSlateColor HLODOutliner::FLODActorItem::GetTint() const
{
	ALODActor* LODActorPtr = LODActor.Get();
	if (LODActorPtr)
	{
		return LODActorPtr->IsDirty() ? FSlateColor::UseSubduedForeground() : FLinearColor(1.0f, 1.0f, 1.0f);
	}

	return FLinearColor(1.0f, 1.0f, 1.0f);
}

FText HLODOutliner::FLODActorItem::GetNumTrianglesAsText() const
{
	if (LODActor.IsValid())
	{
		const uint32 SubActorCount = LODActor->GetNumTrianglesInSubActors();
		const uint32 MergedCount = LODActor->GetNumTrianglesInMergedMesh();
		const float PercentageOfOriginal = ((float)MergedCount / (float)SubActorCount) * 100.0f;
		return FText::FromString(FString::FromInt(SubActorCount) + TEXT(" (Original) - ") + FString::FromInt(MergedCount) + TEXT(" (Merged) - ") + FString::SanitizeFloat(PercentageOfOriginal) + "% (of Original)");
	}
	else
	{
		return FText::FromString(TEXT("Not available"));
	}
}

void HLODOutliner::FLODActorItem::PopulateDragDropPayload(FDragDropPayload& Payload) const
{
	ALODActor* ActorPtr = LODActor.Get();
	if (ActorPtr)
	{
		if (!Payload.LODActors)
		{
			Payload.LODActors = TArray<TWeakObjectPtr<AActor>>();
		}
		Payload.LODActors->Add(LODActor);
	}
}

HLODOutliner::FDragValidationInfo HLODOutliner::FLODActorItem::ValidateDrop(FDragDropPayload& DraggedObjects) const
{
	if (Parent.IsValid())
	{
		if (Parent.Pin()->GetTreeItemType() == ITreeItem::HierarchicalLODActor)
		{
			return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("CannotAddToChildCluster", "Cannot add to child cluster"));
		}
	}

	FLODActorDropTarget Target(LODActor.Get());
	return Target.ValidateDrop(DraggedObjects);
}

void HLODOutliner::FLODActorItem::OnDrop(FDragDropPayload& DraggedObjects, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	FLODActorDropTarget Target(LODActor.Get());
	Target.OnDrop(DraggedObjects, ValidationInfo, DroppedOnWidget);

	// Expand this HLOD actor item
	bIsExpanded = true;	
}

HLODOutliner::FDragValidationInfo HLODOutliner::FLODActorDropTarget::ValidateDrop(FDragDropPayload& DraggedObjects) const
{
	if (DraggedObjects.StaticMeshActors.IsSet() && DraggedObjects.StaticMeshActors->Num() > 0)
	{
		if (DraggedObjects.StaticMeshActors->Num() > 0 && DraggedObjects.LODActors->Num() == 0)
		{			
			if (DraggedObjects.bSceneOutliner == false)
			{
				bool bContaining = false;

				// Check if this StaticMesh Actor is not already inside this cluster
				for (int32 ActorIndex = 0; ActorIndex < DraggedObjects.StaticMeshActors->Num(); ++ActorIndex)
				{
					if (LODActor->SubActors.Contains(DraggedObjects.StaticMeshActors.GetValue()[ActorIndex]))
					{
						bContaining = true;
						break;
					}
				}

				if (!bContaining)
				{
					if (DraggedObjects.StaticMeshActors->Num() > 1)
					{
						return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleMoveToCluster, LOCTEXT("MoveMultipleToCluster", "Move Actors to Cluster"));
					}
					else
					{
						return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_CompatibleMoveToCluster, LOCTEXT("MoveToCluster", "Move Actor to Cluster"));
					}
				}
				else
				{
					return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("AlreadyInCluster", "Cannot add to existing cluster"));
				}
			}
			else
			{
				if (DraggedObjects.StaticMeshActors->Num() > 1)
				{
					return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleAddToCluster, LOCTEXT("AddMultipleToCluster", "Add Actors to Cluster"));
				}
				else
				{
					return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_CompatibleAddToCluster, LOCTEXT("AddToCluster", "Add Actor to Cluster"));
				}

			}			
		}

		if (DraggedObjects.bSceneOutliner)
		{
			return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("AlreadyInHLOD", "Actor is already in one of the Hierarchical LOD clusters"));
		}
	}
	else if (DraggedObjects.LODActors.IsSet() && DraggedObjects.LODActors->Num() > 0)
	{
		// Only valid if dragging inside the HLOD outliner
		if (!DraggedObjects.bSceneOutliner)
		{		
			bool bValidForMerge = true;
			bool bValidForChilding = true;
			int32 FirstLODLevel = -1;
			ULevel* Level = nullptr;

			for (auto Actor : DraggedObjects.LODActors.GetValue())
			{
				ALODActor* InLODActor = Cast<ALODActor>(Actor.Get());			

				if (InLODActor)
				{
					// If dragged onto self or already containing LODActor early out
					if (InLODActor == LODActor.Get() || LODActor->SubActors.Contains(InLODActor))
					{
						bValidForMerge = false;
						bValidForChilding = false;
						break;
					}

					// Check in case of multiple selected LODActor items to make sure all of them come from the same LOD level
					if (FirstLODLevel == -1)
					{
						FirstLODLevel = InLODActor->LODLevel;
					}

					if (InLODActor->LODLevel != LODActor->LODLevel)
					{
						bValidForMerge = false;

						if (InLODActor->LODLevel != FirstLODLevel)
						{
							bValidForChilding = false;
						}
					}		

					// Check if in same level asset
					if (Level == nullptr)
					{
						Level = InLODActor->GetLevel();
					}
					else if (Level != InLODActor->GetLevel())
					{
						bValidForMerge = false;
						bValidForChilding = false;
					}
				}
			}

			if (bValidForMerge)
			{
				return FDragValidationInfo((DraggedObjects.LODActors->Num() == 1) ? FHLODOutlinerDragDropOp::ToolTip_CompatibleMergeCluster : FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleMergeClusters, LOCTEXT("MergeHLODClusters", "Merge cluster(s)"));				
			}
			else if (bValidForChilding)
			{
				return FDragValidationInfo((DraggedObjects.LODActors->Num() == 1) ? FHLODOutlinerDragDropOp::ToolTip_CompatibleChildCluster : FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleChildClusters, LOCTEXT("ChildHLODClusters", "Child cluster(s)"));
			}
			else
			{
				return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("InvalidOperation", "Invalid Operation"));//"Cannot merge clusters from different Hierarchical LOD levels"));
			}
		}
	}

	return FDragValidationInfo(FHLODOutlinerDragDropOp::ToolTip_Incompatible, LOCTEXT("NotImplemented", "Not implemented"));
}

void HLODOutliner::FLODActorDropTarget::OnDrop(FDragDropPayload& DraggedObjects, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget)
{
	AActor* DropActor = LODActor.Get();
	if (!DropActor)
	{
		return;
	}

	auto& DraggedStaticMeshActors = DraggedObjects.StaticMeshActors.GetValue();
	auto& DraggedLODActors = DraggedObjects.LODActors.GetValue();
	if (ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_CompatibleMoveToCluster || ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleMoveToCluster)
	{
		for (int32 ActorIndex = 0; ActorIndex < DraggedStaticMeshActors.Num(); ++ActorIndex)
		{
			auto Actor = DraggedStaticMeshActors[ActorIndex];
			MoveToCluster(Actor.Get(), nullptr, LODActor.Get());
		}
	}
	else if (ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_CompatibleAddToCluster || ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleAddToCluster)
	{
		for (int32 ActorIndex = 0; ActorIndex < DraggedStaticMeshActors.Num(); ++ActorIndex)
		{
			auto Actor = DraggedStaticMeshActors[ActorIndex];
			AddToCluster(Actor.Get(), LODActor.Get());
		}
	}
	else if (ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_CompatibleMergeCluster || ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleMergeClusters)
	{
		for (int32 ActorIndex = 0; ActorIndex < DraggedLODActors.Num(); ++ActorIndex)
		{
			ALODActor* InLODActor = Cast<ALODActor>(DraggedLODActors[ActorIndex].Get());
			MergeCluster(InLODActor);
		}
	}
	else if (ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_CompatibleChildCluster || ValidationInfo.TooltipType == FHLODOutlinerDragDropOp::ToolTip_MultipleSelection_CompatibleChildClusters)
	{
		for (int32 ActorIndex = 0; ActorIndex < DraggedLODActors.Num(); ++ActorIndex)
		{
			auto Actor = DraggedLODActors[ActorIndex];
			MoveToCluster(Actor.Get(), nullptr, LODActor.Get());
		}
	}
}

void HLODOutliner::FLODActorDropTarget::MoveToCluster(AActor* InActor, ALODActor* OldParentActor, ALODActor* NewParentActor)
{
	const FScopedTransaction Transaction(LOCTEXT("UndoAction_MoveActorBetweenClusters", "Move Actor between Clusters"));
	InActor->Modify();
	ALODActor* CurrentParentActor = (OldParentActor) ? OldParentActor : FHierarchicalLODUtilities::GetParentLODActor(InActor);
	if (CurrentParentActor)
	{
		CurrentParentActor->Modify();
		CurrentParentActor->RemoveSubActor(InActor);
	}
	
	NewParentActor->Modify();
	NewParentActor->AddSubActor(InActor);
	
	GEngine->BroadcastHLODActorMoved(InActor, NewParentActor);
}

void HLODOutliner::FLODActorDropTarget::AddToCluster(AActor* InActor, ALODActor* NewParentActor)
{
	const FScopedTransaction Transaction(LOCTEXT("UndoAction_AddActorToCluster", "Add Actor To Cluster"));
	NewParentActor->Modify();
	InActor->Modify();

	NewParentActor->AddSubActor(InActor);
	GEngine->BroadcastHLODActorAdded(InActor, NewParentActor);
}

void HLODOutliner::FLODActorDropTarget::MergeCluster(ALODActor* ToMergeActor)
{
	const FScopedTransaction Transaction(LOCTEXT("UndoAction_MergeClusters", "Merge Clusters"));
	ToMergeActor->Modify();
	LODActor->Modify();

	while (ToMergeActor->SubActors.Num())
	{
		AActor* SubActor = ToMergeActor->SubActors.Last();
		MoveToCluster(SubActor, ToMergeActor, LODActor.Get());
	}
}

#undef LOCTEXT_NAMESPACE
