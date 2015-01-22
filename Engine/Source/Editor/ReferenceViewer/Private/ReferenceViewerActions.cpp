// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "ReferenceViewerActions.h"

//////////////////////////////////////////////////////////////////////////
// FReferenceViewerActions

void FReferenceViewerActions::RegisterCommands()
{
	UI_COMMAND(OpenSelectedInAssetEditor, "Edit Asset", "Opens the selected asset in the relevent editor.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ReCenterGraph, "Re-Center Graph", "Re-centers the graph on this node, showing all referencers and dependencies for this asset instead", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ListReferencedObjects, "List Referenced Objects", "Shows a list of objects that the selected asset references.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ListObjectsThatReference, "List Objects That Reference", "Lists objects that reference the selected asset.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ShowReferenceTree, "Show Reference Tree...", "Shows a reference tree for the selected asset.", EUserInterfaceActionType::Button, FInputGesture());

	UI_COMMAND(MakeLocalCollectionWithReferencedAssets, "Local", "Local. This collection is only visible to you and is not in source control.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(MakePrivateCollectionWithReferencedAssets, "Private", "Private. This collection is only visible to you.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(MakeSharedCollectionWithReferencedAssets, "Shared", "Shared. This collection is visible to everyone.", EUserInterfaceActionType::Button, FInputGesture());
}
