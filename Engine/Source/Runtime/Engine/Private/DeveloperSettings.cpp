// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/DeveloperSettings.h"

UDeveloperSettings::UDeveloperSettings(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
	CategoryName = NAME_None;
	SectionName = NAME_None;
}

FName UDeveloperSettings::GetContainerName() const
{
	static const FName ProjectName(TEXT("Project"));
	static const FName EditorName(TEXT("Editor"));

	static const FName EditorSettingsName(TEXT("EditorSettings"));
	static const FName EditorPerProjectUserSettingsName(TEXT("EditorPerProjectUserSettings"));

	FName ConfigName = GetClass()->ClassConfigName;

	if ( ConfigName == EditorSettingsName || ConfigName == EditorPerProjectUserSettingsName )
	{
		return EditorName;
	}
	
	return ProjectName;
}

FName UDeveloperSettings::GetCategoryName() const
{
	static const FName GeneralName(TEXT("General"));
	static const FName EditorSettingsName(TEXT("EditorSettings"));
	static const FName EditorPerProjectUserSettingsName(TEXT("EditorPerProjectUserSettings"));

	if ( CategoryName != NAME_None )
	{
		return CategoryName;
	}

	FName ConfigName = GetClass()->ClassConfigName;
	if ( ConfigName == NAME_Engine || ConfigName == NAME_Input )
	{
		return NAME_Engine;
	}
	else if ( ConfigName == EditorSettingsName || ConfigName == EditorPerProjectUserSettingsName )
	{
		return GeneralName;
	}
	else if ( ConfigName == NAME_Editor || ConfigName == NAME_EditorSettings || ConfigName == NAME_EditorLayout || ConfigName == NAME_EditorKeyBindings )
	{
		return NAME_Editor;
	}
	else if ( ConfigName == NAME_Game )
	{
		return NAME_Game;
	}

	return NAME_Engine;
}

FName UDeveloperSettings::GetSectionName() const
{
	if ( SectionName != NAME_None )
	{
		return SectionName;
	}

	return GetClass()->GetFName();
}

#if WITH_EDITOR
FText UDeveloperSettings::GetSectionText() const
{
	return GetClass()->GetDisplayNameText();
}

FText UDeveloperSettings::GetSectionDescription() const
{
	return GetClass()->GetToolTipText();
}
#endif

TSharedPtr<SWidget> UDeveloperSettings::GetCustomSettingsWidget() const
{
	return TSharedPtr<SWidget>();
}
