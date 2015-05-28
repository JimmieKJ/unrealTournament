// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Module includes
#include "IConfigEditorModule.h"
#include "SConfigEditor.h"

/*-----------------------------------------------------------------------------
   FConfigEditorModule
-----------------------------------------------------------------------------*/
class FConfigEditorModule : public IConfigEditorModule
{
public:
	// Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface interface


public:
	// Begin IConfigEditorModule interface
	virtual void CreateHierarchyEditor(UProperty* EditProperty) override;
	virtual void AddExternalPropertyValueWidgetAndConfigPairing(const FString& ConfigFile, const TSharedPtr<SWidget> ValueWidget) override;
	virtual TSharedRef<SWidget> GetValueWidgetForConfigProperty(const FString& ConfigFile) override;
	// End IConfigEditorModule interface	
	

private:
	/** 
	 * Creates a new Config editor tab 
	 *
	 * @return A spawned tab containing a config editor for a pre-defined property
	 */
	TSharedRef<class SDockTab> SpawnConfigEditorTab(const FSpawnTabArgs& Args);


private:
	// We use this to maintain a reference to a property value widget, ie. a combobox for enum, a check box for bool...
	// and provide these references when they are needed later in the details view construction
	// This is important.
	TMap<FString, TSharedPtr<SWidget>> ExternalPropertyValueWidgetAndConfigPairings;

	// Reference to the Config editor widget
	TSharedPtr<SConfigEditor> PropertyConfigEditor;

	// Reference to the property the hierarchy is to view.
	TWeakObjectPtr<UProperty> CachedPropertyToView;
};

