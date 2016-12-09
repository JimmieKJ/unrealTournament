// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphModule.h"
#include "Textures/SlateIcon.h"
#include "AnimGraphCommands.h"
#include "Modules/ModuleManager.h"
#include "AnimNodeEditModes.h"
#include "EditorModeRegistry.h"
#include "AnimNodeEditMode.h"
#include "EditModes/TwoBoneIKEditMode.h"
#include "EditModes/ObserveBoneEditMode.h"
#include "EditModes/ModifyBoneEditMode.h"
#include "EditModes/FabrikEditMode.h"
#include "EditModes/PoseDriverEditMode.h"

IMPLEMENT_MODULE(FAnimGraphModule, AnimGraph);

#define LOCTEXT_NAMESPACE "AnimGraphModule"

void FAnimGraphModule::StartupModule()
{
	FAnimGraphCommands::Register();

	// Register the editor modes
	FEditorModeRegistry::Get().RegisterMode<FAnimNodeEditMode>(AnimNodeEditModes::AnimNode, LOCTEXT("AnimNodeEditMode", "Anim Node"), FSlateIcon(), false);
	FEditorModeRegistry::Get().RegisterMode<FTwoBoneIKEditMode>(AnimNodeEditModes::TwoBoneIK, LOCTEXT("TwoBoneIKEditMode", "2-Bone IK"), FSlateIcon(), false);
	FEditorModeRegistry::Get().RegisterMode<FObserveBoneEditMode>(AnimNodeEditModes::ObserveBone, LOCTEXT("ObserveBoneEditMode", "Observe Bone"), FSlateIcon(), false);
	FEditorModeRegistry::Get().RegisterMode<FModifyBoneEditMode>(AnimNodeEditModes::ModifyBone, LOCTEXT("ModifyBoneEditMode", "Modify Bone"), FSlateIcon(), false);
	FEditorModeRegistry::Get().RegisterMode<FFabrikEditMode>(AnimNodeEditModes::Fabrik, LOCTEXT("FabrikEditMode", "Fabrik"), FSlateIcon(), false);
	FEditorModeRegistry::Get().RegisterMode<FPoseDriverEditMode>(AnimNodeEditModes::PoseDriver, LOCTEXT("PoseDriverEditMode", "PoseDriver"), FSlateIcon(), false);
}

void FAnimGraphModule::ShutdownModule()
{
	// Unregister the editor modes
	FEditorModeRegistry::Get().UnregisterMode(AnimNodeEditModes::PoseDriver);
	FEditorModeRegistry::Get().UnregisterMode(AnimNodeEditModes::Fabrik);
	FEditorModeRegistry::Get().UnregisterMode(AnimNodeEditModes::ModifyBone);
	FEditorModeRegistry::Get().UnregisterMode(AnimNodeEditModes::ObserveBone);
	FEditorModeRegistry::Get().UnregisterMode(AnimNodeEditModes::TwoBoneIK);
	FEditorModeRegistry::Get().UnregisterMode(AnimNodeEditModes::AnimNode);
}

#undef LOCTEXT_NAMESPACE
