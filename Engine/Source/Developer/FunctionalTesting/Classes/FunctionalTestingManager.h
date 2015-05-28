// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AutomationTest.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FunctionalTestingManager.generated.h"

class UWorld;

namespace FFunctionalTesting
{
	extern const TCHAR* ReproStringTestSeparator;
	extern const TCHAR* ReproStringParamsSeparator;
}

UCLASS(BlueprintType, MinimalAPI)
class UFunctionalTestingManager : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(Transient)
	TArray<class AFunctionalTest*> TestsLeft;

	UPROPERTY(Transient)
	TArray<class AFunctionalTest*> AllTests;

	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnSetupTests;
	
	UFUNCTION(BlueprintCallable, Category="FunctionalTesting", meta=(WorldContext="WorldContext", CallableWithoutWorldContext ) )
	/** Triggers in sequence all functional tests found on the level.
	 *	@return true if any tests have been triggered */
	 static bool RunAllFunctionalTests(UObject* WorldContext, bool bNewLog = true, bool bRunLooped = false, bool bWaitForNavigationBuildFinish = true, FString FailedTestsReproString = TEXT(""));
		
	bool IsRunning() const { return bIsRunning; }
	bool IsFinished() const { return bFinished; }
	bool IsLooped() const { return bLooped; }
	void SetLooped(const bool bNewLooped) { bLooped = bNewLooped; }

	void TickMe(float DeltaTime);

	//----------------------------------------------------------------------//
	// Automation logging
	//----------------------------------------------------------------------//
	static void SetAutomationExecutionInfo(FAutomationTestExecutionInfo* InExecutionInfo) { ExecutionInfo = InExecutionInfo; }
	static void AddError(const FText& InError);
	static void AddWarning(const FText& InWarning);
	static void AddLogItem(const FText& InLogItem);

private:
	void LogMessage(const FString& MessageString, TSharedPtr<IMessageLogListing> LogListing = NULL);
	
protected:
	static UFunctionalTestingManager* GetManager(UObject* WorldContext);

	void TriggerFirstValidTest();
	void SetUpTests();

	void OnTestDone(class AFunctionalTest* FTest);
	void OnEndPIE(const bool bIsSimulating);

	bool RunFirstValidTest();

	void NotifyTestDone(class AFunctionalTest* FTest);

	void SetReproString(FString ReproString);
	void AllTestsDone();

	void OnWorldCleanedUp(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	virtual UWorld* GetWorld() const override;
	
	bool bIsRunning;
	bool bFinished;
	bool bLooped;
	bool bWaitForNavigationBuildFinish;
	bool bInitialDelayApplied;
	uint32 CurrentIteration;

	FFunctionalTestDoneSignature TestFinishedObserver;
	
	FString GatheredFailedTestsReproString;
	FString StartingReproString;
	TArray<FString> TestReproStrings;

	static FAutomationTestExecutionInfo* ExecutionInfo;

private:
	FTimerHandle TriggerFirstValidTestTimerHandle;
};
