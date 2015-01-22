// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __LevelViewportTabContent_h__
#define __LevelViewportTabContent_h__

#pragma once

#include "LevelEditor.h"
#include "LevelEditorActions.h"

/**
 * Represents the content in a viewport tab in the level editor.
 * Each SDockTab holding viewports in the level editor contains and owns one of these.
 */
class FLevelViewportTabContent : public TSharedFromThis<FLevelViewportTabContent>
{
public:
	/** Starts the tab content object and creates the initial layout based on the layout string */
	void Initialize(TSharedPtr<class SLevelEditor> InParentLevelEditor, TSharedPtr<SDockTab> InParentTab, const FString& InLayoutString);

	/** Returns whether the tab is currently shown */
	bool IsVisible() const;

	/** Returns an array of viewports inside this tab */
	const TArray< TSharedPtr< class SLevelViewport > >* GetViewports() const;

	/**
	 * Sets the current layout by changing the contained layout object
	 * 
	 * @param ConfigurationName		The name of the layout (for the names in namespace LevelViewportConfigurationNames)
	 */
	void SetViewportConfiguration(const FName& ConfigurationName);

	/**
	 * Returns whether the named layout is currently selected
	 *
	 * @param ConfigurationName		The name of the layout (for the names in namespace LevelViewportConfigurationNames)
	 * @return						True, if the named layout is currently active
	 */
	bool IsViewportConfigurationSet(const FName& ConfigurationName) const;

	/** @return True if this viewport belongs to the tab given */
	bool BelongsToTab(TSharedRef<class SDockTab> InParentTab) const;

	/** @return The string used to identify the layout of this viewport tab */
	const FString& GetLayoutString() const
	{
		return LayoutString;
	}

private:
	/**
	 * Updates state after the layout is changed by SetViewportConfiguration()
	 */
	void UpdateViewportTabWidget();

	/** Callback registered with the parent dock tab to persist state when needed */
	void SaveLayoutString(const FString LayoutString) const;

	/**
	 * Translates layout names from namespace LevelViewportConfigurationNames into layout class and returns a new object of that class
	 *
	 * @param TypeName	FName of the layout type from those in LevelViewportConfigurationNames
	 * @param bSwitchingLayouts	If true, the new layout is replacing an existing one.
	 * @return	A new layout of the requested type.
	 */
	static TSharedPtr< class FLevelViewportLayout > ConstructViewportLayoutByTypeName(const FName& TypeName, bool bSwitchingLayouts);

private:
	TWeakPtr<class SDockTab> ParentTab;
	TWeakPtr<class SLevelEditor> ParentLevelEditor;
	FString LayoutString;

	/** Current layout */
	TSharedPtr< class FLevelViewportLayout > ActiveLevelViewportLayout;
};

#endif
