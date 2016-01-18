// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Persona commands */
class FPersonaCommands : public TCommands<FPersonaCommands>
{
public:
	FPersonaCommands()
		: TCommands<FPersonaCommands>( TEXT("Persona"), NSLOCTEXT("Contexts", "Persona", "Persona"), NAME_None, FEditorStyle::GetStyleSetName() )
	{
	}	

	virtual void RegisterCommands() override;

public:

	// save and load menu options
	// save all animation assets related to skeleton
	TSharedPtr<FUICommandInfo> SaveAnimationAssets;

	// skeleton menu options
	// Command to allow users to set the skeletons preview mesh
	TSharedPtr<FUICommandInfo> ChangeSkeletonPreviewMesh;
	// Command to allow users to remove unused bones (not referenced by any skeletalmesh) from the skeleton
	TSharedPtr<FUICommandInfo> RemoveUnusedBones;
	// Command to show Anim Notify window
	TSharedPtr<FUICommandInfo> AnimNotifyWindow;
	// Command to show Retarget Source Manager
	TSharedPtr<FUICommandInfo> RetargetManager;
	// Import Mesh for this Skeleton
	TSharedPtr<FUICommandInfo> ImportMesh;
			
	// mesh menu options
	// reimport current mesh
	TSharedPtr<FUICommandInfo> ReimportMesh;
	// import LODs
	TSharedPtr<FUICommandInfo> ImportLODs;
	// import Body part
	TSharedPtr<FUICommandInfo> AddBodyPart;

	// animation menu options
	// import animation
	TSharedPtr<FUICommandInfo> ImportAnimation;
	// reimport animation
	TSharedPtr<FUICommandInfo> ReimportAnimation;
	// record animation 
 	TSharedPtr<FUICommandInfo> RecordAnimation;
	// apply compression
	TSharedPtr<FUICommandInfo> ApplyCompression;
	// export to FBX
	TSharedPtr<FUICommandInfo> ExportToFBX;
	// Add looping interpolation
	TSharedPtr<FUICommandInfo> AddLoopingInterpolation;
	// set key for bone track
	TSharedPtr<FUICommandInfo> SetKey;
	// bake bone track curves to animation
	TSharedPtr<FUICommandInfo> ApplyAnimation;
	// Create Asset command
	TSharedPtr<FUICommandInfo> CreateAimOffset1D;
	TSharedPtr<FUICommandInfo> CreateAimOffset2D;
	TSharedPtr<FUICommandInfo> CreateBlendSpace1D;
	TSharedPtr<FUICommandInfo> CreateBlendSpace2D;
	TSharedPtr<FUICommandInfo> CreateAnimComposite;
	TSharedPtr<FUICommandInfo> CreateAanimMontage;
	// Command to allow users to remove unused bones (not referenced by any skeletalmesh) from the skeleton
	TSharedPtr<FUICommandInfo> UpdateSkeletonRefPose;

	// Toggle Reference Pose
	TSharedPtr<FUICommandInfo> ToggleReferencePose;
	TSharedPtr<FUICommandInfo> TogglePreviewAsset;
};
