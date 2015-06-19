// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"

#include "Settings/EditorSettings.h"

#define LOCTEXT_NAMESPACE "FEditorLoadingSavingSettingsCustomization"


/**
 * Implements a details view customization for UGameMapsSettingsCustomization objects.
 */
class FEditorLoadingSavingSettingsCustomization
	: public IDetailCustomization
{
public:

	/** Virtual destructor. */
	virtual ~FEditorLoadingSavingSettingsCustomization( ) { }

public:

	// IDetailCustomization interface

	virtual void CustomizeDetails( IDetailLayoutBuilder& LayoutBuilder ) override
	{
		CustomizeStartupCategory(LayoutBuilder);
	}

public:

	/**
	 * Creates a new instance.
	 *
	 * @return A new struct customization for game maps settings.
	 */
	static TSharedRef<IDetailCustomization> MakeInstance( )
	{
		return MakeShareable(new FEditorLoadingSavingSettingsCustomization());
	}

protected:

	/**
	 * Customizes the Startup property category.
	 *
	 * This customization pulls in a setting from the game agnostic Editor settings, which
	 * are stored in a different UObject, but which we would like to show in this section.
	 *
	 * @param LayoutBuilder The layout builder.
	 */
	void CustomizeStartupCategory( IDetailLayoutBuilder& LayoutBuilder )
	{
		IDetailCategoryBuilder& StartupCategory = LayoutBuilder.EditCategory("Startup");
		{
			TArray<UObject*> ObjectList;
			ObjectList.Add(GetMutableDefault<UEditorSettings>());
			StartupCategory.AddExternalProperty(ObjectList, "bLoadTheMostRecentlyLoadedProjectAtStartup");

			StartupCategory.AddExternalProperty(ObjectList, "bEditorAnalyticsEnabled", EPropertyLocation::Advanced);
		}
	}
};


#undef LOCTEXT_NAMESPACE
