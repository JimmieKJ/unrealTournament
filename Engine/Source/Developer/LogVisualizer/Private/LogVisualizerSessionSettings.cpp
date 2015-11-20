// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "Misc/CoreMisc.h"
#include "LogVisualizerSessionSettings.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "ISettingsModule.h"
#include "UnrealEdMisc.h"
#endif // WITH_EDITOR

ULogVisualizerSessionSettings::ULogVisualizerSessionSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bEnableGraphsVisualization = true;
}

#if WITH_EDITOR
void ULogVisualizerSessionSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
#if 0  //FIXME: should we save this settings too? (SebaK)
	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}
#endif
	SettingChangedEvent.Broadcast(Name);
}
#endif
