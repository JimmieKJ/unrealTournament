// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineServiceMessages.generated.h"


/* Service discovery messages
 *****************************************************************************/

/**
 * Implements a message for discovering engine instances on the network.
 */
USTRUCT()
struct FEngineServicePing
{
	GENERATED_USTRUCT_BODY()
};


/**
 * Implements a message for responding to a request to discover engine instances on the network.
 */
USTRUCT()
struct FEngineServicePong
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the currently loaded level, if any. */
	UPROPERTY()
	FString CurrentLevel;

	/** Holds the engine version. */
	UPROPERTY()
	int32 EngineVersion;

	/** Holds a flag indicating whether game play has begun. */
	UPROPERTY()
	bool HasBegunPlay;

	/** Holds the instance identifier. */
	UPROPERTY()
	FGuid InstanceId;

	/** Holds the type of the engine instance. */
	UPROPERTY()
	FString InstanceType;

	/** Holds the identifier of the session that the application belongs to. */
	UPROPERTY()
	FGuid SessionId;

	/** Holds the time in seconds since the world was loaded. */
	UPROPERTY()
	float WorldTimeSeconds;
};


/* Authorization messages
 *****************************************************************************/

/**
 * Implements a message for denying service access to a remote user.
 */
USTRUCT()
struct FEngineServiceAuthDeny
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user that denied access. */
	UPROPERTY()
	FString UserName;

	/** Holds the name of the user that access is denied to. */
	UPROPERTY()
	FString UserToDeny;
};


/**
 * Implements a message for granting service access to a remote user.
 */
USTRUCT()
struct FEngineServiceAuthGrant
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user that granted access. */
	UPROPERTY()
	FString UserName;

	/** Holds the name of the user that access is granted to. */
	UPROPERTY()
	FString UserToGrant;

};


/* Command messages
 *****************************************************************************/

/**
 * Implements a message for executing a console command.
 */
USTRUCT()
struct FEngineServiceExecuteCommand
{
	GENERATED_USTRUCT_BODY()

	/** Holds the command to execute. */
	UPROPERTY()
	FString Command;

	/** Holds the name of the user that wants to execute the command. */
	UPROPERTY()
	FString UserName;

	/**
	 * Default constructor.
	 */
	FEngineServiceExecuteCommand( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FEngineServiceExecuteCommand( const FString& InCommand, const FString& InUserName )
		: Command(InCommand)
		, UserName(InUserName)
	{ }
};


/**
 * Implements a message for terminating the engine.
 */
USTRUCT()
struct FEngineServiceTerminate
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the user that wants to terminate the engine. */
	UPROPERTY()
	FString UserName;

	/**
	 * Default constructor.
	 */
	FEngineServiceTerminate( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FEngineServiceTerminate( const FString& InUserName )
		: UserName(InUserName)
	{ }
};


/* Status messages
 *****************************************************************************/

/**
 * Implements a message that contains a notification or log output.
 */
USTRUCT()
struct FEngineServiceNotification
{
	GENERATED_USTRUCT_BODY()

	/** Holds the notification text. */
	UPROPERTY()
	FString Text;

	/** Holds the time in seconds since the engine started. */
	UPROPERTY()
	double TimeSeconds;

	/**
	 * Default constructor.
	 */
	FEngineServiceNotification( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FEngineServiceNotification( const FString& InText, double InTimeSeconds )
		: Text(InText)
		, TimeSeconds(InTimeSeconds)
	{ }
};
