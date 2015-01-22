// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// platforms which can have runtime threading switches
#define HAVE_RUNTIME_THREADING_SWITCHES			(PLATFORM_DESKTOP || PLATFORM_ANDROID || PLATFORM_IOS)


/**
 * Provides information about the application.
 */
class CORE_API FApp
{
public:

	/**
	 * Checks whether this application can render anything.
	 *
	 * @return true if the application can render, false otherwise.
	 */
	FORCEINLINE static bool CanEverRender()
	{
		return !IsRunningCommandlet() && !IsRunningDedicatedServer();
	}

	/**
	 * Gets the name of the version control branch that this application was built from.
	 *
	 * @return The branch name.
	 */
	static FString GetBranchName();

	/**
	 * Gets the application's build configuration, i.e. Debug or Shipping.
	 *
	 * @return The build configuration.
	 */
	static EBuildConfigurations::Type GetBuildConfiguration();

	/**
	 * Gets the date at which this application was built.
	 *
	 * @return Build date string.
	 */
	static FString GetBuildDate();

	/**
	 * Gets the name of the currently running game.
	 *
	 * @return The game name.
	 */
	FORCEINLINE static const TCHAR* GetGameName()
	{
		return GInternalGameName;
	}

	/**
	* Sets the name of the currently running game.
	*
	* @param InGameName Name of the curently running game.
	*/
	FORCEINLINE static void SetGameName(const TCHAR* InGameName)
	{
		// At the moment Strcpy is not safe as we don't check the buffer size on all platforms, so we use strncpy here.
		FCString::Strncpy(GInternalGameName, InGameName, ARRAY_COUNT(GInternalGameName));
		// And make sure the GameName string is null terminated.
		GInternalGameName[ARRAY_COUNT(GInternalGameName) - 1] = 0;
	}

	/**
	 * Gets the globally unique identifier of this application instance.
	 *
	 * Every running instance of the engine has a globally unique instance identifier
	 * that can be used to identify it from anywhere on the network.
	 *
	 * @return Instance identifier, or an invalid GUID if there is no local instance.
	 * @see GetSessionId
	 */
	FORCEINLINE static FGuid GetInstanceId()
	{
		return InstanceId;
	}

	/**
	 * Gets the name of this application instance.
	 *
	 * By default, the instance name is a combination of the computer name and process ID.
	 *
	 * @return Instance name string.
	 */
	static FString GetInstanceName()
	{
		return FString::Printf(TEXT("%s-%i"), FPlatformProcess::ComputerName(), FPlatformProcess::GetCurrentProcessId());
	}

	/**
	 * Gets the name of the application, i.e. "UE4" or "Rocket".
	 *
	 * @todo need better application name discovery. this is quite horrible and may not work on future platforms.
	 * @return Application name string.
	 */
	static FString GetName()
	{
		FString ExecutableName = FPlatformProcess::ExecutableName();

		int32 ChopIndex = ExecutableName.Find(TEXT("-"));

		if (ExecutableName.FindChar(TCHAR('-'), ChopIndex))
		{
			return ExecutableName.Left(ChopIndex);
		}

		if (ExecutableName.FindChar(TCHAR('.'), ChopIndex))
		{
			return ExecutableName.Left(ChopIndex);
		}

		return ExecutableName;
	}

	/**
	 * Gets the identifier of the session that this application is part of.
	 *
	 * A session is group of applications that were launched and logically belong together.
	 * For example, when starting a new session in UFE that launches a game on multiple devices,
	 * all engine instances running on those devices will have the same session identifier.
	 * Conversely, sessions that were launched separately will have different session identifiers.
	 *
	 * @return Session identifier, or an invalid GUID if there is no local instance.
	 * @see GetInstanceId
	 */
	FORCEINLINE static FGuid GetSessionId()
	{
		return SessionId;
	}

	/**
	 * Gets the name of the session that this application is part of, if any.
	 *
	 * @return Session name string.
	 */
	FORCEINLINE static FString GetSessionName()
	{
		return SessionName;
	}

	/**
	 * Gets the name of the user who owns the session that this application is part of, if any.
	 *
	 * If this application is part of a session that was launched from UFE, this function
	 * will return the name of the user that launched the session. If this application is
	 * not part of a session, this function will return the name of the local user account
	 * under which the application is running.
	 *
	 * @return Name of session owner.
	 */
	FORCEINLINE static FString GetSessionOwner()
	{
		return SessionOwner;
	}

