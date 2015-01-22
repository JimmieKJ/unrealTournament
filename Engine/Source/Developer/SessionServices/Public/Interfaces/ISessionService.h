// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of ISessionService. */
typedef TSharedPtr<class ISessionService> ISessionServicePtr;

/** Type definition for shared references to instances of ISessionService. */
typedef TSharedRef<class ISessionService> ISessionServiceRef;


/**
 * Interface for application session services.
 */
class ISessionService
{
public:

	/**
	 * Checks whether the service is running.
	 *
	 * @return true if the service is running, false otherwise.
	 */
	virtual bool IsRunning() = 0;

	/**
	 * Starts the service.
	 *
	 * @return true if the service was started, false otherwise.
	 * @see Stop
	 */
	virtual bool Start() = 0;

	/**
	 * Stops the service.
	 * @see Start
	 */
	virtual void Stop() = 0;

public:

	/** Virtual destructor. */
	virtual ~ISessionService() { }
};
