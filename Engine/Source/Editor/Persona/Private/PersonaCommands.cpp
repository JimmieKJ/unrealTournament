// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "PersonaCommands.h"

#define LOCTEXT_NAMESPACE ""

void FPersonaCommands::RegisterCommands()
{
	// save menu
	UI_COMMAND(SaveAnimationAssets, "Save Animation Assets", "Only save all animation assets including skeletons, skeletal meshes and animation blueprints.", EUserInterfaceActionType::Button, FInputChord());

	// skeleton menu
	UI_COMMAND( ChangeSkeletonPreviewMesh, "Set Preview Mesh as Default", "Changes the skeletons default preview mesh to the current open preview mesh. The skeleton will require saving after this action.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( RemoveUnusedBones, "Remove Unused Bones from Skeleton", "Removes any bones from the skeleton that are not used by any of its meshes. The skeleton and associated animations will require saving after this action.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( AnimNotifyWindow, "Anim Notifies", "You can manage animation notifies that belong to the skeleton.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( RetargetManager, "Retarget Manager", "Manager retarget setups. ", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ImportMesh, "Import Mesh", "Import new mesh for this skeleton. ", EUserInterfaceActionType::Button, FInputChord() );

	// mesh menu
	UI_COMMAND( ReimportMesh, "Reimport Mesh", "Reimport the current mesh.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ImportLODs, "Import LODs", "Import LODs of the current mesh.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( AddBodyPart, "Add Body Part", "Add extra body parts for the current mesh", EUserInterfaceActionType::Button, FInputChord());

	// animation menu
	// import animation
	UI_COMMAND( ImportAnimation, "Import Animation", "Import new animation for the skeleton.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND( ReimportAnimation, "Reimport Animation", "Reimport current animation.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND( ApplyCompression, "Apply Compression", "Apply compression to current animation", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( RecordAnimation, "Record to new Animation", "Create new animation from currently playing", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ExportToFBX, "Export to FBX", "Export current animation to FBX", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( AddLoopingInterpolation, "Add Looping Interpolation", "Add an extra first frame at the end of the animation to create interpolation when looping", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( SetKey, "Set Key", "Add Bone Transform to Additive Layer Tracks", EUserInterfaceActionType::Button, FInputChord(EKeys::S) );
	UI_COMMAND( ApplyAnimation, "Apply Animation", "Apply Additive Layer Tracks to Runtime Animation Data", EUserInterfaceActionType::Button, FInputChord() );

	UI_COMMAND( UpdateSkeletonRefPose, "Update Skeleton RefPose", "Update Skeleton ref pose based on current preview mesh", EUserInterfaceActionType::Button, FInputChord() );

	UI_COMMAND( ToggleReferencePose, "Toggle Reference Pose", "Toggle Reference Pose", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( TogglePreviewAsset, "Toggle Preview ASset", "Toggle Preview Asset", EUserInterfaceActionType::ToggleButton, FInputChord() );
}

#undef LOCTEXT_NAMESPACE