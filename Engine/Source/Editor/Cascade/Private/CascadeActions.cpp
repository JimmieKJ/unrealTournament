// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CascadeModule.h"
#include "CascadeActions.h"
#include "Cascade.h"

/** UI_COMMAND takes long for the compile to optimize */
PRAGMA_DISABLE_OPTIMIZATION
void FCascadeCommands::RegisterCommands()
{
	UI_COMMAND(RestartSimulation, "Restart Sim", "Restart Simulation", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RestartInLevel, "Restart Level", "Restart in Level", EUserInterfaceActionType::Button, FInputGesture(EKeys::SpaceBar));
	UI_COMMAND(SaveThumbnailImage, "Thumbnail", "Generate Thumbnail", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(CascadePlay, "Play/Pause", "Play/Pause Simulation", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(AnimSpeed_100, "100%", "100% Speed", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(AnimSpeed_50, "50%", "50% Speed", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(AnimSpeed_25, "25%", "25% Speed", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(AnimSpeed_10, "10%", "10% Speed", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(AnimSpeed_1, "1%", "1% Speed", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(RegenerateLowestLODDuplicatingHighest, "Regen LOD", "Regenerate Lowest LOD, Duplicating Highest", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RegenerateLowestLOD, "Regen LOD", "Regenerate Lowest LOD", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(JumpToHighestLOD, "Highest LOD", "Select Highest LOD", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(JumpToHigherLOD, "Higher LOD", "Select Higher LOD", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(AddLODAfterCurrent, "Add LOD", "Add LOD After Current", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(AddLODBeforeCurrent, "Add LOD", "Add LOD Before Current", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(JumpToLowerLOD, "Lower LOD", "Select Lower LOD", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(JumpToLowestLOD, "Lowest LOD", "Select Lowest LOD", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DeleteLOD, "Delete LOD", "Delete Selected LOD", EUserInterfaceActionType::Button, FInputGesture());

	UI_COMMAND(ToggleOriginAxis, "Origin Axis", "Display Origin Axis", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(View_ParticleCounts, "Particle Counts", "Display Particle Counts", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(View_ParticleEventCounts, "Particle Event Counts", "Display Particle Event Counts", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(View_ParticleTimes, "Particle Times", "Display Particle Times", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(View_ParticleMemory, "Particle Memory", "Display Particle Memory", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ViewMode_Wireframe, "Wireframe", "Select Wireframe Render Mode", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(ViewMode_Unlit, "Unlit", "Select Unlit Render Mode", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(ViewMode_Lit, "Lit", "Select Lit Render Mode", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(ViewMode_ShaderComplexity, "Shader Complexity", "Select Shader Complexity Render Mode", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(DetailMode_Low, "Low", "Select Low Detail Mode", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(DetailMode_Medium, "Medium", "Select Medium Detail Mode", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(DetailMode_High, "High", "Select High Detail Mode", EUserInterfaceActionType::RadioButton, FInputGesture());
	UI_COMMAND(ToggleGeometry, "Geometry", "Display preview mesh", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ToggleGeometry_Properties, "Geometry Properties", "Display Preview Mesh Properties", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ToggleLocalVectorFields, "Vector Fields", "Display Local Vector Fields", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ToggleOrbitMode, "Orbit Mode", "Toggle Orbit Mode", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ToggleMotion, "Motion", "Toggle Motion", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(SetMotionRadius, "Motion Radius", "Set Motion Radius", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ToggleBounds, "Bounds", "Display Bounds", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ToggleBounds_SetFixedBounds, "Set Fixed Bounds", "Set Fixed Bounds", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(TogglePostProcess, "Post Process", "Toggle Post Process", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ToggleGrid, "Grid", "Display Grid", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ToggleLoopSystem, "Loop", "Toggle Looping", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(ToggleRealtime, "Realtime", "Toggles real time rendering in this viewport", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(CascadeBackgroundColor, "Background Color", "Change Background Color", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ToggleWireframeSphere, "Wireframe Sphere", "Display Wireframe Sphere", EUserInterfaceActionType::ToggleButton, FInputGesture());

	UI_COMMAND(DeleteModule, "Delete Module", "Delete Module", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RefreshModule, "Refresh Module", "Refresh Module", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(SyncMaterial, "Sync Material", "Sync Material", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(UseMaterial, "Use Material", "Use Material", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DupeFromHigher, "Duplicate From Higher", "Duplicate From Higher LOD", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ShareFromHigher, "Share From Higher", "Share From Higher LOD", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DupeFromHighest, "Duplicate From Highest", "Duplicate From Second Highest LOD", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(SetRandomSeed, "Set Random Seed", "Set Random Seed", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ConvertToSeeded, "Convert To Seeded", "Convert To Seeded Module", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RenameEmitter, "Rename Emitter", "Rename Emitter", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DuplicateEmitter, "Duplicate Emitter", "Duplicate Emitter", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DuplicateShareEmitter, "Duplicate and Share Emitter", "Duplicate and Share Emitter", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DeleteEmitter, "Delete Emitter", "Delete Emitter", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ExportEmitter, "Export Emitter", "Export Emitter", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ExportAllEmitters, "Export All", "Export All Emitters", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(SelectParticleSystem, "Select Particle System", "Select Particle System", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(NewEmitterBefore, "Add New Emitter Before", "Add New Emitter Before Selected", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(NewEmitterAfter, "Add New Emitter After", "Add New Emitter After Selected", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(RemoveDuplicateModules, "Remove Duplicate Modules", "Remove Duplicate Modules", EUserInterfaceActionType::Button, FInputGesture());
}

PRAGMA_ENABLE_OPTIMIZATION