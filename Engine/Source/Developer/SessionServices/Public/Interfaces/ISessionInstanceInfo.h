// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of IGameInstanceInfo. */
typedef TSharedPtr<class ISessionInstanceInfo> ISessionInstanceInfoPtr;

/** Type definition for shared references to instances of IGameInstanceInfo. */
typedef TSharedRef<class ISessionInstanceInfo> ISessionInstanceInfoRef;


/**
 * Delegate type for received log entries.
 *
 * The first parameter is the engine instance that generated the log.
 * The second parameter is the new log message.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSessionInstanceLogReceived, const ISessionInstanceInfoRef&, const FSessionLogMessageRef&);


/**
 * Interface for game instances.
 */
class ISessionInstanceInfo
{
public:

	/**
	 * Executes a console command on this engine instance.
	 *
	 * @param CommandString The command to execute.
	 */
	virtual void ExecuteCommand( const FString& CommandString ) = 0;

	/**
	 * Gets instance's build date.
	 *
	 * @return The build date string.
	 */
	virtual const FString GetBuildDate() const = 0;

	/**
	 * Gets the name of the level that the instance is currently running.
	 *
	 * @return The name of the level.
	 */
	virtual const FString GetCurrentLevel() const = 0;

	/**
	 * Gets the name of the device that this instance is running on.
	 *
	 * @return The device name string.
	 */
	virtual const FString GetDeviceName() const = 0;

	/**
	 * Gets the instance's engine version number.
	 *
	 * @return Engine version.
	 */
	virtual int32 GetEngineVersion() const = 0;

	/**
	 * Gets the instance identifier.
	 *
	 * @return Instance identifier.
	 */
	virtual const FGuid GetInstanceId() const = 0;

	/**
	 * Gets the name of this instance.
	 *
	 * @return The instance name string.
	 */
	virtual const FString GetInstanceName() const = 0;

	/**
	 * Gets the instance type (i.e. Editor or Game).
	 *
	 * @return The game instance type string.
	 */
	virtual const FString GetInstanceType() const = 0;

	/**
	 * Gets the time at which the last update was received from this instance.
	 *
	 * @return The receive time.
	 */
	virtual FDateTime GetLastUpdateTime() = 0;

	/**
	 * Gets the collection of log entries received from this instance.
	 *
	 * @return Log entries.
	 */
	virtual const TArray<FSessionLogMessagePtr>& GetLog() = 0;

	/**
	 * Gets a reference to the session that owns this instance.
	 *
	 * @return Owner session.
	 */
	virtual TSharedPtr<class ISessionInfo> GetOwnerSession() = 0;

	/**
	 * Gets the name of the platform that the instance is running on.
	 *
	 * @return Platform name string.
	 */
	virtual const FString& GetPlatformName() const = 0;

	/**
	 * Gets the instance's current game world time.
	 *
	 * @return World time in seconds.
	 */
	virtual float GetWorldTimeSeconds() const = 0;

	/**
	 * Checks whether this instance is a console build (i.e. no editor features).
	 *
	 * @return true if it is a console build, false otherwise.
	 */
	virtual const bool IsConsole() const = 0;

	/**
	 * Checks whether this instance has already begun game play.
	 *
	 * @return true if game play has begun, false otherwise.
	 */
	virtual bool PlayHasBegun() const = 0;

	/**
	 * Terminates the instance.
	 */
	virtual void Terminate() = 0;

public:

	/**
	 * Returns a delegate that is executed when a new log message has been received.
	 *
	 * @return The delegate.
	 */
	virtual FOnSessionInstanceLogReceived& OnLogReceived() = 0;

public:

	/** Virtual destructor. */
	virtual ~ISessionInstanceInfo() { }
};
