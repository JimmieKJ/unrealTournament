// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of ILauncherAutomatedServiceProvider. */
typedef TSharedPtr<class ILauncherAutomatedServiceProvider> ILauncherAutomatedServiceProviderPtr;

/** Type definition for shared references to instances of ILauncherAutomatedServiceProvider. */
typedef TSharedRef<class ILauncherAutomatedServiceProvider> ILauncherAutomatedServiceProviderRef;


/**
 * Interface for launchers automated ServiceProvider
 */
class ILauncherAutomatedServiceProvider
{
public:

	/**
	 * Get the exit code of the service
	 *
	 * @return The exit Code of the service
	 */
	virtual int32 GetExitCode() = 0;
	
	/**
	 * Determine if the service is running
	 *
	 * @return if the service has work to continue with
	 */
	virtual bool IsRunning() = 0;
	
	/**
	 * Setup the service pre-requisites
	 *
	 * @param Params The settings to use for the service, I.e. Game/Config to deploy etc
	 */
	virtual void Setup( const TCHAR* Params ) = 0;
	
	/**
	 * Shutdown the service and cleanup anything we have control of.
	 */
	virtual void Shutdown() = 0;
	
	/**
	 * Tick the service so it can continue with its work.
	 *
	 * @param DeltaTime Time since last tick
	 */
	virtual void Tick( float DeltaTime ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~ILauncherAutomatedServiceProvider( ) { }
};
