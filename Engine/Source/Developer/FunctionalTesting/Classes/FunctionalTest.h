// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "FunctionalTest.generated.h"

class UBillboardComponent;

//Experimental effort at automated cpu captures from the functional testing.
class FFunctionalTestExternalProfiler : public FScopedExternalProfilerBase
{
public:
	void StartProfiler(const bool bWantPause){ StartScopedTimer(bWantPause); }
	void StopProfiler(){StopScopedTimer();}
}; 

// Used to measure a distribution
struct FStatisticalFloat
{
public:
	FStatisticalFloat()
		: MinValue(0.0)
		, MaxValue(0.0)
		, Accumulator(0.0)
		, NumSamples(0)
	{
	}

	void AddSample(double Value)
	{
		if (NumSamples == 0)
		{
			MinValue = MaxValue = Value;
		}
		else
		{
			MinValue = FMath::Min(MinValue, Value);
			MaxValue = FMath::Max(MaxValue, Value);
		}
		Accumulator += Value;
		++NumSamples;
	}

	double GetMinValue() const
	{
		return MinValue;
	}

	double GetMaxValue() const
	{
		return MaxValue;
	}

	double GetAvgValue() const
	{
		return Accumulator / (double)NumSamples;
	}

	int32 GetCount() const
	{
		return NumSamples;
	}

private:
	double MinValue;
	double MaxValue;
	double Accumulator;
	int32 NumSamples;
};

struct FStatsData
{
	FStatsData() :NumFrames(0), SumTimeSeconds(0.0f){}

	uint32 NumFrames;
	uint32 SumTimeSeconds;
	FStatisticalFloat FrameTimeTracker;
	FStatisticalFloat GameThreadTimeTracker;
	FStatisticalFloat RenderThreadTimeTracker;
	FStatisticalFloat GPUTimeTracker;
};

/** A set of simple perf stats recorded over a period of frames. */
struct FUNCTIONALTESTING_API FPerfStatsRecord
{
	FPerfStatsRecord(FString InName);

	FString Name;

	/** Stats data for the period we're interested in timing. */
	FStatsData Record;
	/** Stats data for the baseline. */
	FStatsData Baseline;
	
	float GPUBudget;
	float RenderThreadBudget;
	float GameThreadBudget;

	void SetBudgets(float InGPUBudget, float InRenderThreadBudget, float InGameThreadBudget);
	void Sample(UWorld* Owner, float DeltaSeconds, bool bBaseline);

	FString GetReportString()const;
	FString GetBaselineString()const;
	FString GetRecordString()const;
	FString GetOverBudgetString()const;

	void GetGPUTimes(double& OutMin, double& OutMax, double& OutAvg)const;
	void GetGameThreadTimes(double& OutMin, double& OutMax, double& OutAvg)const;
	void GetRenderThreadTimes(double& OutMin, double& OutMax, double& OutAvg)const;

	bool IsWithinGPUBudget()const;
	bool IsWithinGameThreadBudget()const;
	bool IsWithinRenderThreadBudget()const;
};


UENUM(BlueprintType)
enum class EComparisonMethod : uint8
{
	Equal_To,
	Not_Equal_To,
	Greater_Than_Or_Equal_To,
	Less_Than_Or_Equal_To,
	Greater_Than,
	Less_Than
};


/** 
 * Class for use with functional tests which provides various performance measuring features. 
 * Recording of basic, unintrusive performance stats.
 * Automatic captures using external CPU and GPU profilers.
 * Triggering and ending of writing full stats to a file.
*/
UCLASS(Blueprintable)
class FUNCTIONALTESTING_API UAutomationPerformaceHelper : public UObject
{
	GENERATED_BODY()

	TArray<FPerfStatsRecord> Records;
	bool bRecordingBasicStats;
	bool bRecordingBaselineBasicStats;
	bool bRecordingCPUCapture;
	bool bRecordingStatsFile;

	/** If true we check the GPU times vs GPU budget each tick and trigger a GPU trace if we fall below budget.*/
	bool bGPUTraceIfBelowBudget;

public:

	UAutomationPerformaceHelper();

	//Begin basic stat recording

	UFUNCTION(BlueprintCallable, Category = Perf)
	void Tick(float DeltaSeconds);

