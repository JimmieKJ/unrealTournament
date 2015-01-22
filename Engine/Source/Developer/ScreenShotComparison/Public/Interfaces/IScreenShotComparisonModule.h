// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IScreenShotComparisonModule.h: Declares the IScreenShotComparisonModule interface.
=============================================================================*/

#pragma once


/**
 * Interface for screen shot comparison UI modules.
 */
class IScreenShotComparisonModule
	: public IModuleInterface
{
public:
	
	/**
	 * Creates a screen shot comparison widget.
	 *
	 * @param InScreenShotManager - the manager containing the screen shot data
	 * @return The new widget
	 */
	virtual TSharedRef<class SWidget> CreateScreenShotComparison( const IScreenShotManagerRef& InScreenShotManager ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IScreenShotComparisonModule( ) { }
};