// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SharedSettingsWidgetsPrivatePCH.h"
#include "SExternalImageReference.h"

#include "AssetSelection.h"
#include "EditorStyle.h"
#include "ISourceControlModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "SExternalImageReference"


void SExternalImageReference::Construct(const FArguments& InArgs, const FString& InBaseFilename, const FString& InOverrideFilename)
{
	FileDescription = InArgs._FileDescription;
	OnPreExternalImageCopy = InArgs._OnPreExternalImageCopy;
	OnPostExternalImageCopy = InArgs._OnPostExternalImageCopy;

	FExternalImagePickerConfiguration ImageReferenceConfig;
	ImageReferenceConfig.TargetImagePath = InOverrideFilename;
	ImageReferenceConfig.DefaultImagePath = InBaseFilename;
	ImageReferenceConfig.OnExternalImagePicked = FOnExternalImagePicked::CreateSP(this, &SExternalImageReference::HandleExternalImagePicked);
	ImageReferenceConfig.RequiredImageDimensions = InArgs._RequiredSize;
	ImageReferenceConfig.bRequiresSpecificSize = InArgs._RequiredSize.X >= 0;
	ImageReferenceConfig.MaxDisplayedImageDimensions = InArgs._MaxDisplaySize;
	ImageReferenceConfig.OnGetPickerPath = InArgs._OnGetPickerPath;

	ChildSlot
	[
		IExternalImagePickerModule::Get().MakeEditorWidget(ImageReferenceConfig)
	];
}


bool SExternalImageReference::HandleExternalImagePicked(const FString& InChosenImage, const FString& InTargetImage)
{
	if(OnPreExternalImageCopy.IsBound())
	{
		if(!OnPreExternalImageCopy.Execute(InChosenImage))
		{
			return false;
		}
	}

	FText FailReason;
	if(!SourceControlHelpers::CopyFileUnderSourceControl(InTargetImage, InChosenImage, LOCTEXT("ImageDescription", "image"), FailReason))
	{
		FNotificationInfo Info(FailReason);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return false;
	}
	
	if(OnPostExternalImageCopy.IsBound())
	{
		if(!OnPostExternalImageCopy.Execute(InChosenImage))
		{
			return false;
		}
	}

	return true;
}


#undef LOCTEXT_NAMESPACE