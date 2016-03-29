// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InternationalizationSettingsModel.generated.h"


/**
 * Implements loading and saving of internationalization settings.
 */
UCLASS(config=EditorSettings)
class INTERNATIONALIZATIONSETTINGS_API UInternationalizationSettingsModel
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void ResetToDefault();
	bool GetEditorCultureName(FString& OutEditorCultureName) const;
	void SetEditorCultureName(const FString& CultureName);
	bool GetNativeGameCultureName(FString& OutNativeGameCultureName) const;
	void SetNativeGameCultureName(const FString& CultureName);
	bool ShouldLoadLocalizedPropertyNames() const;
	void ShouldLoadLocalizedPropertyNames(const bool Value);
	bool ShouldShowNodesAndPinsUnlocalized() const;
	void ShouldShowNodesAndPinsUnlocalized(const bool Value);
};