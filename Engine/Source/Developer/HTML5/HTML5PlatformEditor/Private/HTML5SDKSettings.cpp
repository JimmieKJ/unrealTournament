// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HTML5PlatformEditorPrivatePCH.h"

//#include "EngineTypes.h"
#include "HTML5SDKSettings.h"
#include "IHTML5TargetPlatformModule.h"
#include "PropertyHandle.h"
#include "DesktopPlatformModule.h"
#include "PlatformFilemanager.h"
#include "GenericPlatformFile.h"
#include "PlatformInfo.h"

#define LOCTEXT_NAMESPACE "FHTML5PlatformEditorModule"

DEFINE_LOG_CATEGORY_STATIC(HTML5SDKSettings, Log, All);

UHTML5SDKSettings::UHTML5SDKSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TargetPlatformModule(nullptr)
{
}

#if WITH_EDITOR
void UHTML5SDKSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!TargetPlatformModule)
	{
		TargetPlatformModule = FModuleManager::LoadModulePtr<IHTML5TargetPlatformModule>("HTML5TargetPlatform");
	}
	if (TargetPlatformModule)
	{
		UpdateGlobalUserConfigFile();
		TargetPlatformModule->RefreshAvailableDevices();
	}
}

#endif //WITH_EDITOR
#undef LOCTEXT_NAMESPACE