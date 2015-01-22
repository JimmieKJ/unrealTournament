// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationSettingsModulePrivatePCH.h"


/* UInternationalizationSettingsModel interface
 *****************************************************************************/

UInternationalizationSettingsModel::UInternationalizationSettingsModel( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


void UInternationalizationSettingsModel::SaveDefaults()
{
	FString SavedCultureName;
	GConfig->GetString( TEXT("Internationalization"), TEXT("Culture"), SavedCultureName, GEditorGameAgnosticIni );
	GConfig->SetString( TEXT("Internationalization"), TEXT("Culture"), *SavedCultureName, GEngineIni );

	bool bShouldLoadLocalizedPropertyNames = true;
	GConfig->GetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEditorGameAgnosticIni );
	GConfig->SetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEngineIni );
}


void UInternationalizationSettingsModel::ResetToDefault()
{
	FString SavedCultureName;
	GConfig->GetString( TEXT("Internationalization"), TEXT("Culture"), SavedCultureName, GEngineIni );
	GConfig->SetString( TEXT("Internationalization"), TEXT("Culture"), *SavedCultureName, GEditorGameAgnosticIni );

	bool bShouldLoadLocalizedPropertyNames = true;
	GConfig->GetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEngineIni );
	GConfig->SetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEditorGameAgnosticIni );

	SettingChangedEvent.Broadcast();
}


FString UInternationalizationSettingsModel::GetCultureName() const
{
	FString SavedCultureName;
	if( !GConfig->GetString( TEXT("Internationalization"), TEXT("Culture"), SavedCultureName, GEditorGameAgnosticIni ) )
	{
		GConfig->GetString( TEXT("Internationalization"), TEXT("Culture"), SavedCultureName, GEngineIni );
	}
	return SavedCultureName;
}


void UInternationalizationSettingsModel::SetCultureName(const FString& CultureName)
{
	GConfig->SetString( TEXT("Internationalization"), TEXT("Culture"), *CultureName, GEditorGameAgnosticIni );
	SettingChangedEvent.Broadcast();
}


bool UInternationalizationSettingsModel::ShouldLoadLocalizedPropertyNames() const
{
	bool bShouldLoadLocalizedPropertyNames = true;
	if( !GConfig->GetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEditorGameAgnosticIni ) )
	{
		GConfig->GetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEngineIni );
	}
	return bShouldLoadLocalizedPropertyNames;
}


void UInternationalizationSettingsModel::ShouldLoadLocalizedPropertyNames(const bool Value)
{
	GConfig->SetBool( TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), Value, GEditorGameAgnosticIni );
	SettingChangedEvent.Broadcast();
}
