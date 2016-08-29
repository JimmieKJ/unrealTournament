// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SceneDepthPickerModePrivatePCH.h"

IMPLEMENT_MODULE( FSceneDepthPickerModeModule, SceneDepthPickerMode );

void FSceneDepthPickerModeModule::StartupModule()
{
	FEditorModeRegistry::Get().RegisterMode<FEdModeSceneDepthPicker>(FBuiltinEditorModes::EM_SceneDepthPicker);
}

void FSceneDepthPickerModeModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_SceneDepthPicker);
}

void FSceneDepthPickerModeModule::BeginSceneDepthPickingMode(FOnSceneDepthLocationSelected InOnSceneDepthLocationSelected)
{
	// Activate the mode
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_SceneDepthPicker);

	// Set the required delegates
	FEdModeSceneDepthPicker* const Mode = GLevelEditorModeTools().GetActiveModeTyped<FEdModeSceneDepthPicker>(FBuiltinEditorModes::EM_SceneDepthPicker);
	if (ensure(Mode))
	{
		Mode->OnSceneDepthLocationSelected = InOnSceneDepthLocationSelected;
	}
}

void FSceneDepthPickerModeModule::EndSceneDepthPickingMode()
{
	GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_SceneDepthPicker);
}

bool FSceneDepthPickerModeModule::IsInSceneDepthPickingMode() const
{
	return GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_SceneDepthPicker);
}