	/** Adds a sample to the stats counters for the current performance stats record. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void Sample(float DeltaSeconds);
	/** Begins recording a new named performance stats record. We start by recording the baseline */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void BeginRecordingBaseline(FString RecordName);
	/** Stops recording the baseline and moves to the main record. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void EndRecordingBaseline();
	/** Begins recording a new named performance stats record. We start by recording the baseline. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void BeginRecording(FString RecordName,	float InGPUBudget, float InRenderThreadBudget, float InGameThreadBudget);
	/** Stops recording performance stats. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void EndRecording();
	/** Writes the current set of performance stats records to a csv file in the profiling directory. An additional directory and an extension override can also be used. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void WriteLogFile(const FString& CaptureDir, const FString& CaptureExtension);
	/** Returns true if this stats tracker is currently recording performance stats. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	bool IsRecording()const;

	/** Does any init work across all tests.. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void OnBeginTests();
	/** Does any final work needed as all tests are complete. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void OnAllTestsComplete();

	const FPerfStatsRecord* GetCurrentRecord()const;
	FPerfStatsRecord* GetCurrentRecord();

	UFUNCTION(BlueprintCallable, Category = Perf)
	bool IsCurrentRecordWithinGPUBudget()const;
	UFUNCTION(BlueprintCallable, Category = Perf)
	bool IsCurrentRecordWithinGameThreadBudget()const;
	UFUNCTION(BlueprintCallable, Category = Perf)
	bool IsCurrentRecordWithinRenderThreadBudget()const;
	//End basic stats recording.

	// Automatic traces capturing 

	/** Communicates with external profiler to being a CPU capture. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void StartCPUProfiling();
	/** Communicates with external profiler to end a CPU capture. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void StopCPUProfiling();
	/** Will trigger a GPU trace next time the current test falls below GPU budget. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void TriggerGPUTraceIfRecordFallsBelowBudget();
	/** Begins recording stats to a file. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void BeginStatsFile(const FString& RecordName);
	/** Ends recording stats to a file. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void EndStatsFile();

	FFunctionalTestExternalProfiler ExternalProfiler;

	/** The path and base name for all output files. */
	FString OutputFileBase;
	
	FString StartOfTestingTime;
};

UENUM()
enum class EFunctionalTestResult : uint8
{
	/**
	 * When finishing a test if you use Default, you're not explicitly stating if the test passed or failed.
	 * Instead you're a
	 */
	Default,
	Invalid,
	Error,
	Running,
	Failed,
	Succeeded
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFunctionalTestEventSignature);
DECLARE_DELEGATE_OneParam(FFunctionalTestDoneSignature, class AFunctionalTest*);

UCLASS(hidecategories=( Actor, Input, Rendering ), Blueprintable)
class FUNCTIONALTESTING_API AFunctionalTest : public AActor
{
	GENERATED_BODY()

public:
	AFunctionalTest(const FObjectInitializer& ObjectInitializer);

private_subobject:
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UBillboardComponent* SpriteComponent;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Functional Testing")
	uint32 bIsEnabled:1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Functional Testing", meta=( MultiLine="true" ))
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Functional Testing")
	AActor* ObservationPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Functional Testing", AdvancedDisplay)
	FRandomStream RandomNumbersStream;

public:

	UPROPERTY(BlueprintReadWrite, Category="Functional Testing")
	EFunctionalTestResult Result;

	/** The Test's time limit for preparation, this is the time it has to return true when checking IsReady(). '0' means no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timeout")
	float PreparationTimeLimit;

	/** Test's time limit. '0' means no limit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timeout")
	float TimeLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timeout", meta=( MultiLine="true" ))
	FText TimesUpMessage;

	/** If test is limited by time this is the result that will be returned when time runs out */
	UPROPERTY(EditAnywhere, Category="Timeout")
	EFunctionalTestResult TimesUpResult;

public:

	/** Called when the test is started */
	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnTestStart;

	/** Called when the test is finished. Use it to clean up */
	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnTestFinished;

	UPROPERTY(Transient)
	TArray<AActor*> AutoDestroyActors;
	
	FString FailureMessage;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	class UFuncTestRenderingComponent* RenderComp;

	UPROPERTY()
	class UTextRenderComponent* TestName;
#endif // WITH_EDITORONLY_DATA

	/** List of causes we need a re-run. */
	TArray<FName> RerunCauses;