	/**
	* Reports if the game name is empty
	*
	* @return true if the game name is empty
	*/
	FORCEINLINE static bool IsGameNameEmpty()
	{
		return (GInternalGameName[0] == 0);
	}

	/**
	 * Reports if the game name has been set
	 *
	 * @return true if the game name has been set
	 */
	FORCEINLINE static bool HasGameName()
	{
		return (IsGameNameEmpty() == false) && (FCString::Stricmp(GInternalGameName, TEXT("None")) != 0);
	}

	/**
	 * Initializes the application session.
	 */
	static void InitializeSession();

	/**
	 * Checks whether this application is a game.
	 *
	 * Returns true if a normal or PIE game is active (basically !GIsEdit or || GIsPlayInEditorWorld)
	 * This must NOT be accessed on threads other than the game thread!
	 * Use View->Family->EnginShowFlags.Game on the rendering thread.
	 *
	 * @return true if the application is a game, false otherwise.
	 */
	FORCEINLINE static bool IsGame()
	{
#if WITH_EDITOR
		return !GIsEditor || GIsPlayInEditorWorld || IsRunningGame();
#else
		return true;
#endif
	}

	/**
	 * Checks whether this application has been installed.
	 *
	 * Non-server desktop shipping builds are assumed to be installed.
	 *
	 * Installed applications may not write to the folder in which they exist since they are likely in a system folder (like "Program Files" in windows).
	 * Instead, they should write to the OS-specific user folder FPlatformProcess::UserDir() or application data folder FPlatformProcess::ApplicationSettingsDir()
	 * User Access Control info for windows Vista and newer: http://en.wikipedia.org/wiki/User_Account_Control
	 *
	 * To run an "installed" build without installing, use the -Installed command line parameter.
	 * To run a "non-installed" desktop shipping build, use the -NotInstalled command line parameter.
	 *
	 * @return true if the application is installed, false otherwise.
	 */
	static bool IsInstalled();

	/**
	 * Checks whether the engine components of this application have been installed.
	 *
	 * In binary UE4 releases, the engine can be installed while the game is not. The game IsInstalled()
	 * setting will take precedence over this flag.
	 *
	 * To override, pass -engineinstalled or -enginenotinstalled on the command line.
	 *
	 * @return true if the engine is installed, false otherwise.
	 */
	static bool IsEngineInstalled();

	/**
	 * Checks whether this is a standalone application.
	 *
	 * An application is standalone if it has not been launched as part of a session from UFE.
	 * If an application was launched from UFE, it is part of a session and not considered standalone.
	 *
	 * @return true if this application is standalone, false otherwise.
	 */
	FORCEINLINE static bool IsStandalone()
	{
		return Standalone;
	}

	/**
	 * Checks whether this application runs unattended.
	 *
	 * Unattended applications are not monitored by anyone or are unable to receive user input.
	 * This method can be used to determine whether UI pop-ups or other dialogs should be shown.
	 *
	 * @return true if the application runs unattended, false otherwise.
	 */
	static bool IsUnattended()
	{
		// FCommandLine::Get() will assert that the command line has been set.
		// This function may not be used before FCommandLine::Set() is called.
		static bool bIsUnattended = FParse::Param(FCommandLine::Get(), TEXT("UNATTENDED"));
		return bIsUnattended || GIsAutomationTesting;
	}

	/**
	 * Checks whether the application should run multi-threaded for performance critical features.
	 *
	 * This method is used for performance based threads (like rendering, task graph).
	 * This will not disable async IO or similar threads needed to disable hitching
	 *
	 * @return true if this isn't a server, has more than one core, does not have a -onethread command line options, etc.
	 */
#if HAVE_RUNTIME_THREADING_SWITCHES
	static bool ShouldUseThreadingForPerformance();
#else
	FORCEINLINE static bool ShouldUseThreadingForPerformance()
	{
	#if PLATFORM_HTML5
		return false;
	#else
		return true;
	#endif // PLATFORM_HTML5
	}
#endif // HAVE_RUNTIME_THREADING_SWITCHES

	/**
	 * Checks whether application is in benchmark mode.
	 *
	 * @return true if application is in benchmark mode, false otherwise.
	 */
	FORCEINLINE static bool IsBenchmarking()
	{
		return bIsBenchmarking;
	}

