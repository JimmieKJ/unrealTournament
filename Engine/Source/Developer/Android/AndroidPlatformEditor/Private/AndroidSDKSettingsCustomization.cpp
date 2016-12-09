// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AndroidSDKSettingsCustomization.h"
#include "Modules/ModuleManager.h"
#include "Layout/Visibility.h"
#include "UnrealClient.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "AndroidSDKSettings.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"

#define LOCTEXT_NAMESPACE "AndroidSDKSettings"

//////////////////////////////////////////////////////////////////////////
// FAndroidSDKSettingsCustomization

TSharedRef<IDetailCustomization> FAndroidSDKSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAndroidSDKSettingsCustomization);
}

FAndroidSDKSettingsCustomization::FAndroidSDKSettingsCustomization()
{
	TargetPlatformManagerModule = &FModuleManager::LoadModuleChecked<ITargetPlatformManagerModule>("TargetPlatform");
}

void FAndroidSDKSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	SavedLayoutBuilder = &DetailLayout;
	BuildSDKPathSection(DetailLayout);
}


void FAndroidSDKSettingsCustomization::BuildSDKPathSection(IDetailLayoutBuilder& DetailLayout)
{
#if PLATFORM_MAC
	IDetailCategoryBuilder& SDKConfigCategory = DetailLayout.EditCategory(TEXT("SDKConfig"));

	// hide the property on Mac only
	TSharedRef<IPropertyHandle> JavaPathProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UAndroidSDKSettings, JavaPath));
	SDKConfigCategory.AddProperty(JavaPathProperty)
		.Visibility(EVisibility::Hidden);
#endif
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
