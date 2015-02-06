// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AndroidPlatformEditorPrivatePCH.h"
#include "AndroidSDKSettingsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyEditing.h"

#include "ScopedTransaction.h"
#include "SExternalImageReference.h"
#include "SHyperlinkLaunchURL.h"
#include "SPlatformSetupMessage.h"
#include "SFilePathPicker.h"
#include "PlatformIconInfo.h"
#include "SourceControlHelpers.h"
#include "ManifestUpdateHelper.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "ITargetPlatformManagerModule.h"

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
