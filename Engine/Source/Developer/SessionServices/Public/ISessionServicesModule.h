// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// forward declarations
class ISessionManager;
class ISessionService;


/**
 * Interface for session core modules.
 */
class ISessionServicesModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the session manager.
	 *
	 * @return The session manager.
	 */
	virtual TSharedRef<ISessionManager> GetSessionManager() = 0;

	/** 
	 * Gets the session service.
	 *
	 * @return The session service.
	 */
	virtual TSharedRef<ISessionService> GetSessionService() = 0;

public:

	/** Virtual destructor. */
	virtual ~ISessionServicesModule() { }
};
