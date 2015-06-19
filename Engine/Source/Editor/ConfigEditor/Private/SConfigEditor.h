// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConfigPropertyHelper.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "ConfigEditor"

/*-----------------------------------------------------------------------------
   SConfigEditor
-----------------------------------------------------------------------------*/
class SConfigEditor : public SCompoundWidget
{
public:
	SLATE_USER_ARGS(SConfigEditor) {}
	SLATE_END_ARGS()

	// Begin SCompoundWidget|SWidget interface
	virtual void Construct(const FArguments& InArgs, TWeakObjectPtr<UProperty> InEditProperty);
	// End SCompoundWidget|SWidget interface

private:
	/**
	 * Handle a change of target platform in the Config Editor UI
	 */
	void HandleTargetPlatformChanged();
	
	// Panel used to select an available target platform.
	TSharedPtr<class STargetPlatformSelector> TargetPlatformSelection;
	
	/**
	 * Create the displayable area object for the selected platform
	 */
	void CreateDisplayObjectForSelectedTargetPlatform();

private:
	// This is an object we use to create a config hierarchy display for a property
	TWeakObjectPtr<UConfigHierarchyPropertyView> PropHelper;

	// The display area for the config hierarchy editor.
	TSharedPtr<SWidget> PropertyValueEditor;

	// We keep a cache of config files for the provided platforms.
	TSharedPtr<FConfigCacheIni> LocalConfigCache;

	// The bulk of the display of this hierarchy.
	TSharedPtr<IDetailsView> DetailsView;

	// Keep track of the property we are viewing.
	TWeakObjectPtr<UProperty> EditProperty;
};

#undef LOCTEXT_NAMESPACE // "ConfigEditor"