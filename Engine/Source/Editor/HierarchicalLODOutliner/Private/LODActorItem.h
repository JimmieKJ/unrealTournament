// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ITreeItem.h"
#include "TreeItemID.h"

class ALODActor;
class AActor;

namespace HLODOutliner
{
	/** Helper class to manage moving arbitrary data onto an actor */
	struct FLODActorDropTarget : IDropTarget
	{
		/** The actor this tree item is associated with. */
		TWeakObjectPtr<ALODActor> LODActor;

		/** Construct this object out of an Actor */
		FLODActorDropTarget(ALODActor* InLODActor) : LODActor(InLODActor) {}

	public:
		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;
	private:
		/** Move a static mesh actor to a different LODActor (cluster) */
		void MoveToCluster(AActor* InActor, ALODActor* OldParentActor, ALODActor* NewParentActor);

		/** Add a static mesh actor to a LODActor (cluster) */
		void AddToCluster(AActor* InActor, ALODActor* NewParentActor);

		/** Merges ToMergeActor in the represented Cluster/LODActor */
		void MergeCluster(ALODActor* ToMergeActor);
	};

	struct FLODActorItem : ITreeItem
	{
		mutable TWeakObjectPtr<ALODActor> LODActor;
		mutable FTreeItemID ID;

		FLODActorItem(ALODActor* InLODActor);

		//~ Begin ITreeItem Interface.
		virtual bool CanInteract() const override;
		virtual void GenerateContextMenu(FMenuBuilder& MenuBuilder, SHLODOutliner& Outliner) override;
		virtual FString GetDisplayString() const override;
		virtual FSlateColor GetTint() const override;
		virtual FTreeItemID GetID() override;
		//~ End ITreeItem Interface.

		/** Returns the number of triangles for the represented LODActor in FText form */
		FText GetNumTrianglesAsText() const;

		/** Populate the specified drag/drop payload with any relevant information for this type */
		virtual void PopulateDragDropPayload(FDragDropPayload& Payload) const override;

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;
	};
};