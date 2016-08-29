// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "AnimViewportShowCommands.h"

#define LOCTEXT_NAMESPACE "AnimViewportShowCommands"

void FAnimViewportShowCommands::RegisterCommands()
{
	UI_COMMAND( ToggleGrid, "Grid", "Display Grid", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( AutoAlignFloorToMesh, "Auto Align Floor To Mesh", "Auto aligns floor to mesh bounds", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( MuteAudio, "Mute Audio", "Mutes audio from the preview", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND(ProcessRootMotion, "Process Root Motion", "Moves preview based on animation root motion", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND( ShowReferencePose, "Reference Pose", "Show reference pose on preview mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowRetargetBasePose, "Retarget Base Pose", "Show retarget Base pose on preview mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowBound, "Bound", "Show bound on preview mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowPreviewMesh, "Mesh", "Show the preview mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowMorphTargets, "MorphTargets", "Display Applied Morph Targets of the mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( ShowBoneNames, "Bone Names", "Display Bone Names in Viewport", EUserInterfaceActionType::ToggleButton, FInputChord() );

	// below 3 menus are radio button styles
	UI_COMMAND(ShowDisplayInfoBasic, "Basic", "Display Basic Mesh Info in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ShowDisplayInfoDetailed, "Detailed", "Display Detailed Mesh Info in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ShowDisplayInfoSkelControls, "Skeletal Controls", "Display selected skeletal control info in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(HideDisplayInfo, "None", "Hide All Display Info in Viewport", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND( ShowBoneWeight, "View Selected Bone Weight", "Display Selected Bone Weight in Viewport", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowRawAnimation, "Uncompressed Animation", "Display Skeleton With Uncompressed Animation Data", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowNonRetargetedAnimation, "NonRetargeted Animation", "Display Skeleton With non retargeted Animation Data", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowAdditiveBaseBones, "Additive Base", "Display Skeleton In Additive Base Pose", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowSourceRawAnimation, "Source Animation", "Display Skeleton In Source Raw Animation if you have Track Curves Modified.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowBakedAnimation, "Baked Animation", "Display Skeleton In Baked Raw Animation if you have Track Curves Modified.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowSockets, "Sockets", "Display socket hitpoints", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( ShowBoneDrawNone, "None", "Hides bone selection", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND( ShowBoneDrawSelected, "Selected Only", "Shows only the selected bone", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND( ShowBoneDrawAll, "All Hierarchy", "Shows all hierarchy joints", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND( ShowLocalAxesNone, "None", "Hides all local hierarchy axis", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( ShowLocalAxesSelected, "Selected Only", "Shows only the local bone axis of the selected bones", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( ShowLocalAxesAll, "All", "Shows all local hierarchy axes", EUserInterfaceActionType::RadioButton, FInputChord() );

#if WITH_APEX_CLOTHING
	UI_COMMAND( DisableClothSimulation, "Disable Cloth Simulation", "Disable Cloth Simulation and Show non-simulated mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ApplyClothWind, "Apply Wind", "Apply Wind to Clothing", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( ShowClothSimulationNormals, "Simulation Normals", "Display Cloth Simulation Normals", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowClothGraphicalTangents, "Graphical Tangents", "Display Cloth Graphical Tangents", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowClothCollisionVolumes, "Collision Volumes", "Display Cloth Collision Volumes", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( EnableCollisionWithAttachedClothChildren, "Collide with Cloth Children", "Enables collision detection between collision primitives in the base mesh and clothing on any attachments in the preview scene.", EUserInterfaceActionType::ToggleButton, FInputChord() );	

	UI_COMMAND( ShowClothPhysicalMeshWire, "Physical Mesh Wire", "Display Simulation Mesh's wire frame. Shows all physical mesh loaded from an asset file when cloth simulation is disabled and shows simulated results when simulating. The part having zero max distance value will be drawn in pink.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowClothMaxDistances, "Max Distances", "Display Max Distances only when simulation is disabled. If turned on this option, disables cloth simulation automatically", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowClothBackstop, "Back stops", "Display Back stops. If it has a proper value, draws a line in red. Otherwise, draws a vertex in white.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ShowClothFixedVertices, "Fixed Vertices", "Display Fixed Vertices", EUserInterfaceActionType::ToggleButton, FInputChord() );	

	// below 3 menus are radio button styles
	UI_COMMAND(ShowAllSections, "Show All Sections", "Display All sections including Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ShowOnlyClothSections, "Show Only Cloth Sections", "Display Only Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(HideOnlyClothSections, "Hide Only Cloth Sections", "Display All Except Cloth Mapped Sections", EUserInterfaceActionType::RadioButton, FInputChord());

#endif// #if WITH_APEX_CLOTHING
}

#undef LOCTEXT_NAMESPACE