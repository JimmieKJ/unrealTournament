// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITreeItem.h"
#include "ObjectKey.h"

namespace SceneOutliner
{
	/** Custom type to distinguish levels from level blueprints */
	struct FLevelBlueprintHandle
	{
		/** We reference level blueprints by their parent level, as they don't always exist */
		TWeakObjectPtr<ULevel> ParentLevel;

		/** A key that is used to reference the level */
		FObjectKey LevelKey;

		/** Construct this handle from a level */
		explicit FLevelBlueprintHandle(ULevel* InLevel) : ParentLevel(InLevel), LevelKey(InLevel) {}

		/** Get the level blueprint for the parent level (if it exists) */
		ULevelScriptBlueprint* GetLevelBlueprint()
		{
			if (auto* Level = ParentLevel.Get())
			{
				const bool bCreateIfNotFound = false;
				return Level->GetLevelScriptBlueprint(!bCreateIfNotFound);
			}
			return nullptr;
		}

		/** Create the level blueprint for the parent level (if it doesn't exist) */
		ULevelScriptBlueprint* GetOrCreateLevelBlueprint()
		{
			if (auto* Level = ParentLevel.Get())
			{
				const bool bCreateIfNotFound = true;
				return Level->GetLevelScriptBlueprint(!bCreateIfNotFound);
			}
			return nullptr;
		}

		friend uint32 GetTypeHash(const FLevelBlueprintHandle& Handle)
		{
			return GetTypeHash(Handle.LevelKey);
		}

		bool operator==(const FLevelBlueprintHandle& Other) const { return LevelKey == LevelKey; }
		bool operator!=(const FLevelBlueprintHandle& Other) const { return LevelKey != LevelKey; }
	};

	/** A tree item that represents a level blueprint */
	struct FLevelBlueprintTreeItem : ITreeItem
	{
		/** The level containing the blueprint to which this tree item relates. */
		mutable FLevelBlueprintHandle Handle;

		/** Construct this item from a level or handle */
		FLevelBlueprintTreeItem(const FLevelBlueprintHandle& InHandle);

		/** Get this item's parent item. Always returns nullptr. */
		virtual FTreeItemPtr FindParent(const FTreeItemMap& ExistingItems) const override;

		/** Create this item's parent. Always returns nullptr. */
		virtual FTreeItemPtr CreateParent() const override;

 		/** Visit this tree item */
 		virtual void Visit(const ITreeItemVisitor& Visitor) const override;
 		virtual void Visit(const IMutableTreeItemVisitor& Visitor) override;

	public:

		/** Get the ID that represents this tree item. Used to reference this item in a map */
		virtual FTreeItemID GetID() const override;

		/** Get the raw string to display for this tree item - used for sorting */
		virtual FString GetDisplayString() const override;

		/** Get the sort priority given to this item's type */
		virtual int32 GetTypeSortPriority() const override;

		/** Check whether it should be possible to interact with this tree item */
		virtual bool CanInteract() const override;

		/** Generate a context menu for this item. Only called if *only* this item is selected. */
		virtual void GenerateContextMenu(FMenuBuilder& MenuBuilder, SSceneOutliner& Outliner) override;

		/** Populate the specified drag/drop payload with any relevant information for this type */
		virtual void PopulateDragDropPayload(FDragDropPayload& Payload) const override { }

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override { }

	public:

		/** Open the level blueprint contained by this tree item */
		void Open() const;
	};

}		// namespace SceneOutliner