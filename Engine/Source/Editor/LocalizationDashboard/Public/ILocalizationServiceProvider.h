// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Core/Public/Features/IModularFeature.h"
#include "EditorStyleSet.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "LocalizationTargetTypes.h"

/**
 * Interface to talking with localization service providers.
 */
class ILocalizationServiceProvider : public IModularFeature
{
public:
	/**
	 * Virtual destructor
	 */
	virtual ~ILocalizationServiceProvider() {}

	virtual bool Initialize() = 0;
	virtual void Terminate() = 0;

	/** Get the localization service provider display name */
	virtual const FText GetDisplayName() const = 0;

	/** Get the localization service provider name */
	virtual const FName GetName() const = 0;

	/** 
	 * Create a settings widget for display in the localization dashboard.
	 * @returns a widget used to edit project-specific localization service settings.
	 */
	virtual void CustomizeSettingsDetails(IDetailCategoryBuilder& DetailCategoryBuilder) = 0;

	/** 
	 * Create a settings widget for display in the localization target editor.
	 * @returns a widget used to edit target-specific localization service settings.
	 */
	virtual void CustomizeTargetDetails(IDetailCategoryBuilder& DetailCategoryBuilder, ULocalizationTarget& Target) = 0;
};