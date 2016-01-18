// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ModuleInterface.h"
#include "UnrealEd.h"

/**
* The module holding all of the UI related pieces for HLOD Outliner
*/
class FHierarchicalLODOutlinerModule : public IModuleInterface
{
public:
	/**
	* Called right after the module DLL has been loaded and the module object has been created
	*/
	virtual void StartupModule();

	/**
	* Called before the module is unloaded, right before the module object is destroyed.
	*/
	virtual void ShutdownModule();

	void OnHLODLevelsArrayChangedEvent();

	/** Creates the HLOD Outliner widget */
	virtual TSharedRef<SWidget> CreateHLODOutlinerWidget();

private:
	FDelegateHandle ArrayChangedDelegate;
};