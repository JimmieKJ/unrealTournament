// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClientUnitTest.h"


/** The list of registered unit test environments, mapped to game name (unit test modules should add to this, within StartupModule) */
extern TMap<FString, FUnitTestEnvironment*> UnitTestEnvironments;


/**
 * Class for handling per-game implementation of unit test environmental defaults
 */
class NETCODEUNITTEST_API FUnitTestEnvironment
{
	friend class FNetcodeUnitTest;

public:
	/**
	 * Registers the unit test environment, with the environment list
	 * IMPORTANT: This MUST be implemented in every subclass, and the subclass version MUST called, within your modules 'StartupModule'
	 */
	static void Register()
	{
		// Example
		//AddUnitTestEnvironment(TEXT("UE4"), new FUnitTestEnvironment());
	}

	/**
	 * Adds a new environment to the unit test environment list
	 *
	 * @param Game	The game the unit test environment is for
	 * @param Env	The environment being added
	 */
	static void AddUnitTestEnvironment(FString Game, FUnitTestEnvironment* Env);

	virtual ~FUnitTestEnvironment()
	{
	}

private:
	/**
	 * Winds-down the UnitTestEnvironments map; does not need implementation in subclasses
	 */
	static void Unregister()
	{
		TArray<FUnitTestEnvironment*> EnvList;

		UnitTestEnvironments.GenerateValueArray(EnvList);

		for (auto CurEntry : EnvList)
		{
			if (CurEntry != NULL)
			{
				delete CurEntry;
			}
		}

		UnitTestEnvironments.Empty();
	}

public:

	/**
	 * Returns the default map name, that should be used with unit tests, for the current game
	 *
	 * @param UnitTestFlags		The flags for the current unit test - affects chosen map in some games
	 * @return					Returns the default map name
	 */
	virtual FString GetDefaultMap(EUnitTestFlags UnitTestFlags);

	/**
	 * Returns the default parameters for launched servers
	 *
	 * @return	Returns the default parameters
	 */
	FString GetDefaultServerParameters();

	/**
	 * Sets up the default server parameters, specifying commandline and '-LogCmds=x' separately (combined within parent function above)
	 *
	 * @param OutCommandline	Passes in the current state of the commandline, for modification
	 * @param OutLogCmds		Passes in the current LogCmds list, for modification
	 */
	virtual void SetupDefaultServerParameters(FString& OutCommandline, FString& OutLogCmds)
	{
	}

	/**
	 * Returns the default parameters for launched clients
	 *
	 * @return	Returns the default parameters
	 */
	FString GetDefaultClientParameters();

	/**
	 * Sets up the default client parameters, specifying a (potentially already modified) commandline for modification
	 *
	 * @param OutCommandline	Passes in the current state of the commandline, for modification
	 */
	virtual void SetupDefaultClientParameters(FString& OutCommandline)
	{
	}

	/**
	 * Returns the default URL that fake clients should use, when connecting to a server, for the current game
	 *
	 * @return	Returns the default URL
	 */
	virtual FString GetDefaultClientConnectURL();

	/**
	 * Returns server log message, for the current game, that indicate progress in starting up
	 *
	 * @param OutStartProgressLogs	Outputs partial server log messages that indicate progress starting up
	 * @param OutReadyLogs			Outputs partial server log messages that indicate startup is complete
	 * @param OutTimeoutResetLogs	Outputs partial server log messages that indicate resetting unit-test/connection timeout is required
	 */
	void GetServerProgressLogs(const TArray<FString>*& OutStartProgressLogs, const TArray<FString>*& OutReadyLogs,
										const TArray<FString>*& OutTimeoutResetLogs);

	/**
	 * Called when initializing the static arrays, containing the progress logs
	 *
	 * @param StartProgressLogs		The static array containing logs indicating progress starting up
	 * @param ReadyLogs				The static array containing logs indicating startup is complete
	 * @param TimeoutResetLogs		The static array containing logs indicating resetting of unit-test/connection timeout is required
	 */
	virtual void InitializeServerProgressLogs(TArray<FString>& StartProgressLogs, TArray<FString>& ReadyLogs,
												TArray<FString>& TimeoutResetLogs)
	{
	}


	/**
	 * Executes game-specific handling, upon client unit tests initializing the client PlayerController
	 *
	 * @param UnitTestFlags	The flags for the current unit test - affects handling, depending upon game
	 * @param PC			The PlayerController being initialized
	 */
	virtual void HandleClientPlayer(EUnitTestFlags UnitTestFlags, APlayerController* PC)
	{
	}
};
