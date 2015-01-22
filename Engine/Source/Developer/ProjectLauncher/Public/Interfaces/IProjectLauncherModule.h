// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for launcher UI modules.
 */
class IProjectLauncherModule
	: public IModuleInterface
{
public:
	
	/**
	 * Creates a session launcher progress panel widget.
	 *
	 * @param LauncherWorker The launcher worker.
	 * @return The new widget.
	 */
	virtual TSharedRef<class SWidget> CreateSProjectLauncherProgressPanel( const ILauncherWorkerRef& LauncherWorker ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IProjectLauncherModule( ) { }
};
