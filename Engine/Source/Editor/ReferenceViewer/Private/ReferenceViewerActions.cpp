// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "ReferenceViewerActions.h"

//////////////////////////////////////////////////////////////////////////
// FReferenceViewerActions

#define LOCTEXT_NAMESPACE ""

void FReferenceViewerActions::RegisterCommands()
{
	UI_COMMAND(OpenSelectedInAssetEditor, "Edit Asset", "Opens the selected asset in the relevent editor.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReCenterGraph, "Re-Center Graph", "Re-centers the graph on this node, showing all referencers and dependencies for this asset instead", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ListReferencedObjects, "List Referenced Objects", "Shows a list of objects that the selected asset references.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ListObjectsThatReference, "List Objects That Reference", "Lists objects that reference the selected asset.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ShowSizeMap, "Size Map...", "Displays an interactive map showing the approximate size of this asset and everything it references", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ShowReferenceTree, "Show Reference Tree...", "Shows a reference tree for the selected asset.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(MakeLocalCollectionWithReferencedAssets, "Local", "Local. This collection is only visible to you and is not in source control.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MakePrivateCollectionWithReferencedAssets, "Private", "Private. This collection is only visible to you.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MakeSharedCollectionWithReferencedAssets, "Shared", "Shared. This collection is visible to everyone.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE