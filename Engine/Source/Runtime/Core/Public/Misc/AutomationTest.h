// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Flags for specifying automation test requirements/behavior */
namespace EAutomationTestFlags
{
	enum Type
	{
		ATF_None						= 0,			//empty flags for tests that require no flags

		//application type required for the test - not specifying means it will be valid for any application
		ATF_Editor						= 0x00000001,	// Test is suitable for running within the editor
		ATF_Game						= 0x00000002,	// Test is suitable for running within the game
		ATF_Commandlet					= 0x00000004,	// Test is suitable for running within a commandlet
		ATF_ApplicationMask				= ATF_Editor | ATF_Game | ATF_Commandlet,

		//features required for the test - not specifying means it is valid for any feature combination
		ATF_NonNullRHI					= 0x00000100,	// Test requires a non-null RHI to run correctly
		ATF_RequiresUser				= 0x00000200,	// Test requires a user instigated session
		ATF_FeatureMask					= ATF_NonNullRHI | ATF_RequiresUser,

		//action options - each autotest can have only one of this and this has to match
		ATF_VisualCommandlet			= 0x00010000,	//Test requires visual commandlet
		ATF_ActionMask					= ATF_VisualCommandlet,

		//speed of the test
		ATF_SmokeTest					= 0x01000000,	//Test should be run in "fast" mode
	};
};

/** Flags for specifying the type of test */
namespace EAutomationTestType
{
	enum Type
	{
		ATT_SmokeTest = 0x00000001,	// Smoke test
		ATT_NormalTest = 0x00000002,	// Normal test
		ATT_StressTest = 0x00000004	// Stress test
	};
};


/** Simple class to store the results of the execution of a automation test */
class FAutomationTestExecutionInfo
{
public:
	/** Constructor */
	FAutomationTestExecutionInfo() 
		: bSuccessful( false )
		, Duration(0.0f)
	{}

	/** Destructor */
	~FAutomationTestExecutionInfo()
	{
		Clear();
	}

	/** Helper method to clear out the results from a previous execution */
	void Clear()
	{
		Errors.Empty();
		Warnings.Empty();
		LogItems.Empty();
	}

	/** Whether the automation test completed successfully or not */
	bool bSuccessful;
	/** Any errors that occurred during execution */
	TArray<FString> Errors;
	/** Any warnings that occurred during execution */
	TArray<FString> Warnings;
	/** Any log items that occurred during execution */
	TArray<FString> LogItems;
	/** Time to complete the task */
	float Duration;
};

/** Simple class to store the automation test info */
class FAutomationTestInfo
{
public:

	// Default constructor
	FAutomationTestInfo( ) :
		TestType( 0 )
		, NumParticipantsRequired( 0 )
		, NumDevicesCurrentlyRunningTest( 0 )
	{}


	/**
	 * Constructor
	 *
	 * @param	InTestInfo - comma separated value string containing the test info
	 */
	FAutomationTestInfo( const FString& InTestInfo ) :
		NumParticipantsRequired( 0 )
		, NumDevicesCurrentlyRunningTest( 0 )
	{
		ParseStringInfo( InTestInfo );
	}

	/**
	 * Constructor
	 *
	 * @param	InDisplayName - Name used in the UI
	 * @param	InTestName - The test command string
	 * @param	InTestType - Type of test e.g. smoke test
	 * @param	InParameterName - optional parameter. e.g. asset name
	 */
	FAutomationTestInfo( const FString& InDisplayName, const FString& InTestName, const uint8 InTestType, const int32 InNumParticipantsRequired, const FString& InParameterName = FString() )
		: DisplayName( InDisplayName )
		, TestName( InTestName )
		, TestParameter( InParameterName )
		, TestType( InTestType )
		, NumParticipantsRequired( InNumParticipantsRequired )
		, NumDevicesCurrentlyRunningTest( 0 )
	{}

public:

	/**
	 * Add a test type if a parent node.
	 *
	 * @Param InTestType - the child test type to add.
	 */
	void AddTestType( const uint8 InTestType )
	{
		TestType |= InTestType;
	}


	/**
	 * Get the display name of this test.
	 *
	 * @return the display name.
	 */
	const FString& GetDisplayName() const
	{
		return DisplayName;
	}


