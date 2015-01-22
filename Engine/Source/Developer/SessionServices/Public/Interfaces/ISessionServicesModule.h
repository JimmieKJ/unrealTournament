// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


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
	virtual ISessionManagerRef GetSessionManager() = 0;

	/** 
	 * Gets the session service.
	 *
	 * @return The session service.
	 */
	virtual ISessionServiceRef GetSessionService() = 0;

public:

	/** Virtual destructor. */
	virtual ~ISessionServicesModule() { }
};