	/** Cause of the current rerun if we're in a named rerun. */
	FName CurrentRerunCause;

public:
	/**
	 * Assert that a boolean value is true.
	 * @param Message	The message to display if the assert fails ("Assertion Failed: 'Message' for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertTrue(bool Condition, FString Message, const UObject* ContextObject = nullptr);

	/**
	 * Assert that a boolean value is false.
	 * @param Message	The message to display if the assert fails ("Assertion Failed: 'Message' for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertFalse(bool Condition, FString Message, const UObject* ContextObject = nullptr);

	/**
	 * Assert that a UObject is valid
	 * @param Message	The message to display if the object is invalid ("Invalid object: 'Message' for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertIsValid(UObject* Object, FString Message, const UObject* ContextObject = nullptr);

	/**
	 * Assert on a relationship between two integers.
	 * @param What	A name to use in the message if the assert fails (What: expected {Actual} to be <ShouldBe> {Expected} for context '')
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Value (int)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertValue_Int(int32 Actual, EComparisonMethod ShouldBe, int32 Expected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert on a relationship between two floats.
	 * @param What	A name to use in the message if the assert fails (What: expected {Actual} to be <ShouldBe> {Expected} for context '')
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Value (float)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertValue_Float(float Actual, EComparisonMethod ShouldBe, float Expected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert on a relationship between two DateTimes.
	 * @param What	A name to use in the message if the assert fails (What: expected {Actual} to be <ShouldBe> {Expected} for context '')
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Value (DateTime)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertValue_DateTime(FDateTime Actual, EComparisonMethod ShouldBe, FDateTime Expected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert that two transforms are (components memberwise - translation, rotation, scale) equal within a small tolerance.
	 * @param What	A name to use in the message if the assert fails ("Expected 'What' to be {Expected} but it was {Actual} for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Equal (Transform)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertEqual_Transform(const FTransform Actual, const FTransform Expected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert that two floats are equal within tolerance between two floats.
	 * @param What	A name to use in the message if the assert fails (What: expected {Actual} to be Equal To {Expected} within Tolerance for context '')
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Equal (Float)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertEqual_Float(const float Actual, const float Expected, const FString& What, float Tolerance = 1.e-4, const UObject* ContextObject = nullptr);

	/**
	 * Assert that two transforms are (components memberwise - translation, rotation, scale) not equal within a small tolerance.
	 * @param What	A name to use in the message if the assert fails ("Expected 'What' not to be {Expected} but it was {Actual} for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Not Equal (Transform)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertNotEqual_Transform(const FTransform Actual, const FTransform NotExpected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert that the component angles of two rotators are all equal within a small tolerance.
	 * @param What	A name to use in the message if the assert fails ("Expected 'What' to be {Expected} but it was {Actual} for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Equal (Rotator)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertEqual_Rotator(const FRotator Actual, const FRotator Expected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert that the component angles of two rotators are all not equal within a small tolerance.
	 * @param What	A name to use in the message if the assert fails ("Expected 'What' not to be {Expected} but it was {Actual} for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Not Equal (Rotator)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertNotEqual_Rotator(const FRotator Actual, const FRotator NotExpected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert that two vectors are (memberwise) equal within a small tolerance.
	 * @param What	A name to use in the message if the assert fails ("Expected 'What' to be {Expected} but it was {Actual} for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Equal (Vector)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertEqual_Vector(const FVector Actual, const FVector Expected, const FString& What, const float Tolerance = 1.e-4f, const UObject* ContextObject = nullptr);

	/**
	 * Assert that two vectors are (memberwise) not equal within a small tolerance.
	 * @param What	A name to use in the message if the assert fails ("Expected 'What' not to be {Expected} but it was {Actual} for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Not Equal (Vector)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertNotEqual_Vector(const FVector Actual, const FVector NotExpected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert that two Strings are equal.
	 * @param What	A name to use in the message if the assert fails ("Expected 'What' to be {Expected} but it was {Actual} for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Equal (String)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertEqual_String(const FString Actual, const FString Expected, const FString& What, const UObject* ContextObject = nullptr);

	/**
	 * Assert that two Strings are not equal.
	 * @param What	A name to use in the message if the assert fails ("Expected 'What' not to be {Expected} but it was {Actual} for context ''")
	 */
	UFUNCTION(BlueprintCallable, Category = "Asserts", DisplayName = "Assert Not Equal (String)", meta = ( HidePin = "ContextObject", DefaultToSelf = "ContextObject"))
	void AssertNotEqual_String(const FString Actual, const FString NotExpected, const FString& What, const UObject* ContextObject = nullptr);

protected:
	void LogAssertFail(const FString& Message);
	void LogAssertPass(const FString& Message);

public:
	virtual bool RunTest(const TArray<FString>& Params = TArray<FString>());

public:
	FString	GetCurrentStepName() { return IsInStep() ? CurrentStepName : FString(); }
	void 	StartStep(const FString& StepName) { CurrentStepName = StepName; bInStep = true; }
	void 	FinishStep() { CurrentStepName.Empty(); bInStep = false; }
	bool	IsInStep() { return bInStep; }


