// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "AssetViewerSettingsCustomization.h"
#include "AssetViewerSettings.h"

#define LOCTEXT_NAMESPACE "AssetViewerSettingsCustomizations"

TSharedRef<IDetailCustomization> FAssetViewerSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAssetViewerSettingsCustomization);
}

void FAssetViewerSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	UAssetViewerSettings* ViewerSettings = UAssetViewerSettings::Get();

	// Find profiles array property handle and hide it from settings view
	TSharedPtr<IPropertyHandle> ProfileHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetViewerSettings, Profiles));
	check(ProfileHandle->IsValidHandle());
	ProfileHandle->MarkHiddenByCustomization();

	// Create new category
	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory("Settings", LOCTEXT("AssetViewerSettingsCategory", "Settings"));

	const UEditorPerProjectUserSettings* PerProjectUserSettings = GetDefault<UEditorPerProjectUserSettings>();
	const int32 ProfileIndex = ViewerSettings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex) ? PerProjectUserSettings->AssetViewerProfileIndex : 0;

	ensureMsgf(ViewerSettings && ViewerSettings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex), TEXT("Invalid default settings pointer or current profile index"));

	// Add current active profile property child nodes (rest of profiles remain hidden)
	TSharedPtr<IPropertyHandle> ProfilePropertyHandle = ProfileHandle->GetChildHandle(ProfileIndex);
	checkf(ProfileHandle->IsValidHandle(), TEXT("Invalid indexing into profiles child properties"));	
	uint32 PropertyCount = 0;
	ProfilePropertyHandle->GetNumChildren(PropertyCount);
	for (uint32 PropertyIndex = 0; PropertyIndex < PropertyCount; ++PropertyIndex)
	{
		CategoryBuilder.AddProperty(ProfilePropertyHandle->GetChildHandle(PropertyIndex));		
	}
}

#undef LOCTEXT_NAMESPACE 
