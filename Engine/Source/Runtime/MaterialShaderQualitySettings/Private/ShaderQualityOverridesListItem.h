// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShaderQualityOverridesListItem.generated.h"

// FShaderQualityOverridesListItem
// Helper struct for FMaterialShaderQualitySettingsCustomization, contains info required to populate a material quality row.
USTRUCT()
struct FShaderQualityOverridesListItem
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString RangeName;

	UPROPERTY()
	UBoolProperty* QualityProperty;

	UPROPERTY()
	class UShaderPlatformQualitySettings* SettingContainer;

	FShaderQualityOverridesListItem() {}

	FShaderQualityOverridesListItem(FString InRangeName, UBoolProperty* InQualityProperty, UShaderPlatformQualitySettings* InSettingContainer)
		: RangeName(InRangeName)
		  , QualityProperty(InQualityProperty)
		  , SettingContainer(InSettingContainer)
	{}

};