	UFUNCTION(BlueprintCallable, Category="Functional Testing")
	virtual void FinishTest(EFunctionalTestResult TestResult, const FString& Message);

	UFUNCTION(BlueprintCallable, Category="Functional Testing")
	virtual void LogMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, Category="Functional Testing")
	virtual void SetTimeLimit(float NewTimeLimit, EFunctionalTestResult ResultWhenTimeRunsOut);

public:

	/** Used by debug drawing to gather actors this test is using and point at them on the level to better understand test's setup */
	UFUNCTION(BlueprintImplementableEvent, Category="Functional Testing")
	TArray<AActor*> DebugGatherRelevantActors() const;

	virtual void GatherRelevantActors(TArray<AActor*>& OutActors) const;

	/** retrieves information whether test wants to have another run just after finishing */
	UFUNCTION(BlueprintImplementableEvent, Category="Functional Testing")
	bool OnWantsReRunCheck() const;

	virtual bool WantsToRunAgain() const { return false; }

	/** Causes the test to be rerun for a specific named reason. */
	UFUNCTION(BlueprintCallable, Category = "Functional Testing")
	void AddRerun(FName Reason);

	/** Returns the current re-run reason if we're in a named re-run. */
	UFUNCTION(BlueprintCallable, Category = "Functional Testing")
	FName GetCurrentRerunReason() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Functional Testing")
	FString OnAdditionalTestFinishedMessageRequest(EFunctionalTestResult TestResult) const;
	
	virtual FString GetAdditionalTestFinishedMessage(EFunctionalTestResult TestResult) const { return FString(); }

public:
	
	/** ACtors registered this way will be automatically destroyed (by limiting their lifespan)
	 *	on test finish */
	UFUNCTION(BlueprintCallable, Category="Development", meta=(Keywords = "Delete"))
	virtual void RegisterAutoDestroyActor(AActor* ActorToAutoDestroy);

	/** Called to clean up when tests is removed from the list of active tests after finishing execution. 
	 *	Note that FinishTest gets called after every "cycle" of a test (where further cycles are enabled by  
	 *	WantsToRunAgain calls). CleanUp gets called when all cycles are done. */
	virtual void CleanUp();

	virtual FString GetReproString() const { return GetFName().ToString(); }

#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	static void OnSelectObject(UObject* NewSelection);
#endif // WITH_EDITOR

	// AActor interface begin
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// AActor interface end

	UFUNCTION(BlueprintCallable, Category = "Functional Testing")
	bool IsRunning() const;

	UFUNCTION(BlueprintCallable, Category = "Functional Testing")
	bool IsEnabled() const;

protected:
	/**
	 * Prepare Test is fired once the test starts up, before the test IsReady() and thus before Start Test is called.
	 * So if there's some initial conditions or setup that you might need for your IsReady() check, you might want
	 * to do that here.
	 */
	virtual void PrepareTest();

	/**
	 * Prepare Test is fired once the test starts up, before the test IsReady() and thus before Start Test is called.
	 * So if there's some initial conditions or setup that you might need for your IsReady() check, you might want
	 * to do that here.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=( DisplayName="Prepare Test" ))
	void ReceivePrepareTest();

	/**
	 * Called once the IsReady() check for the test returns true.  After that happens the test has Officially started,
	 * and it will begin receiving Ticks in the blueprint.
	 */
	virtual void StartTest();
	
	/**
	 * Called once the IsReady() check for the test returns true.  After that happens the test has Officially started,
	 * and it will begin receiving Ticks in the blueprint.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=( DisplayName="Start Test" ))
	void ReceiveStartTest();

	/**
	 * IsReady() is called once per frame after a test is run, until it returns true.  You should use this function to
	 * delay Start being called on the test until preconditions are met.
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Functional Testing")
	bool IsReady();

	virtual bool IsReady_Implementation();

	/**
	 * Goto an observation location.
	 */
	void GoToObservationPoint();

public:
	FFunctionalTestDoneSignature TestFinishedObserver;

protected:
	bool bIsRunning;

	bool bInStep;
	FString CurrentStepName;
	
	float TotalTime;

private:
	bool bIsReady;

public:
	/** Returns SpriteComponent subobject **/
	UBillboardComponent* GetSpriteComponent();
};
