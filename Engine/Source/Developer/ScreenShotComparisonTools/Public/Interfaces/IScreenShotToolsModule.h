// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IScreenShotToolsModule.h: Declares the IScreenShotToolsModule interface.
=============================================================================*/

#pragma once


/**
 * Interface for session core modules.
 */
class IScreenShotToolsModule
	: public IModuleInterface
{
public:

	/**
	* Get the session manager.
	*
	* @return The session manager.
	*/
	virtual IScreenShotManagerPtr GetScreenShotManager( ) = 0;

	/**
	* Update the screenshot data after it has been updated.
	*/
	virtual void UpdateScreenShotData( ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IScreenShotToolsModule( ) { }
};