	/**
	 * Get the test as a string so it can be sent over the network.
	 *
	 * @return The test as a string.
	 */
	const FString GetTestAsString()
	{
		return FString::Printf( TEXT("%s,%s,%i,%d,%s,"), *DisplayName, *TestName, TestType, NumParticipantsRequired, *TestParameter );
	}


	/**
	 * Get the test name of this test.
	 *
	 * @return The test name.
	 */
	const FString GetTestName() const
	{
		return TestName;
	}


	/**
	 * Get the type of parameter. This will be the asset name for linked assets.
	 *
	 * @return the parameter.
	 */
	const FString GetTestParameter() const
	{
		return TestParameter;
	}


	/**
	 * Get the type of test.
	 *
	 * @return the test type.
	 */
	const uint8 GetTestType() const
	{
		return TestType;
	}
	
	/**
	 * Zero the number of devices running this test
	 */
	void ResetNumDevicesRunningTest()
	{
		NumDevicesCurrentlyRunningTest = 0;
	}
	
	/**
	 * Be notified of a new device running the test so we should update our flag counting these
	 */
	void InformOfNewDeviceRunningTest()
	{
		NumDevicesCurrentlyRunningTest++;
	}
	
	/**
	 * Get the number of devices running this test
	 *
	 * @return The number of devices which have been given this test to run
	 */
	const int GetNumDevicesRunningTest() const
	{
		return NumDevicesCurrentlyRunningTest;
	}

	/**
	 * Get the number of participant this test needs in order to be run
	 *
	 * @return The number of participants needed
	 */
	const int32 GetNumParticipantsRequired() const
	{
		return NumParticipantsRequired;
	}


	/**
	 * Set the display name of the child node.
	 *
	 * @Param InDisplayName - the new child test name.
	 */
	void SetDisplayName( const FString& InDisplayName )
	{
		DisplayName = InDisplayName;
	}

	/**
	 * Set the number of participant this test needs in order to be run
	 *
	 * @Param NumRequired - The new number of participants needed
	 */
	void SetNumParticipantsRequired( int32 NumRequired )
	{
		NumParticipantsRequired = NumRequired;
	}


private:

	/**
	 * Set the test info from a string.
	 *
	 * @Param InTestInfo - the test information as a string.
	 */
	void ParseStringInfo( const FString& InTestInfo )
	{
		//Split New Test Name into string array
		TArray<FString> Pieces;
		InTestInfo.ParseIntoArray(Pieces, TEXT(","), true);

		// We should always have at least 3 parameters
		check( Pieces.Num() >= 3 )

		DisplayName = Pieces[0];
		TestName = Pieces[1];
		TestType = uint8( FCString::Atoi( *Pieces[2] ) );

		NumParticipantsRequired = FCString::Atoi( *Pieces[3] );

		// parameter 4 is optional
		if ( Pieces.Num() == 5 )
		{
			TestParameter = Pieces[4];
		}
	}

private:
	/** Display name used in the UI */
	FString DisplayName; 

	/** Test name used to run the test */
	FString TestName;

	/** Parameter - e.g. an asset name or map name */
	FString TestParameter;

	/** The test type - smoke test etc. */
	uint8 TestType;

	/** The number of participants this test requires */
	uint32 NumParticipantsRequired;

	/** The number of devices which have been given this test to run */
	uint32 NumDevicesCurrentlyRunningTest;
};


/**
 * Simple abstract base class for creating time deferred of a single test that need to be run sequentially (Loadmap & Wait, Open Editor & Wait, then execute...)
 */
class IAutomationLatentCommand : public TSharedFromThis<IAutomationLatentCommand>
{
public:
	/* virtual destructor */
	virtual ~IAutomationLatentCommand() {};

	/**
	 * Updates the current command and will only return TRUE when it has fulfilled its role (Load map has completed and wait time has expired)
	 */
	virtual bool Update() = 0;

private:
	/**
	 * Private update that allows for use of "StartTime"
	 */
	bool InternalUpdate()
	{
		if (StartTime == 0.0)
		{
			StartTime = FPlatformTime::Seconds();
		}
		return Update();
	}

protected:
	/** Default constructor*/
	IAutomationLatentCommand()
		: StartTime(0.0f)
	{
	}

