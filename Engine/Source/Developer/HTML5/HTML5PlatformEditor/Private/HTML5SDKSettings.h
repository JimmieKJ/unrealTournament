// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITargetPlatformManagerModule.h"
#include "EditorStyleSet.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "IHTML5TargetPlatformModule.h"
#include "HTML5SDKSettings.generated.h"

class IHTML5TargetPlatformModule;

USTRUCT()
struct FHTML5DeviceMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = HTML5_Devices, Meta = (DisplayName = "Browser name"))
	FString DeviceName;

	UPROPERTY(EditAnywhere, Category = HTML5_Devices, Meta = (DisplayName = "Browser filepath"))
	FFilePath DevicePath;
};

/**
 * Implements the settings for the HTML5 SDK setup.
 */
UCLASS(config=Engine, globaluserconfig)
class HTML5PLATFORMEDITOR_API UHTML5SDKSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// Available browsers that can be used when launching HTML5 builds.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = HTML5_Devices, Meta = (DisplayName = "Available browsers"))
	TArray<FHTML5DeviceMapping> DeviceMap;

#if WITH_EDITOR

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
	ITargetPlatformManagerModule * TargetManagerModule;
#endif
private:
	IHTML5TargetPlatformModule* TargetPlatformModule;
};
