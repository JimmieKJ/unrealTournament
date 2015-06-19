// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginCreatorPrivatePCH.h"
#include "PluginDescriptorObject.h"


UPluginDescriptorObject::UPluginDescriptorObject(const FObjectInitializer& ObjectInitializer)
{
	bRegenerateProjectFiles = true;
	bShowPluginFolder = true;
	FileVersion = 3;
	Version = 1;
	VersionName = TEXT("1.0");
	Category = TEXT("Other");
}

void UPluginDescriptorObject::FillDescriptor(FPluginDescriptor& OutDescriptor)
{
	OutDescriptor.bCanContainContent = bCanContainContent;
	OutDescriptor.bIsBetaVersion = bIsBetaVersion;
	OutDescriptor.Category = Category;
	OutDescriptor.CreatedBy = CreatedBy;
	OutDescriptor.CreatedByURL = CreatedByURL;
	OutDescriptor.Description = Description;
	OutDescriptor.DocsURL = DocsURL;
	OutDescriptor.FileVersion = FileVersion;
	OutDescriptor.FriendlyName = FriendlyName;
	OutDescriptor.Version = Version;
	OutDescriptor.VersionName = VersionName;
}