	/** For timers, track the first time this ticks */
	float StartTime;

	friend class FAutomationTestFramework;
};

/**
 * Simple abstract base class for networked, multi-participant tests
 */
class IAutomationNetworkCommand : public TSharedFromThis<IAutomationNetworkCommand>
{
public:
	/* virtual destructor */
	virtual ~IAutomationNetworkCommand() {};

	/** 
	 * Identifier to distinguish which worker on the network should execute this command 
	 * 
	 * The index of the worker that should execute this command
	 */
	virtual uint32 GetRoleIndex() const = 0;
	
	/** Runs the network command */
	virtual void Run() = 0;
};

/**
 * Delegate type for when a test screenshot has been captured
 *
 * The first parameter is the width.
 * The second parameter is the height.
 * The third parameter is the array of bitmap data.
 * The fourth parameter is the screen shot filename.
 */
DECLARE_DELEGATE_FourParams(FOnTestScreenshotCaptured, int32, int32, const TArray<FColor>&, const FString&);

/** Class representing the main framework for running automation tests */
class CORE_API FAutomationTestFramework
{
public:
	/** Called right before unit testing is about to begin */
	FSimpleMulticastDelegate PreTestingEvent;
	/** Called after all unit tests have completed */
	FSimpleMulticastDelegate PostTestingEvent;

	/**
	 * Return the singleton instance of the framework.
	 *
	 * @return The singleton instance of the framework.
	 */
	static FAutomationTestFramework& GetInstance();

	/**
	 * Register a automation test into the framework. The automation test may or may not be necessarily valid
	 * for the particular application configuration, but that will be determined when tests are attempted
	 * to be run.
	 *
	 * @param	InTestNameToRegister	Name of the test being registered
	 * @param	InTestToRegister		Actual test to register
	 *
	 * @return	true if the test was successfully registered; false if a test was already registered under the same
	 *			name as before
	 */
	bool RegisterAutomationTest( const FString& InTestNameToRegister, class FAutomationTestBase* InTestToRegister );

	/**
	 * Unregister a automation test with the provided name from the framework.
	 *
	 * @return true if the test was successfully unregistered; false if a test with that name was not found in the framework.
	 */
	bool UnregisterAutomationTest( const FString& InTestNameToUnregister );

	/**
	 * Enqueues a latent command for execution on a subsequent frame
	 *
	 * @param NewCommand - The new command to enqueue for deferred execution
	 */
	void EnqueueLatentCommand(TSharedPtr<IAutomationLatentCommand> NewCommand);

	/**
	 * Enqueues a network command for execution in accordance with this workers role
	 *
	 * @param NewCommand - The new command to enqueue for network execution
	 */
	void EnqueueNetworkCommand(TSharedPtr<IAutomationNetworkCommand> NewCommand);

	/**
	 * Checks if a provided test is contained within the framework.
	 *
	 * @param InTestName	Name of the test to check
	 *
	 * @return	true if the provided test is within the framework; false otherwise
	 */
	bool ContainsTest( const FString& InTestName ) const;
		
	/**
	 * Attempt to run all fast smoke tests that are valid for the current application configuration.
	 *
	 * @return	true if all smoke tests run were successful, false if any failed
	 */
	bool RunSmokeTests();

	/**
	 * Reset status of worker (delete local files, etc)
	 */
	void ResetTests();

	/**
	 * Attempt to start the specified test.
	 *
	 * @param	InTestToRun			Name of the test that should be run
	 * @param	InRoleIndex			Identifier for which worker in this group that should execute a command
	 */
	void StartTestByName( const FString& InTestToRun, const int32 InRoleIndex );

	/**
	 * Stop the current test and return the results of execution
	 *
	 * @return	true if the test ran successfully, false if it did not (or the test could not be found/was invalid)
	 */
	bool StopTest( FAutomationTestExecutionInfo& OutExecutionInfo );

	/**
	 * Execute all latent functions that complete during update
	 *
	 * @return - true if the latent command queue is now empty and the test is complete
	 */
	bool ExecuteLatentCommands();

	/**
	 * Execute the next network command if you match the role, otherwise just dequeue
	 *
	 * @return - true if any network commands were in the queue to give subsequent latent commands a chance to execute next frame
	 */
	bool ExecuteNetworkCommands();

