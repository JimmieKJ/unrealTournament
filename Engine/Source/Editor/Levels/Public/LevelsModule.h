// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ModuleInterface.h"

#include "LevelEdMode.h"

/**
 * The module holding all of the UI related peices for Levels
 */
class FLevelsModule : public IModuleInterface
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

	/**
	 * Creates a Level Browser widget
	 */
	virtual TSharedRef<class SWidget> CreateLevelBrowser();
	
	/** Delegates to be called to extend the levels menus */
	DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<FExtender>, FLevelsMenuExtender, const TSharedRef<FUICommandList>);
	virtual TArray<FLevelsMenuExtender>& GetAllLevelsMenuExtenders() {return LevelsMenuExtenders;}

private:

	/** All extender delegates for the levels menus */
	TArray<FLevelsMenuExtender> LevelsMenuExtenders;
};


