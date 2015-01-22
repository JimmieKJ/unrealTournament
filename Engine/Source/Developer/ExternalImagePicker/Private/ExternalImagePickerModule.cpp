// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ExternalImagePickerPrivatePCH.h"
#include "IExternalImagePickerModule.h"
#include "SExternalImagePicker.h"

/**
 * Public interface for splash screen editor module
 */
class FExternalImagePickerModule : public IExternalImagePickerModule
{
public:
	virtual TSharedRef<SWidget> MakeEditorWidget(const FExternalImagePickerConfiguration& InConfiguration) override
	{
		return SNew(SExternalImagePicker)
			.TargetImagePath(InConfiguration.TargetImagePath)
			.DefaultImagePath(InConfiguration.DefaultImagePath)
			.OnExternalImagePicked(InConfiguration.OnExternalImagePicked)
			.OnGetPickerPath(InConfiguration.OnGetPickerPath)
			.MaxDisplayedImageDimensions(InConfiguration.MaxDisplayedImageDimensions)
			.RequiresSpecificSize(InConfiguration.bRequiresSpecificSize)
			.RequiredImageDimensions(InConfiguration.RequiredImageDimensions);
	}
};

IMPLEMENT_MODULE(FExternalImagePickerModule, ExternalImagePicker)