	/**
	 * Load any modules that are not loaded by default and have test classes in them
	 */
	void LoadTestModules();

	/**
	 * Populates the provided array with the names of all tests in the framework that are valid to run for the current
	 * application settings.
	 *
	 * @param	TestInfo	Array to populate with the test information
	 */
	void GetValidTestNames( TArray<FAutomationTestInfo>& TestInfo ) const;

	/**
	 * Whether the testing framework should allow content to be tested or not.  Intended to block developer directories.
	 * @param Path - Full path to the content in question
	 * @return - Whether this content should have tests performed on it
	 */
	bool ShouldTestContent(const FString& Path) const;

	/**
	 * Sets whether we want to include content in developer directories in automation testing
	 */
	void SetDeveloperDirectoryIncluded(const bool bInDeveloperDirectoryIncluded);

	/**
	 * Sets whether the automation tests should include visual commandlet.
	 */
	void SetVisualCommandletFilter(const bool bInVisualCommandletFilterOn);

	/**
	 * Accessor for delegate called when a png screenshot is captured 
	 */
	FOnTestScreenshotCaptured& OnScreenshotCaptured();

	/**
	 * Sets screenshot options
	 * @param bInScreenshotsEnabled - If screenshots are enabled
	 * @param bInUseFullSizeScreenshots - If true, we won't resize the screenshots
	 */
	void SetScreenshotOptions( const bool bInScreenshotsEnabled, const bool bInUseFullSizeScreenshots );

	/**
	 * Gets if screenshots are allowed
	 */
	bool IsScreenshotAllowed() const;

	/**
	 * Gets if we are using fulll size screenshots
	 */
	bool ShouldUseFullSizeScreenshots() const;

	/**
	 * Sets forcing smoke tests.
	 */
	void SetForceSmokeTests(const bool bInForceSmokeTests)
	{
		bForceSmokeTests = bInForceSmokeTests;
	}
private:

	/** Special feedback context used exclusively while automation testing */
	class FAutomationTestFeedbackContext : public FFeedbackContext
	{
	public:

		/** Constructor */
		FAutomationTestFeedbackContext() 
			: CurTest( NULL ) {}

		/** Destructor */
		~FAutomationTestFeedbackContext()
		{
			CurTest = NULL;
		}

		/**
		 * FOutputDevice interface
		 *
		 * @param	V		String to serialize within the context
		 * @param	Event	Event associated with the string
		 */
		virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

		/**
		 * Set the automation test associated with the feedback context. The automation test is where all warnings, errors, etc.
		 * will be routed to.
		 *
		 * @param	InAutomationTest	Automation test to associate with the feedback context.
		 */
		void SetCurrentAutomationTest( class FAutomationTestBase* InAutomationTest )
		{
			CurTest = InAutomationTest;
		}

	private:

		/** Associated automation test; all warnings, errors, etc. are routed to the automation test to track */
		class FAutomationTestBase* CurTest;
	};

	/** Helper method called to prepare settings for automation testing to follow */
	void PrepForAutomationTests();

	/** Helper method called after automation testing is complete to restore settings to how they should be */
	void ConcludeAutomationTests();

	/**
	 * Helper method to dump the contents of the provided test name to execution info map to the provided feedback context
	 *
	 * @param	InContext		Context to dump the execution info to
	 * @param	InInfoToDump	Execution info that should be dumped to the provided feedback context
	 */
	void DumpAutomationTestExecutionInfo( const TMap<FString, FAutomationTestExecutionInfo>& InInfoToDump );

	/**
	 * Internal helper method designed to simply start the provided test name.
	 *
	 * @param	InTestToRun			Name of the test that should be run
	 * @param	OutExecutionInfo	Results of executing the test
	 */
	void InternalStartTest( const FString& InTestToRun );

	/**
	 * Internal helper method designed to stop current executing test and return the results of execution.
	 *
	 * @return	true if the test was successfully run; false if it was not, could not be found, or is invalid for
	 *			the current application settings
	 */
	bool InternalStopTest(FAutomationTestExecutionInfo& OutExecutionInfo);


	/** Constructor */
	FAutomationTestFramework();

