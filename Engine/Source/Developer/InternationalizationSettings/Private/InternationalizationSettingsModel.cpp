// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationSettingsModulePrivatePCH.h"

/* UInternationalizationSettingsModel interface
 *****************************************************************************/

UInternationalizationSettingsModel::UInternationalizationSettingsModel( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }

void UInternationalizationSettingsModel::ResetToDefault()
{
	// Inherit editor culture from engine settings. Empty otherwise.
	FString SavedCultureName;
	GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), SavedCultureName, GEngineIni);
	GConfig->SetString( TEXT("Internationalization"), TEXT("Culture"), *SavedCultureName, GEditorSettingsIni );

	GConfig->SetString( TEXT("Internationalization"), TEXT("NativeGameCulture"), TEXT(""), GEditorSettingsIni );

	GConfig->SetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), true, GEditorSettingsIni );

	GConfig->SetBool( TEXT("Internationalization"), TEXT("ShowNodesAndPinsUnlocalized"), false, GEditorSettingsIni );

	GConfig->Flush(false, GEditorSettingsIni);
}

bool UInternationalizationSettingsModel::GetEditorCultureName(FString& OutEditorCultureName) const
{
	return GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), OutEditorCultureName, GEditorSettingsIni)
		|| GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), OutEditorCultureName, GEngineIni);
}

void UInternationalizationSettingsModel::SetEditorCultureName(const FString& CultureName)
{
	GConfig->SetString( TEXT("Internationalization"), TEXT("Culture"), *CultureName, GEditorSettingsIni );
	GConfig->Flush(false, GEditorSettingsIni);
}

bool UInternationalizationSettingsModel::GetNativeGameCultureName(FString& OutNativeGameCultureName) const
{
	return GConfig->GetString(TEXT("Internationalization"), TEXT("NativeGameCulture"), OutNativeGameCultureName, GEditorSettingsIni);
}

void UInternationalizationSettingsModel::SetNativeGameCultureName(const FString& CultureName)
{
	GConfig->SetString( TEXT("Internationalization"), TEXT("NativeGameCulture"), *CultureName, GEditorSettingsIni );
	GConfig->Flush(false, GEditorSettingsIni);
}

bool UInternationalizationSettingsModel::ShouldLoadLocalizedPropertyNames() const
{
	bool bShouldLoadLocalizedPropertyNames = true;
	GConfig->GetBool(TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEditorSettingsIni);
	return bShouldLoadLocalizedPropertyNames;
}

void UInternationalizationSettingsModel::ShouldLoadLocalizedPropertyNames(const bool Value)
{
	GConfig->SetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), Value, GEditorSettingsIni );
	GConfig->Flush(false, GEditorSettingsIni);
}

bool UInternationalizationSettingsModel::ShouldShowNodesAndPinsUnlocalized() const
{
	bool bShowNodesAndPinsUnlocalized = false;
	GConfig->GetBool(TEXT("Internationalization"), TEXT("ShowNodesAndPinsUnlocalized"), bShowNodesAndPinsUnlocalized, GEditorSettingsIni);
	return bShowNodesAndPinsUnlocalized;
}

void UInternationalizationSettingsModel::ShouldShowNodesAndPinsUnlocalized(const bool Value)
{
	GConfig->SetBool( TEXT("Internationalization"), TEXT("ShowNodesAndPinsUnlocalized"), Value, GEditorSettingsIni );
	GConfig->Flush(false, GEditorSettingsIni);
}
