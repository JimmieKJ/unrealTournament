// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of ISessionInfo. */
typedef TSharedPtr<class ISessionInfo> ISessionInfoPtr;

/** Type definition for shared references to instances of ISessionInfo. */
typedef TSharedRef<class ISessionInfo> ISessionInfoRef;


/**
 * Delegate type for discovered session instances.
 *
 * The first parameter is the session that discovered an instance.
 * The second parameter is the discovered instance.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSessionInstanceDiscovered, const ISessionInfoRef&, const ISessionInstanceInfoRef&)

/**
 * Delegate type for received session log entries.
 *
 * The first parameter is the session that the log belongs to.
 * The second parameter is the engine instance that generated the log.
 * The third parameter is the new log message.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSessionLogReceived, const ISessionInfoRef&, const ISessionInstanceInfoRef&, const FSessionLogMessageRef&);


/**
 * Interface for game instances.
 */
class ISessionInfo
{
public:

	/**
	 * Gets a read-only collection of all instances that belong to this session.
	 *
	 * @param OutInstances Will hold the collection of instances.
	 */
	virtual void GetInstances( TArray<ISessionInstanceInfoPtr>& OutInstances ) const = 0;

	/**
	 * Gets the time at which the last update was received from this instance.
	 *
	 * @return The receive time.
	 */
	virtual FDateTime GetLastUpdateTime() = 0;

	/**
	 * Gets the number of engine instances that are part of the session.
	 *
	 * @return The number of engine instances.
	 */
	virtual const int32 GetNumInstances() const = 0;

	/**
	 * Gets the session identifier.
	 *
	 * @return Session identifier.
	 */
	virtual const FGuid& GetSessionId() const = 0;

	/**
	 * Gets the name of the session.
	 *
	 * @return Session name.
	 */
	virtual const FString& GetSessionName() const = 0;

	/**
	 * Gets the name of the user that owns the session.
	 *
	 * @return User name.
	 */
	virtual const FString& GetSessionOwner() const = 0;

	/**
	 * Checks whether this is a standalone session.
	 *
	 * A session is standalone if has not been created from the Launcher.
	 *
	 * @return true if this is a standalone session, false otherwise.
	 */
	virtual const bool IsStandalone() const = 0;

	/**
	 * Terminates the session.
	 */
	virtual void Terminate() = 0;

public:

	/**
	 * Returns a delegate that is executed when a new instance has been discovered.
	 *
	 * @return The delegate.
	 */
	virtual FOnSessionInstanceDiscovered& OnInstanceDiscovered() = 0;

	/**
	 * Returns a delegate that is executed when a new log message has been received.
	 *
	 * @return The delegate.
	 */
	virtual FOnSessionLogReceived& OnLogReceived() = 0;

public:

	/** Virtual destructor. */
	virtual ~ISessionInfo() { }
};
