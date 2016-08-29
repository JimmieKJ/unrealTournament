// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"


// forward declarations
class ISessionManager;
class SWidget;


/**
 * Interface for launcher UI modules.
 */
class ISessionFrontendModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a session browser widget.
	 *
	 * @param SessionManager The session manager to use.
	 * @return The new session browser widget.
	 */
	virtual TSharedRef<SWidget> CreateSessionBrowser( const TSharedRef<ISessionManager>& SessionManager ) = 0;
	
	/**
	 * Creates a session console widget.
	 *
	 * @param SessionManager The session manager to use.
	 */
	virtual TSharedRef<SWidget> CreateSessionConsole( const TSharedRef<ISessionManager>& SessionManager ) = 0;

	/**
	 * Show the session frontend tab.
	 */
	virtual void InvokeSessionFrontend(FName SubTabToActivate = NAME_None) = 0;

public:

	/** Virtual destructor. */
	virtual ~ISessionFrontendModule() { }
};
