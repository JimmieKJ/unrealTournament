// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * Tasks Browser module
 */
class FTaskBrowserModule : public IModuleInterface
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
	 * Creates a tasks browser widget
	 *
	 * @return	New class viewer widget
	 */
	virtual TSharedRef<class SWidget> CreateTaskBrowser();

};