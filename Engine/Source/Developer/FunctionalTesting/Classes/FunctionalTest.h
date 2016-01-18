// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	/** Communicates with an external GPU profiler to trigger a GPU trace. */
	UFUNCTION(BlueprintCallable, Category = Perf)
	void TriggerGPUTrace();
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
namespace EFunctionalTestResult
{
	enum Type
	{
		Invalid,
		Error,
		Running,		
		Failed,
		Succeeded,
	};
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFunctionalTestEventSignature);
DECLARE_DELEGATE_OneParam(FFunctionalTestDoneSignature, class AFunctionalTest*);

UCLASS(Blueprintable, MinimalAPI)
class AFunctionalTest : public AActor
{
	GENERATED_UCLASS_BODY()

	static const uint32 DefaultTimeLimit = 60;	// seconds

private_subobject:
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
public:

	UPROPERTY(BlueprintReadWrite, Category=FunctionalTesting)
	TEnumAsByte<EFunctionalTestResult::Type> Result;

	/** If test is limited by time this is the result that will be returned when time runs out */
	UPROPERTY(EditAnywhere, Category=FunctionalTesting)
	TEnumAsByte<EFunctionalTestResult::Type> TimesUpResult;

	/** Test's time limit. '0' means no limit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=FunctionalTesting)
	float TimeLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FunctionalTesting)
	FText TimesUpMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FunctionalTesting)
	AActor* ObservationPoint;

	/** Called when the test is started */
	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnTestStart;

	/** Called when the test is finished. Use it to clean up */
	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnTestFinished;

	UPROPERTY(Transient)
	TArray<AActor*> AutoDestroyActors;
	
	FString FailureMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FunctionalTesting)
	FRandomStream RandomNumbersStream;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FunctionalTesting)
	FString Description;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	class UFuncTestRenderingComponent* RenderComp;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=FunctionalTesting)
	uint32 bIsEnabled:1;

	/** List of causes we need a re-run. */
	TArray<FName> RerunCauses;
	/** Cause of the current rerun if we're in a named rerun. */
	FName CurrentRerunCause;

public:

	virtual bool StartTest(const TArray<FString>& Params = TArray<FString>());

	UFUNCTION(BlueprintCallable, Category="Development")
	virtual void FinishTest(TEnumAsByte<EFunctionalTestResult::Type> TestResult, const FString& Message);

	UFUNCTION(BlueprintCallable, Category="Development")
	virtual void LogMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, Category="Development")
	virtual void SetTimeLimit(float NewTimeLimit, TEnumAsByte<EFunctionalTestResult::Type> ResultWhenTimeRunsOut);

	/** Used by debug drawing to gather actors this test is using and point at them on the level to better understand test's setup */
	UFUNCTION(BlueprintImplementableEvent, Category = "FunctionalTesting")
	TArray<AActor*> DebugGatherRelevantActors() const;

	virtual void GatherRelevantActors(TArray<AActor*>& OutActors) const;

	/** retrieves information whether test wants to have another run just after finishing */
	UFUNCTION(BlueprintImplementableEvent, Category="FunctionalTesting")
	bool OnWantsReRunCheck() const;

	virtual bool WantsToRunAgain() const { return false; }

	/** Causes the test to be rerun for a specific named reason. */
	UFUNCTION(BlueprintCallable, Category = "FunctionalTesting")
	void AddRerun(FName Reason);

	/** Returns the current re-run reason if we're in a named re-run. */
	UFUNCTION(BlueprintCallable, Category = "FunctionalTesting")
	FName GetCurrentRerunReason()const;

	UFUNCTION(BlueprintImplementableEvent, Category = "FunctionalTesting")
	FString OnAdditionalTestFinishedMessageRequest(EFunctionalTestResult::Type TestResult) const;
	
	virtual FString GetAdditionalTestFinishedMessage(EFunctionalTestResult::Type TestResult) const { return FString(); }
	
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
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// AActor interface end

	bool IsSuccessful() const { return Result == EFunctionalTestResult::Succeeded; }
	bool IsRunning() const { return !!bIsRunning; }

protected:
	void GoToObservationPoint();

public:
	FFunctionalTestDoneSignature TestFinishedObserver;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=FunctionalTesting)
	bool bIsRunning;
	
	float TotalTime;

public:
	/** Returns SpriteComponent subobject **/
	UBillboardComponent* GetSpriteComponent();
};