	/** Destructor */
	~FAutomationTestFramework();

	// Copy constructor and assignment operator intentionally left unimplemented
	FAutomationTestFramework( const FAutomationTestFramework& );
	FAutomationTestFramework& operator=( const FAutomationTestFramework& );

	/** Cached feedback context, contains the contents of GWarn at the time of automation testing, restored to GWarn when automation testing is complete */
	FFeedbackContext* CachedContext;

	/** Specialized feedback context used for automation testing */
	FAutomationTestFeedbackContext AutomationTestFeedbackContext;

	/** Mapping of automation test names to their respective object instances */
	TMap<FString, class FAutomationTestBase*> AutomationTestClassNameToInstanceMap;

	/** Queue of deferred commands */
	TQueue< TSharedPtr<IAutomationLatentCommand> > LatentCommands;

	/** Queue of deferred commands */
	TQueue< TSharedPtr<IAutomationNetworkCommand> > NetworkCommands;

	/** Whether we are currently executing smoke tests for startup/commandlet to minimize log spam */
	bool bRunningSmokeTests;

	/** Time when the test began executing */
	double StartTime;

	/** True if the execution of the test (but possibly not the latent actions) were successful */
	bool bTestSuccessful;

	/** Pointer to the current test being run */
	FAutomationTestBase* CurrentTest;

	/** Copy of the parameters for the active test */
	FString Parameters;

	/** Whether we want to run automation tests on content within the Developer Directories */
	bool bDeveloperDirectoryIncluded;

	/** Whether we want to convert Anim Blueprint **/
	bool bVisualCommandletFilterOn;

	/** Wheather screenshots are enabled */
	bool bScreenshotsEnabled;

	/** Wheather we should resize screenshots or not */
	bool bUseFullSizeScreenShots;

	/** Participation role as given by the automation controller */
	uint32 NetworkRoleIndex;

	/** Delegate called at the end of the frame when a screenshot is captured and a .png is requested */
	FOnTestScreenshotCaptured TestScreenshotCapturedDelegate;

	/** Forces running smoke tests */
	bool bForceSmokeTests;
};


/** Simple abstract base class for all automation tests */
class CORE_API FAutomationTestBase
{
public:
	/**
	 * Constructor
	 *
	 * @param	InName	Name of the test
	 */
	FAutomationTestBase( const FString& InName, const bool bInComplexTask )
		: bComplexTask( bInComplexTask )
		, bSuppressLogs( false )
		, TestName( InName )
	{
		// Register the newly created automation test into the automation testing framework
		FAutomationTestFramework::GetInstance().RegisterAutomationTest( InName, this );
	}

	/** Destructor */
	virtual ~FAutomationTestBase() 
	{ 
		// Unregister the automation test from the automation testing framework
		FAutomationTestFramework::GetInstance().UnregisterAutomationTest( TestName ); 
	}

	/**
	 * Pure virtual method; returns the flags associated with the given automation test
	 *
	 * @return	Automation test flags associated with the test
	 */
	virtual uint32 GetTestFlags() const = 0;

	/**
	 * Pure virtual method; returns the number of participants for this test
	 *
	 * @return	Number of required participants
	 */
	virtual uint32 GetRequiredDeviceNum() const = 0;

	/** Clear any execution info/results from a prior running of this test */
	void ClearExecutionInfo();

	/**
	 * Adds an error message to this test
	 *
	 * @param	InError	Error message to add to this test
	 */
	void AddError( const FString& InError );

	/**
	 * Adds a warning to this test
	 *
	 * @param	InWarning	Warning message to add to this test
	 */
	void AddWarning( const FString& InWarning );

	/**
	 * Adds a log item to this test
	 *
	 * @param	InLogItem	Log item to add to this test
	 */
	void AddLogItem( const FString& InLogItem );

	/**
	 * Returns whether this test has any errors associated with it or not
	 *
	 * @return true if this test has at least one error associated with it; false if not
 	 */
	bool HasAnyErrors() const;

	/**
	 * Forcibly sets whether the test has succeeded or not
	 *
	 * @param	bSuccessful	true to mark the test successful, false to mark the test as failed
	 */
	void SetSuccessState( bool bSuccessful );

	/**
	 * Populate the provided execution info object with the execution info contained within the test. Not particularly efficient,
	 * but providing direct access to the test's private execution info could result in errors.
	 *
	 * @param	OutInfo	Execution info to be populated with the same data contained within this test's execution info
	 */
	void GetExecutionInfo( FAutomationTestExecutionInfo& OutInfo ) const;

	/** 
	 * Helper function that will generate a list of sub-tests via GetTests
	 */
	void GenerateTestNames( TArray<FAutomationTestInfo>& TestInfo ) const;

	/**
	 * Is this a complex tast - if so it will be a stress test.
	 *
	 * @return true if this is a complex task.
	 */
	const bool IsComplexTask() const
	{
		return bComplexTask;
	}

	/**
	 * Used to suppress / unsuppress logs.
	 *
	 * @param bNewValue - True if you want to suppress logs.  False to unsuppress.
	 */
	void SetSuppressLogs(bool bNewValue)
	{
		bSuppressLogs = bNewValue;
	}

public:

	/**
	 * Logs an error if the two values are not equal.
	 *
	 * @param Description - Description text for the test.
	 * @param A - The first value.
	 * @param B - The second value.
	 *
	 * @see TestNotEqual
	 */
	template<typename ValueType> void TestEqual(const FString& Description, const ValueType& A, const ValueType& B)
	{
		if (A != B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not equal."), *Description));
		}
	}

	/**
	 * Logs an error if the specified Boolean value is not false.
	 *
	 * @param Description - Description text for the test.
	 * @param Value - The value to test.
	 *
	 * @see TestFalse
	 */
	void TestFalse(const FString& Description, bool Value)
	{
		if (Value)
		{
			AddError(FString::Printf(TEXT("%s: The value is not false."), *Description));
		}
	}

	/**
	 * Logs an error if the given shared pointer is valid.
	 *
	 * @param Description - Description text for the test.
	 * @param SharedPointer - The shared pointer to test.
	 *
	 * @see TestValid
	 */
	template<typename ValueType> void TestInvalid(const FString& Description, const TSharedPtr<ValueType>& SharedPointer)
	{
		if (SharedPointer.IsValid())
		{
			AddError(FString::Printf(TEXT("%s: The shared pointer is valid."), *Description));
		}
	}

	/**
	 * Logs an error if the two values are equal.
	 *
	 * @param Description - Description text for the test.
	 * @param A - The first value.
	 * @param B - The second value.
	 *
	 * @see TestEqual
	 */
	template<typename ValueType> void TestNotEqual(const FString& Description, const ValueType& A, const ValueType& B)
	{
		if (A == B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not equal."), *Description));
		}
	}

	/**
	 * Logs an error if the specified pointer is NULL.
	 *
	 * @param Description - Description text for the test.
	 * @param Pointer - The pointer to test.
	 *
	 * @see TestNull
	 */
	template<typename ValueType> void TestNotNull(const FString& Description, ValueType* Pointer)
	{
		if (Pointer == NULL)
		{
			AddError(FString::Printf(TEXT("%s: The pointer is NULL."), *Description));
		}
	}

	/**
	 * Logs an error if the two values are the same object in memory.
	 *
	 * @param Description - Description text for the test.
	 * @param A - The first value.
	 * @param B - The second value.
	 *
	 * @see TestSame
	 */
	template<typename ValueType> void TestNotSame(const FString& Description, const ValueType& A, const ValueType& B)
	{
		if (&A == &B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are the same."), *Description));
		}
	}

	/**
	 * Logs an error if the specified pointer is not NULL.
	 *
	 * @param Description - Description text for the test.
	 * @param Pointer - The pointer to test.
	 *
	 * @see TestNotNull
	 */
	template<typename ValueType> void TestNull(const FString& Description, ValueType* Pointer)
	{
		if (Pointer != NULL)
		{
			AddError(FString::Printf(TEXT("%s: The pointer is not NULL."), *Description));
		}
	}

	/**
	 * Logs an error if the two values are not the same object in memory.
	 *
	 * @param Description - Description text for the test.
	 * @param A - The first value.
	 * @param B - The second value.
	 *
	 * @see TestNotSame
	 */
	template<typename ValueType> void TestSame(const FString& Description, const ValueType& A, const ValueType& B)
	{
		if (&A != &B)
		{
			AddError(FString::Printf(TEXT("%s: The two values are not the same."), *Description));
		}
	}

	/**
	 * Logs an error if the specified Boolean value is not true.
	 *
	 * @param Description - Description text for the test.
	 * @param Value - The value to test.
	 *
	 * @see TestFalse
	 */
	void TestTrue(const FString& Description, bool Value)
	{
		if (!Value)
		{
			AddError(FString::Printf(TEXT("%s: The value is not true."), *Description));
		}
	}

	/**
	 * Logs an error if the given shared pointer is not valid.
	 *
	 * @param Description - Description text for the test.
	 * @param SharedPointer - The shared pointer to test.
	 *
	 * @see TestInvalid
	 */
	template<typename ValueType> void TestValid(const FString& Description, const TSharedPtr<ValueType>& SharedPointer)
	{
		if (!SharedPointer.IsValid())
		{
			AddError(FString::Printf(TEXT("%s: The shared pointer is not valid."), *Description));
		}
	}


protected:
	/**
	 * Asks the test to enumerate variants that will all go through the "RunTest" function with different parameters (for load all maps, this should enumerate all maps to load)\
	 *
	 * @param OutBeautifiedNames - Name of the test that can be displayed by the UI (for load all maps, it would be the map name without any directory prefix)
	 * @param OutTestCommands - The parameters to be specified to each call to RunTests (for load all maps, it would be the map name to load)
	 */
	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const = 0;
	
	/**
	 * Virtual call to execute the automation test.  
	 *
	 * @param Parameters - Parameter list for the test (but it will be empty for simple tests)
	 * @return TRUE if the test was run successfully; FALSE otherwise
	 */
	virtual bool RunTest(const FString& Parameters)=0;

	/**
	 * Returns the beautified test name
	 */
	virtual FString GetBeautifiedTestName() const = 0;

	//Flag to indicate if this is a complex task
	bool bComplexTask;

	/** Flag to suppress logs */
	bool bSuppressLogs;

	/** Name of the test */
	FString TestName;

	/** Info related to the last execution of this test */
	FAutomationTestExecutionInfo ExecutionInfo;

	//allow framework to call protected function
	friend class FAutomationTestFramework;

};



//////////////////////////////////////////////////////////////////////////
// Latent command definition macros

#define DEFINE_LATENT_AUTOMATION_COMMAND(CommandName)	\
class CommandName : public IAutomationLatentCommand \
	{ \
	public: \
	virtual ~CommandName() \
		{} \
		virtual bool Update() override; \
}

#define DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(CommandName,ParamType,ParamName)	\
class CommandName : public IAutomationLatentCommand \
	{ \
	public: \
	CommandName(ParamType InputParam) \
	: ParamName(InputParam) \
		{} \
		virtual ~CommandName() \
		{} \
		virtual bool Update() override; \
	private: \
	ParamType ParamName; \
}

#define DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND(CommandName)	\
class ENGINE_API CommandName : public IAutomationLatentCommand \
	{ \
	public: \
	virtual ~CommandName() \
		{} \
		virtual bool Update() override; \
}

#define DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(CommandName,ParamType,ParamName)	\
class ENGINE_API CommandName : public IAutomationLatentCommand \
	{ \
	public: \
	CommandName(ParamType InputParam) \
	: ParamName(InputParam) \
		{} \
		virtual ~CommandName() \
		{} \
		virtual bool Update() override; \
	private: \
	ParamType ParamName; \
}

//macro to simply the syntax for enqueueing a latent command
#define ADD_LATENT_AUTOMATION_COMMAND(ClassDeclaration) FAutomationTestFramework::GetInstance().EnqueueLatentCommand(MakeShareable(new ClassDeclaration));


//declare the class
#define START_NETWORK_AUTOMATION_COMMAND(ClassDeclaration)	\
class F##ClassDeclaration : public IAutomationNetworkCommand \
{ \
private:\
	int32 RoleIndex; \
public: \
	F##ClassDeclaration(int32 InRoleIndex) : RoleIndex(InRoleIndex) {} \
	virtual ~F##ClassDeclaration() {} \
	virtual uint32 GetRoleIndex() const override { return RoleIndex; } \
	virtual void Run() override 

//close the class and add to the framework
#define END_NETWORK_AUTOMATION_COMMAND(ClassDeclaration,InRoleIndex) }; \
	FAutomationTestFramework::GetInstance().EnqueueNetworkCommand(MakeShareable(new F##ClassDeclaration(InRoleIndex))); \

/**
 * Macros to simplify the creation of new automation tests. To create a new test one simply must put
 * IMPLEMENT_SIMPLE_AUTOMATION_TEST( NewAutomationClassName, AutomationClassFlags )
 * IMPLEMENT_COMPLEX_AUTOMATION_TEST( NewAutomationClassName, AutomationClassFlags )
 * in their cpp file, and then proceed to write an implementation for:
 * bool NewAutomationTestClassName::RunTest() {}
 * While the macro could also have allowed the code to be specified, leaving it out of the macro allows
 * the code to be debugged more easily.
 *
 * Builds supporting automation tests will automatically create and register an instance of the automation test within
 * the automation test framework as a result of the macro.
 */

#define IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE( TClass, PrettyName, TFlags ) \
	class TClass : public FAutomationTestBase \
	{ \
	public: \
		TClass( const FString& InName ) \
		:FAutomationTestBase( InName, false ) {} \
		virtual uint32 GetTestFlags() const override { return TFlags; } \
		virtual bool IsStressTest() const { return false; } \
		virtual uint32 GetRequiredDeviceNum() const override { return 1; } \
	protected: \
		virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override \
		{ \
			OutBeautifiedNames.Add(PrettyName); \
			OutTestCommands.Add(FString()); \
		} \
		virtual bool RunTest(const FString& Parameters) override; \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
	};

#define IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE( TClass, PrettyName, TFlags ) \
	class TClass : public FAutomationTestBase \
	{ \
	public: \
		TClass( const FString& InName ) \
		:FAutomationTestBase( InName, true ) {} \
		virtual uint32 GetTestFlags() const override { return (TFlags & ~(EAutomationTestFlags::ATF_SmokeTest)); } \
		virtual bool IsStressTest() const { return true; } \
		virtual uint32 GetRequiredDeviceNum() const override { return 1; } \
	protected: \
		virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override; \
		virtual bool RunTest(const FString& Parameters) override; \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
	};

#define IMPLEMENT_NETWORKED_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags, NumParticipants) \
	class TClass : public FAutomationTestBase \
	{ \
	public: \
		TClass( const FString& InName ) \
		:FAutomationTestBase( InName, false ) {} \
		virtual uint32 GetTestFlags() const override { return (TFlags & ~(EAutomationTestFlags::ATF_Editor | EAutomationTestFlags::ATF_Commandlet | EAutomationTestFlags::ATF_SmokeTest)); } \
		virtual uint32 GetRequiredDeviceNum() const override { return NumParticipants; } \
	protected: \
		virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override \
		{ \
			OutBeautifiedNames.Add(PrettyName); \
			OutTestCommands.Add(FString()); \
		} \
		virtual bool RunTest(const FString& Parameters) override; \
		virtual FString GetBeautifiedTestName() const override { return PrettyName; } \
	};


#if WITH_AUTOMATION_WORKER
	#define IMPLEMENT_SIMPLE_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}
	#define IMPLEMENT_COMPLEX_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}
	#define IMPLEMENT_NETWORKED_AUTOMATION_TEST(TClass, PrettyName, TFlags, NumParticipants) \
		IMPLEMENT_NETWORKED_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags, NumParticipants) \
		namespace\
		{\
			TClass TClass##AutomationTestInstance( TEXT(#TClass) );\
		}
#else
	#define IMPLEMENT_SIMPLE_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_SIMPLE_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags)
	#define IMPLEMENT_COMPLEX_AUTOMATION_TEST( TClass, PrettyName, TFlags ) \
		IMPLEMENT_COMPLEX_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags)
	#define IMPLEMENT_NETWORKED_AUTOMATION_TEST(TClass, PrettyName, TFlags, NumParticipants) \
		IMPLEMENT_NETWORKED_AUTOMATION_TEST_PRIVATE(TClass, PrettyName, TFlags, NumParticipants)
#endif // #if WITH_AUTOMATION_WORKER