	/**
	 * Sets application benchmarking mode.
	 *
	 * @param bVal True sets application in benchmark mode, false sets to non-benchmark mode.
	 */
	static void SetBenchmarking(bool bVal)
	{
		bIsBenchmarking = bVal;
	}

	/**
	 * Gets time step in seconds if a fixed delta time is wanted.
	 *
	 * @return Time step in seconds for fixed delta time.
	 */
	FORCEINLINE static double GetFixedDeltaTime()
	{
		return FixedDeltaTime;
	}

	/**
	 * Sets time step in seconds if a fixed delta time is wanted.
	 *
	 * @param seconds Time step in seconds for fixed delta time.
	 */
	static void SetFixedDeltaTime(double Seconds)
	{
		FixedDeltaTime = Seconds;
	}

	/**
	 * Gets whether we want to use a fixed time step or not.
	 *
	 * @return True if using fixed time step, false otherwise.
	 */
	static bool UseFixedTimeStep()
	{
		static bool bUseFixedTimeStep = FParse::Param(FCommandLine::Get(), TEXT("UseFixedTimeStep"));
		return bUseFixedTimeStep;
	}

	/**
	 * Gets current time in seconds.
	 *
	 * @return Current time in seconds.
	 */
	FORCEINLINE static double GetCurrentTime()
	{
		return CurrentTime;
	}

	/**
	 * Sets current time in seconds.
	 *
	 * @param seconds - Time in seconds.
	 */
	static void SetCurrentTime(double Seconds)
	{
		CurrentTime = Seconds;
	}

	/**
	 * Gets previous value of CurrentTime.
	 *
	 * @return Previous value of CurrentTime.
	 */
	FORCEINLINE static double GetLastTime()
	{
		return LastTime;
	}

	/** Updates Last time to CurrentTime. */
	static void UpdateLastTime()
	{
		LastTime = CurrentTime;
	}

	/**
	 * Gets time delta in seconds.
	 *
	 * @return Time delta in seconds.
	 */
	FORCEINLINE static double GetDeltaTime()
	{
		return DeltaTime;
	}

	/**
	 * Sets time delta in seconds.
	 *
	 * @param seconds Time in seconds.
	 */
	static void SetDeltaTime(double Seconds)
	{
		DeltaTime = Seconds;
	}

	/**
	 * Get Volume Multiplier
	 * 
	 * @return Current volume multiplier
	 */
	FORCEINLINE static float GetVolumeMultiplier()
	{
		return VolumeMultiplier;
	}

	/**
	 * Set Volume Multiplier
	 * 
	 * @param  InVolumeMultiplier	new volume multiplier
	 */
	FORCEINLINE static void SetVolumeMultiplier(float InVolumeMultiplier)
	{
		VolumeMultiplier = InVolumeMultiplier;
	}

	/**
	 * Helper function to get UnfocusedVolumeMultiplier from config and store so it's not retrieved every frame
	 * 
	 * @return Volume multiplier to use when app loses focus
	 */
	static float GetUnfocusedVolumeMultiplier();

private:

	/** Holds the instance identifier. */
	static FGuid InstanceId;

	/** Holds the session identifier. */
	static FGuid SessionId;

	/** Holds the session name. */
	static FString SessionName;

	/** Holds the name of the user that launched session. */
	static FString SessionOwner;

	/** Holds a flag indicating whether this is a standalone session. */
	static bool Standalone;

	/** Holds a flag Whether we are in benchmark mode or not. */
	static bool bIsBenchmarking;

	/** Holds time step if a fixed delta time is wanted. */
	static double FixedDeltaTime;

	/** Holds current time. */
	static double CurrentTime;

	/** Holds previous value of CurrentTime. */
	static double LastTime;

	/** Holds current delta time in seconds. */
	static double DeltaTime;

	/** Use to affect the app volume when it loses focus */
	static float VolumeMultiplier;

	/** Read from config to define the volume when app loses focus */
	static float UnfocusedVolumeMultiplier;
};


/** Called to determine the result of IsServerForOnlineSubsystems() */
DECLARE_DELEGATE_RetVal_OneParam(bool, FQueryIsRunningServer, FName);


/**
 * @return true if there is a running game world that is a server (including listen servers), false otherwise
 */
CORE_API bool IsServerForOnlineSubsystems(FName WorldContextHandle);

/**
 * Sets the delegate used for IsServerForOnlineSubsystems
 */
CORE_API void SetIsServerForOnlineSubsystemsDelegate(FQueryIsRunningServer NewDelegate);
