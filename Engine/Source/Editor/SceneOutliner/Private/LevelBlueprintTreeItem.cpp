// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerPrivatePCH.h"

#include "LevelBlueprintTreeItem.h"
#include "LevelEditor.h"
#include "Runtime/Engine/Classes/Engine/LevelScriptBlueprint.h"

#include "SNotificationList.h"
#include "NotificationManager.h"

#include "LevelUtils.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_LevelBlueprintTreeItem"

namespace SceneOutliner
{

FLevelBlueprintTreeItem::FLevelBlueprintTreeItem(const FLevelBlueprintHandle& InHandle)
	: Handle(InHandle)
{

}

FTreeItemPtr FLevelBlueprintTreeItem::FindParent(const FTreeItemMap& ExistingItems) const
{
	if (SharedData->RepresentingWorld)
	{
		return ExistingItems.FindRef(SharedData->RepresentingWorld);
	}

	return nullptr;
}

FTreeItemPtr FLevelBlueprintTreeItem::CreateParent() const
{

	if (SharedData->RepresentingWorld)
	{
		return MakeShareable(new FWorldTreeItem(SharedData->RepresentingWorld));
	}

	return nullptr;
}

void FLevelBlueprintTreeItem::Visit(const ITreeItemVisitor& Visitor) const
{
	Visitor.Visit(*this);
}

void FLevelBlueprintTreeItem::Visit(const IMutableTreeItemVisitor& Visitor)
{
	Visitor.Visit(*this);
}

FTreeItemID FLevelBlueprintTreeItem::GetID() const
{
	return FTreeItemID(Handle);
}

FString FLevelBlueprintTreeItem::GetDisplayString() const
{
	FFormatOrderedArguments Args;
	if (ULevel* LevelPtr = Handle.ParentLevel.Get())
	{
		return FPackageName::GetShortName(LevelPtr->GetOutermost()->GetFName().GetPlainNameString());
	}
	else
	{
		return LOCTEXT("UnknownLevel", "Unknown Level").ToString();
	}
}

int32 FLevelBlueprintTreeItem::GetTypeSortPriority() const
{
	return ETreeItemSortOrder::Actor;
}

bool FLevelBlueprintTreeItem::CanInteract() const
{
	if (ULevel* Level = Handle.ParentLevel.Get())
	{
		const EObjectFlags InvalidSelectableFlags = RF_PendingKill|RF_BeginDestroyed|RF_Unreachable;
		if (!FLevelUtils::IsLevelVisible(Level) || FLevelUtils::IsLevelLocked(Level) || Level->HasAnyFlags(InvalidSelectableFlags))
		{
			return false;
		}
	}
	return Flags.bInteractive;
}

void FLevelBlueprintTreeItem::GenerateContextMenu(FMenuBuilder& MenuBuilder, SSceneOutliner& Outliner)
{
	const FSlateIcon LevelBlueprintIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.OpenLevelBlueprint");

	MenuBuilder.AddMenuEntry(LOCTEXT("OpenLevelBlueprint", "Edit Level Blueprint"), FText(), LevelBlueprintIcon, FExecuteAction::CreateSP(this, &FLevelBlueprintTreeItem::Open));
}

FDragValidationInfo FLevelBlueprintTreeItem::ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const
{
	return FDragValidationInfo::Invalid();
}

void FLevelBlueprintTreeItem::Open() const
{
	ULevelScriptBlueprint* LevelBlueprint = Handle.GetOrCreateLevelBlueprint();
	if (!LevelBlueprint || !FAssetEditorManager::Get().OpenEditorForAsset(LevelBlueprint))
	{
		FNotificationInfo Info(LOCTEXT("CannotOpenLevelBlueprint", "Unable to open the specified level blueprint."));
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		Info.bFireAndForget = true;
		Info.bUseSuccessFailIcons = true;
		FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
	}
}


}		// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE