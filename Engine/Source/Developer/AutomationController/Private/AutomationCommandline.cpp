// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutomationControllerPrivatePCH.h"


/** States for running the automation process */
namespace EAutomationTestState
{
	enum Type
	{
		Idle,				// Automation process is not running
		FindWorkers,		// Find workers to run the tests
		RequestTests,		// Find the tests that can be run on the workers
		DoingRequestedWork,	// Do whatever was requested from the commandline
		Complete			// The process is finished
	};
};

namespace EAutomationCommand
{
	enum Type
	{
		ListAllTests,				//List all tests for the session
		//RunSingleTest,			//Run one test specified by the commandline
		RunCommandLineTests,		//Run only tests that are listed on the commandline
		RunAll,						//Run all the tests that are supported
		RunFilter,                  //
		Quit						//quit the app when tests are done
	};
};


class FAutomationExecCmd : private FSelfRegisteringExec
{
public:
	void Init()
	{
		SessionID = FApp::GetSessionId();

		// Set state to FindWorkers to kick off the testing process
		AutomationTestState = EAutomationTestState::Idle;
		DelayTimer = 5.0f;

		// Load the automation controller
		IAutomationControllerModule* AutomationControllerModule = &FModuleManager::LoadModuleChecked<IAutomationControllerModule>("AutomationController");
		AutomationController = AutomationControllerModule->GetAutomationController();

		AutomationController->Init();

		const bool bSkipScreenshots = FParse::Param(FCommandLine::Get(), TEXT("NoScreenshots"));
		//TODO AUTOMATION Always use fullsize screenshots.
		const bool bFullSizeScreenshots = FParse::Param(FCommandLine::Get(), TEXT("FullSizeScreenshots"));
		const bool bSendAnalytics = FParse::Param(FCommandLine::Get(), TEXT("SendAutomationAnalytics"));
		AutomationController->SetScreenshotsEnabled(!bSkipScreenshots);
		AutomationController->SetUsingFullSizeScreenshots(true);

		// Register for the callback that tells us there are tests available
		AutomationController->OnTestsRefreshed().AddRaw(this, &FAutomationExecCmd::HandleRefreshTestCallback);

		TickHandler = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAutomationExecCmd::Tick));

		int32 NumTestLoops = 1;
		FParse::Value(FCommandLine::Get(), TEXT("TestLoops="), NumTestLoops);
		AutomationController->SetNumPasses(NumTestLoops);
		TestCount = 0;
		SetUpFilterMapping();
	}

	void SetUpFilterMapping()
	{
		FilterMaps.Empty();
		FilterMaps.Add("Engine", EAutomationTestFlags::EngineFilter);
		FilterMaps.Add("Smoke", EAutomationTestFlags::SmokeFilter);
		FilterMaps.Add("Stress", EAutomationTestFlags::StressFilter);
		FilterMaps.Add("Perf", EAutomationTestFlags::PerfFilter);
		FilterMaps.Add("Product", EAutomationTestFlags::ProductFilter);
	}
	
	void Shutdown()
	{
		IAutomationControllerModule* AutomationControllerModule = FModuleManager::GetModulePtr<IAutomationControllerModule>("AutomationController");
		if ( AutomationControllerModule )
		{
			AutomationController = AutomationControllerModule->GetAutomationController();
			AutomationController->OnTestsRefreshed().RemoveAll(this);
		}

		FTicker::GetCoreTicker().RemoveTicker(TickHandler);
	}

	bool IsTestingComplete()
	{
		// If the automation controller is no longer processing and we've reached the final stage of testing
		if ((AutomationController->GetTestState() != EAutomationControllerModuleState::Running) && (AutomationTestState == EAutomationTestState::Complete) && (AutomationCommandQueue.Num() == 0))
		{
			// If an actual test was ran we then will let the user know how many of them were ran.
			if (TestCount > 0)
			{
				// TODO AUTOMATION Don't report the wrong number of tests performed.
				// FAutomationTestFramework::GetInstance().LogQueueEmptyMessage();
				OutputDevice->Logf(TEXT("...Automation Test Queue Empty %d tests performed."), TestCount);
				TestCount = 0;
			}
			return true;
		}
		return false;
	}
	
	void GenerateTestNamesFromCommandLine(const TArray<FString>& AllTestNames, TArray<FString>& OutTestNames)
	{
		FString AllTestNamesNoWhiteSpaces;
		OutTestNames.Empty();
		
		//Split the test names up
		TArray<FString> Filters;
		StringCommand.ParseIntoArray(Filters, TEXT("+"), true);
		//trim cruft from all entries
		for (int32 FilterIndex = 0; FilterIndex < Filters.Num(); ++FilterIndex)
		{
			Filters[FilterIndex] = Filters[FilterIndex].Trim();
			Filters[FilterIndex] = Filters[FilterIndex].Replace(TEXT(" "), TEXT(""));
		}

		for ( int FilterIndex = 0; FilterIndex < Filters.Num(); ++FilterIndex )
		{
			for (int32 TestIndex = 0; TestIndex < AllTestNames.Num(); ++TestIndex)
			{
				AllTestNamesNoWhiteSpaces = AllTestNames[TestIndex];
				AllTestNamesNoWhiteSpaces = AllTestNamesNoWhiteSpaces.Replace(TEXT(" "), TEXT(""));
				if (AllTestNamesNoWhiteSpaces.Contains(Filters[FilterIndex]))
				{
					OutTestNames.Add(AllTestNames[TestIndex]);
					TestCount++;
					break;
				}
			}
		}
	}

	void FindWorkers(float DeltaTime)
	{
		DelayTimer -= DeltaTime;

		if (DelayTimer <= 0)
		{
			// Request the workers
			AutomationController->RequestAvailableWorkers(SessionID);
			AutomationTestState = EAutomationTestState::RequestTests;
		}
	}

	void HandleRefreshTestCallback()
	{
		TArray<FString> AllTestNames;

		// We have found some workers
		// Create a filter to add to the automation controller, otherwise we don't get any reports
		AutomationController->SetFilter(MakeShareable(new AutomationFilterCollection()));
		AutomationController->SetVisibleTestsEnabled(true);
		AutomationController->GetEnabledTestNames(AllTestNames);

		//assume we won't run any tests
		bool bRunTests = false;

		if (AutomationCommand == EAutomationCommand::ListAllTests)
		{
			//TArray<FAutomationTestInfo> TestInfo;
			//FAutomationTestFramework::GetInstance().GetValidTestNames(TestInfo);
			for (int TestIndex = 0; TestIndex < AllTestNames.Num(); ++TestIndex)
			{
				OutputDevice->Logf(TEXT("%s"), *AllTestNames[TestIndex]);
			}
			OutputDevice->Logf(TEXT("Found %i Automation Tests"), AllTestNames.Num());
			// Set state to complete
			AutomationTestState = EAutomationTestState::Complete;
		}
		else if (AutomationCommand == EAutomationCommand::RunCommandLineTests)
		{
			TArray <FString> FilteredTestNames;
			GenerateTestNamesFromCommandLine(AllTestNames, FilteredTestNames);
			if (FilteredTestNames.Num())
			{
				AutomationController->StopTests();
				AutomationController->SetEnabledTests(FilteredTestNames);
				bRunTests = true;
			}
			else
			{
				AutomationTestState = EAutomationTestState::Complete;
			}
		}
		else if (AutomationCommand == EAutomationCommand::RunFilter)
		{
			if (FilterMaps.Contains(StringCommand))
			{
				OutputDevice->Logf(TEXT("Running %i Automation Tests"), AllTestNames.Num());
				AutomationController->SetEnabledTests(AllTestNames);
				bRunTests = true;
			}
			else
			{
				AutomationTestState = EAutomationTestState::Complete;
				OutputDevice->Logf(TEXT("%s is not a valid flag to filter on! Valid options are: "), *StringCommand);
				TArray<FString> FlagNames;
				FilterMaps.GetKeys(FlagNames);
				for (int i = 0; i < FlagNames.Num(); i++)
				{
					OutputDevice->Log(FlagNames[i]);
				}
			}
		}
		else if (AutomationCommand == EAutomationCommand::RunAll)
		{
			bRunTests = true;
			TestCount = AllTestNames.Num();
		}

		if (bRunTests)
		{
			AutomationController->RunTests();

			// Set state to monitoring to check for test completion
			AutomationTestState = EAutomationTestState::DoingRequestedWork;
		}
	}

	void MonitorTests()
	{
		if (AutomationController->GetTestState() != EAutomationControllerModuleState::Running)
		{
			// We have finished the testing, and results are available
			AutomationTestState = EAutomationTestState::Complete;
		}
	}

	bool Tick(float DeltaTime)
	{
		// Update the automation controller to keep it running
		AutomationController->Tick();

		// Update the automation process
		switch (AutomationTestState)
		{
			case EAutomationTestState::FindWorkers:
			{
				FindWorkers(DeltaTime);
				break;
			}
			case EAutomationTestState::RequestTests:
			{
				break;
			}
			case EAutomationTestState::DoingRequestedWork:
			{
				MonitorTests();
				break;
			}
			case EAutomationTestState::Complete:
			case EAutomationTestState::Idle:
			default:
			{
				//pop next command
				if (AutomationCommandQueue.Num())
				{
					AutomationCommand = AutomationCommandQueue[0];
					AutomationCommandQueue.RemoveAt(0);
					if (AutomationCommand == EAutomationCommand::Quit)
					{
						if (AutomationCommandQueue.IsValidIndex(0))
						{
							// Add Quit back to the end of the array.
							AutomationCommandQueue.Add(EAutomationCommand::Quit);
							break;
						}
					}
					AutomationTestState = EAutomationTestState::FindWorkers;
				}

				// Only quit if Quit is the actual last element in the array.
				if (AutomationCommand == EAutomationCommand::Quit)
				{
					FPlatformMisc::RequestExit(true);
					// We have finished the testing, and results are available
					AutomationTestState = EAutomationTestState::Complete;
				}
				break;
			}
		}
		return !IsTestingComplete();
	}

	
	/** Console commands, see embeded usage statement **/
	virtual bool Exec(UWorld*, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		bool bHandled = false;
		// Track whether we have a flag we care about passing through.
		FString FlagToUse = "";

		// Hackiest hack to ever hack a hack to get this test running.
		if (FParse::Command(&Cmd, TEXT("RunPerfTests")))
		{
			Cmd = TEXT("Automation RunFilter Perf");
		}
		else if (FParse::Command(&Cmd, TEXT("RunProductTests")))
		{
			Cmd = TEXT("Automation RunFilter Product");
		}

		//figure out if we are handling this request
		if (FParse::Command(&Cmd, TEXT("Automation")))
		{
			//save off device to send results to
			OutputDevice = GLog;

			StringCommand.Empty();

			TArray<FString> CommandList;
			StringCommand = Cmd;
			StringCommand.ParseIntoArray(CommandList, TEXT(";"), true);

			//assume we handle this
			Init();
			bHandled = true;

			for (int CommandIndex = 0; CommandIndex < CommandList.Num(); ++CommandIndex)
			{
				const TCHAR* TempCmd = *CommandList[CommandIndex];
				if (FParse::Command(&TempCmd, TEXT("List")))
				{
					AutomationCommandQueue.Add(EAutomationCommand::ListAllTests);
				}
				else if (FParse::Command(&TempCmd, TEXT("RunTests")))
				{
					if ( FParse::Command(&TempCmd, TEXT("Now")) )
					{
						DelayTimer = 0.0f;
					}

					//only one of these should be used
					StringCommand = TempCmd;
					AutomationCommandQueue.Add(EAutomationCommand::RunCommandLineTests);
				}
				else if (FParse::Command(&TempCmd, TEXT("RunFilter")))
				{
					FlagToUse = TempCmd;
					//only one of these should be used
					StringCommand = TempCmd;
					if (FilterMaps.Contains(FlagToUse))
					{
						AutomationController->SetRequestedTestFlags(FilterMaps[FlagToUse]);
					}
					AutomationCommandQueue.Add(EAutomationCommand::RunFilter);
				}
				else if (FParse::Command(&TempCmd, TEXT("RunAll")))
				{
					AutomationCommandQueue.Add(EAutomationCommand::RunAll);
				}
				else if (FParse::Command(&TempCmd, TEXT("Quit")))
				{
					AutomationCommandQueue.Add(EAutomationCommand::Quit);
				}
				// let user know he mis-typed a param/command
				else if (FParse::Command(&TempCmd, TEXT("RunTest")))
				{
					Ar.Logf(TEXT("Unhandled token \"RunTest\". You probably meant \"RunTests\". Bailing out."));
					bHandled = false;
					break;
				}
			}
		}
		
		// Shutdown our service
		return bHandled;
	}

private:
	/** The automation controller running the tests */
	IAutomationControllerManagerPtr AutomationController;

	/** The current state of the automation process */
	EAutomationTestState::Type AutomationTestState;

	/** What work was requested */
	TArray<EAutomationCommand::Type> AutomationCommandQueue;

	/** What work was requested */
	EAutomationCommand::Type AutomationCommand;

	/** Delay used before finding workers on game instances. Just to ensure they have started up */
	float DelayTimer;

	/** Holds the session ID */
	FGuid SessionID;

	//device to send results to
	FOutputDevice* OutputDevice;

	//so we can release control of the app and just get ticked like all other systems
	FDelegateHandle TickHandler;

	//Extra commandline params
	FString StringCommand;

	//This is the numbers of tests that are found in the command line.
	int32 TestCount;

	//Dictionary that maps flag names to flag values.
	TMap<FString, int32> FilterMaps;
};

static FAutomationExecCmd AutomationExecCmd;

