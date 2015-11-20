// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EditorStylePrivatePCH.h"

/* UEditorStyleSettings interface
 *****************************************************************************/

UEditorStyleSettings::UEditorStyleSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SelectionColor = FLinearColor(0.828f, 0.364f, 0.003f);
	InactiveSelectionColor = FLinearColor(0.25f, 0.25f, 0.25f);
	PressedSelectionColor = FLinearColor(0.701f, 0.225f, 0.003f);

	bOpenAssetEditorTabsInNewWindow = true;

	bShowFriendlyNames = true;
	LogTimestampMode = ELogTimes::None;
}

FLinearColor UEditorStyleSettings::GetSubduedSelectionColor() const
{
	FLinearColor SubduedSelectionColor = SelectionColor.LinearRGBToHSV();
	SubduedSelectionColor.G *= 0.55f;		// take the saturation 
	SubduedSelectionColor.B *= 0.8f;		// and brightness down

	return SubduedSelectionColor.HSVToLinearRGB();
}

#if WITH_EDITOR

void UEditorStyleSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UEditorStyleSettings, bEnableWindowAnimations))
	{
		FSlateApplication::Get().EnableMenuAnimations(bEnableWindowAnimations);
	}

//	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}

	SettingChangedEvent.Broadcast(PropertyName);
}

#endif
