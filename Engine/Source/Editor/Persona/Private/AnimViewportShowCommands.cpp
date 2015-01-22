// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "AnimViewportShowCommands.h"

void FAnimViewportShowCommands::RegisterCommands()
{
	UI_COMMAND( ToggleGrid, "Grid", "Display Grid", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( ToggleFloor, "Floor", "Display Floor", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ToggleSky, "Sky", "Display Sky", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( MuteAudio, "Mute Audio", "Mutes audio from the preview", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND(ProcessRootMotion, "Process Root Motion", "Moves preview based on animation root motion", EUserInterfaceActionType::ToggleButton, FInputGesture());

	UI_COMMAND( ShowReferencePose, "Reference Pose", "Show reference pose on preview mesh", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowRetargetBasePose, "Retarget Base Pose", "Show retarget Base pose on preview mesh", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowBound, "Bound", "Show bound on preview mesh", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowPreviewMesh, "Mesh", "Show the preview mesh", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( ShowBones, "Bones", "Display Skeleton in viewport", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowBoneNames, "Bone Names", "Display Bone Names in Viewport", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	// below 3 menus are radio button styles
	UI_COMMAND(ShowDisplayInfoBasic, "Basic", "Display Basic Mesh Info in Viewport", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(ShowDisplayInfoDetailed, "Detailed", "Display Detailed Mesh Info in Viewport", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(HideDisplayInfo, "None", "Hide All Display Info in Viewport", EUserInterfaceActionType::RadioButton, FInputGesture());

	UI_COMMAND( ShowBoneWeight, "View Selected Bone Weight", "Display Selected Bone Weight in Viewport", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowRawAnimation, "Uncompressed Animation", "Display Skeleton With Uncompressed Animation Data", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowNonRetargetedAnimation, "NonRetargeted Animation", "Display Skeleton With non retargeted Animation Data", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowAdditiveBaseBones, "Additive Base", "Display Skeleton In Additive Base Pose", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowSourceRawAnimation, "Source Animation", "Display Skeleton In Source Raw Animation if you have Track Curves Modified.", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowBakedAnimation, "Baked Animation", "Display Skeleton In Baked Raw Animation if you have Track Curves Modified.", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowSockets, "Sockets", "Display socket hitpoints", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( ShowLocalAxesNone, "None", "Hides all local hierarchy axis", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ShowLocalAxesSelected, "Selected Hierarchy", "Shows only the local bone axis of the selected bones", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ShowLocalAxesAll, "All", "Shows all local hierarchy axes", EUserInterfaceActionType::RadioButton, FInputGesture() );

#if WITH_APEX_CLOTHING
	UI_COMMAND( DisableClothSimulation, "Disable Cloth Simulation", "Disable Cloth Simulation and Show non-simulated mesh", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ApplyClothWind, "Apply Wind", "Apply Wind to Clothing", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( ShowClothSimulationNormals, "Simulation Normals", "Display Cloth Simulation Normals", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowClothGraphicalTangents, "Graphical Tangents", "Display Cloth Graphical Tangents", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowClothCollisionVolumes, "Collision Volumes", "Display Cloth Collision Volumes", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( EnableCollisionWithAttachedClothChildren, "Collide with Cloth Children", "Enables collision detection between collision primitives in the base mesh and clothing on any attachments in the preview scene.", EUserInterfaceActionType::ToggleButton, FInputGesture() );	

	UI_COMMAND( ShowClothPhysicalMeshWire, "Physical Mesh Wire", "Display Simulation Mesh's wire frame. Shows all physical mesh loaded from an asset file when cloth simulation is disabled and shows simulated results when simulating. The part having zero max distance value will be drawn in pink.", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowClothMaxDistances, "Max Distances", "Display Max Distances only when simulation is disabled. If turned on this option, disables cloth simulation automatically", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowClothBackstop, "Back stops", "Display Back stops. If it has a proper value, draws a line in red. Otherwise, draws a vertex in white.", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ShowClothFixedVertices, "Fixed Vertices", "Display Fixed Vertices", EUserInterfaceActionType::ToggleButton, FInputGesture() );	

	// below 3 menus are radio button styles
	UI_COMMAND(ShowAllSections, "Show All Sections", "Display All sections including Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(ShowOnlyClothSections, "Show Only Cloth Sections", "Display Only Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(HideOnlyClothSections, "Hide Only Cloth Sections", "Display All Except Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputGesture());

#endif// #if WITH_APEX_CLOTHING
}
