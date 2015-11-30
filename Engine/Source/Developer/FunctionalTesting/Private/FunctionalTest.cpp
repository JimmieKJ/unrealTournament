// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "ObjectEditorUtils.h"
#include "VisualLogger/VisualLogger.h"
#if WITH_EDITORONLY_DATA
#include "FuncTestRenderingComponent.h"
#endif // WITH_EDITORONLY_DATA
#if WITH_EDITOR
#include "Engine/Selection.h"
#endif // WITH_EDITOR

AFunctionalTest::AFunctionalTest( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, TimesUpResult(EFunctionalTestResult::Failed)
	, TimeLimit(DefaultTimeLimit)
	, TimesUpMessage( NSLOCTEXT("FunctionalTest", "DefaultTimesUpMessage", "Time's up!") )
	, bIsEnabled(true)
	, bIsRunning(false)
	, TotalTime(0.f)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
		
	SpriteComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->bHiddenInGame = false;
#if WITH_EDITORONLY_DATA

		if (!IsRunningCommandlet())
		{
			struct FConstructorStatics
			{
				ConstructorHelpers::FObjectFinderOptional<UTexture2D> Texture;
				FName ID_FTests;
				FText NAME_FTests;

				FConstructorStatics()
					: Texture(TEXT("/Engine/EditorResources/S_FTest"))
					, ID_FTests(TEXT("FTests"))
					, NAME_FTests(NSLOCTEXT( "SpriteCategory", "FTests", "FTests" ))
				{
				}
			};
			static FConstructorStatics ConstructorStatics;

			SpriteComponent->Sprite = ConstructorStatics.Texture.Get();
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_FTests;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_FTests;
		}

#endif
		RootComponent = SpriteComponent;
	}

#if WITH_EDITORONLY_DATA
	RenderComp = CreateDefaultSubobject<UFuncTestRenderingComponent>(TEXT("RenderComp"));
	RenderComp->PostPhysicsComponentTick.bCanEverTick = false;
	RenderComp->AttachParent = RootComponent;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	static bool bSelectionHandlerSetUp = false;
	if (HasAnyFlags(RF_ClassDefaultObject) && !HasAnyFlags(RF_TagGarbageTemp) && bSelectionHandlerSetUp == false)
	{
		USelection::SelectObjectEvent.AddStatic(&AFunctionalTest::OnSelectObject);
		bSelectionHandlerSetUp = true;
	}
#endif // WITH_EDITOR
}

void AFunctionalTest::Tick(float DeltaSeconds)
{
	// already requested not to tick. 
	if (bIsRunning == false)
	{
		return;
	}

	//Do not collect garbage during the test. We force GC at the end.
	GetWorld()->DelayGarbageCollection();

	TotalTime += DeltaSeconds;
	if (TimeLimit > 0.f && TotalTime > TimeLimit)
	{
		FinishTest(TimesUpResult, TimesUpMessage.ToString());
	}
	else
	{
		Super::Tick(DeltaSeconds);
	}
}

bool AFunctionalTest::StartTest(const TArray<FString>& Params)
{
	FailureMessage = TEXT("");
	
	//Do not collect garbage during the test. We force GC at the end.
	GetWorld()->DelayGarbageCollection();

	TotalTime = 0.f;
	if (TimeLimit > 0)
	{
		SetActorTickEnabled(true);
	}

	bIsRunning = true;

	GoToObservationPoint();
	
	OnTestStart.Broadcast();

	return true;
}

void AFunctionalTest::FinishTest(TEnumAsByte<EFunctionalTestResult::Type> TestResult, const FString& Message)
{
	const static UEnum* FTestResultTypeEnum = FindObject<UEnum>( NULL, TEXT("FunctionalTesting.EFunctionalTestResult") );
	
	if (bIsRunning == false)
	{
		// ignore
		return;
	}

	//Force GC at the end of every test.
	GetWorld()->ForceGarbageCollection();

	Result = TestResult;

	bIsRunning = false;
	SetActorTickEnabled(false);

	OnTestFinished.Broadcast();

	AActor** ActorToDestroy = AutoDestroyActors.GetData();

	for (int32 ActorIndex = 0; ActorIndex < AutoDestroyActors.Num(); ++ActorIndex, ++ActorToDestroy)
	{
		if (*ActorToDestroy != NULL)
		{
			// will be removed next frame
			(*ActorToDestroy)->SetLifeSpan( 0.01f );
		}
	}

	const FText ResultText = FTestResultTypeEnum->GetEnumText( TestResult.GetValue() );
	const FString OutMessage = FString::Printf(TEXT("%s %s: \"%s\'")
		, *GetName()
		, *ResultText.ToString()
		, Message.IsEmpty() == false ? *Message : TEXT("Test finished") );
	const FString AdditionalDetails = FString::Printf(TEXT("%s %s, time %.2fs"), *GetAdditionalTestFinishedMessage(TestResult), *OnAdditionalTestFinishedMessageRequest(TestResult), TotalTime);

	AutoDestroyActors.Reset();
		
	switch (TestResult.GetValue())
	{
		case EFunctionalTestResult::Invalid:
		case EFunctionalTestResult::Error:
		case EFunctionalTestResult::Failed:
			UE_VLOG(this, LogFunctionalTest, Error, TEXT("%s"), *OutMessage);
			UE_LOG(LogFunctionalTest, Error, TEXT("%s"), *OutMessage);
			break;

		case EFunctionalTestResult::Running:
			UE_VLOG(this, LogFunctionalTest, Warning, TEXT("%s"), *OutMessage);
			UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *OutMessage);
			break;
		
		default:
			UE_VLOG(this, LogFunctionalTest, Log, TEXT("%s"), *OutMessage);
			UE_LOG(LogFunctionalTest, Log, TEXT("%s"), *OutMessage);
			break;
	}
	
	if (AdditionalDetails.IsEmpty() == false)
	{
		UE_LOG(LogFunctionalTest, Log, TEXT("%s"), *AdditionalDetails);
	}

	TestFinishedObserver.ExecuteIfBound(this);
}

void AFunctionalTest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TestFinishedObserver.Unbind();

	Super::EndPlay(EndPlayReason);
}

void AFunctionalTest::CleanUp()
{
	FailureMessage = TEXT("");
}

//@todo add "warning" level here
void AFunctionalTest::LogMessage(const FString& Message)
{
	UE_LOG(LogFunctionalTest, Log, TEXT("%s"), *Message);
	UE_VLOG(this, LogFunctionalTest, Log
		, TEXT("%s> %s")
		, *GetName(), *Message);
}

void AFunctionalTest::SetTimeLimit(float InTimeLimit, TEnumAsByte<EFunctionalTestResult::Type> InResult)
{
	if (InTimeLimit < 0.f)
	{
		UE_VLOG(this, LogFunctionalTest, Warning
			, TEXT("%s> Trying to set TimeLimit to less than 0. Falling back to 0 (infinite).")
			, *GetName());

		InTimeLimit = 0.f;
	}
	TimeLimit = InTimeLimit;

	if (InResult == EFunctionalTestResult::Invalid)
	{
		UE_VLOG(this, LogFunctionalTest, Warning
			, TEXT("%s> Trying to set test Result to \'Invalid\'. Falling back to \'Failed\'")
			, *GetName());

		InResult = EFunctionalTestResult::Failed;
	}
	TimesUpResult = InResult;
}

void AFunctionalTest::GatherRelevantActors(TArray<AActor*>& OutActors) const
{
	if (ObservationPoint)
	{
		OutActors.AddUnique(ObservationPoint);
	}

	for (auto Actor : AutoDestroyActors)
	{
		if (Actor)
		{
			OutActors.AddUnique(Actor);
		}
	}

	OutActors.Append(DebugGatherRelevantActors());
}

void AFunctionalTest::AddRerun(FName Reason)
{
	RerunCauses.Add(Reason);
}

FName AFunctionalTest::GetCurrentRerunReason()const
{
	return CurrentRerunCause;
}

void AFunctionalTest::RegisterAutoDestroyActor(AActor* ActorToAutoDestroy)
{
	AutoDestroyActors.AddUnique(ActorToAutoDestroy);
}

#if WITH_EDITOR

void AFunctionalTest::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_FunctionalTesting = FName(TEXT("FunctionalTesting"));
	static const FName NAME_TimeLimit = FName(TEXT("TimeLimit"));
	static const FName NAME_TimesUpResult = FName(TEXT("TimesUpResult"));

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != NULL)
	{
		if (FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property) == NAME_FunctionalTesting)
		{
			// first validate new data since there are some dependencies
			if (PropertyChangedEvent.Property->GetFName() == NAME_TimeLimit)
			{
				if (TimeLimit < 0.f)
				{
					TimeLimit = 0.f;
				}
			}
			else if (PropertyChangedEvent.Property->GetFName() == NAME_TimesUpResult)
			{
				if (TimesUpResult == EFunctionalTestResult::Invalid)
				{
					TimesUpResult = EFunctionalTestResult::Failed;
				}
			}
		}
	}
}

void AFunctionalTest::OnSelectObject(UObject* NewSelection)
{
	AFunctionalTest* AsFTest = Cast<AFunctionalTest>(NewSelection);
	if (AsFTest)
	{
		AsFTest->MarkComponentsRenderStateDirty();
	}
}

#endif // WITH_EDITOR

void AFunctionalTest::GoToObservationPoint()
{
	if (ObservationPoint == NULL)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World && World->GetGameInstance())
	{
		APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();
		if (PC && PC->GetPawn())
		{
			PC->GetPawn()->TeleportTo(ObservationPoint->GetActorLocation(), ObservationPoint->GetActorRotation(), /*bIsATest=*/false, /*bNoCheck=*/true);
			PC->SetControlRotation(ObservationPoint->GetActorRotation());
		}
	}
}

/** Returns SpriteComponent subobject **/
UBillboardComponent* AFunctionalTest::GetSpriteComponent() { return SpriteComponent; }


//////////////////////////////////////////////////////////////////////////

FPerfStatsRecord::FPerfStatsRecord(FString InName)
: Name(InName)
, GPUBudget(0.0f)
, RenderThreadBudget(0.0f)
, GameThreadBudget(0.0f)
{
}

void FPerfStatsRecord::SetBudgets(float InGPUBudget, float InRenderThreadBudget, float InGameThreadBudget)
{
	GPUBudget = InGPUBudget;
	RenderThreadBudget = InRenderThreadBudget;
	GameThreadBudget = InGameThreadBudget;
}

FString FPerfStatsRecord::GetReportString() const
{
	return FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
		*Name,
		Record.FrameTimeTracker.GetMinValue() - Baseline.FrameTimeTracker.GetMinValue(),
		Record.FrameTimeTracker.GetAvgValue() - Baseline.FrameTimeTracker.GetAvgValue(),
		Record.FrameTimeTracker.GetMaxValue() - Baseline.FrameTimeTracker.GetMaxValue(),
		Record.RenderThreadTimeTracker.GetMinValue() - Baseline.RenderThreadTimeTracker.GetMinValue(),
		Record.RenderThreadTimeTracker.GetAvgValue() - Baseline.RenderThreadTimeTracker.GetAvgValue(),
		Record.RenderThreadTimeTracker.GetMaxValue() - Baseline.RenderThreadTimeTracker.GetMaxValue(),
		Record.GameThreadTimeTracker.GetMinValue() - Baseline.GameThreadTimeTracker.GetMinValue(),
		Record.GameThreadTimeTracker.GetAvgValue() - Baseline.GameThreadTimeTracker.GetAvgValue(),
		Record.GameThreadTimeTracker.GetMaxValue() - Baseline.GameThreadTimeTracker.GetMaxValue(),
		Record.GPUTimeTracker.GetMinValue() - Baseline.GPUTimeTracker.GetMinValue(),
		Record.GPUTimeTracker.GetAvgValue() - Baseline.GPUTimeTracker.GetAvgValue(),
		Record.GPUTimeTracker.GetMaxValue() - Baseline.GPUTimeTracker.GetMaxValue());
}

FString FPerfStatsRecord::GetBaselineString() const
{
	return FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
		*Name,
		Baseline.FrameTimeTracker.GetMinValue(),
		Baseline.FrameTimeTracker.GetAvgValue(),
		Baseline.FrameTimeTracker.GetMaxValue(),
		Baseline.RenderThreadTimeTracker.GetMinValue(),
		Baseline.RenderThreadTimeTracker.GetAvgValue(),
		Baseline.RenderThreadTimeTracker.GetMaxValue(),
		Baseline.GameThreadTimeTracker.GetMinValue(),
		Baseline.GameThreadTimeTracker.GetAvgValue(),
		Baseline.GameThreadTimeTracker.GetMaxValue(),
		Baseline.GPUTimeTracker.GetMinValue(),
		Baseline.GPUTimeTracker.GetAvgValue(),
		Baseline.GPUTimeTracker.GetMaxValue());
}

FString FPerfStatsRecord::GetRecordString() const
{
	return FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
		*Name,
		Record.FrameTimeTracker.GetMinValue(),
		Record.FrameTimeTracker.GetAvgValue(),
		Record.FrameTimeTracker.GetMaxValue(),
		Record.RenderThreadTimeTracker.GetMinValue(),
		Record.RenderThreadTimeTracker.GetAvgValue(),
		Record.RenderThreadTimeTracker.GetMaxValue(),
		Record.GameThreadTimeTracker.GetMinValue(),
		Record.GameThreadTimeTracker.GetAvgValue(),
		Record.GameThreadTimeTracker.GetMaxValue(),
		Record.GPUTimeTracker.GetMinValue(),
		Record.GPUTimeTracker.GetAvgValue(),
		Record.GPUTimeTracker.GetMaxValue());
}

FString FPerfStatsRecord::GetOverBudgetString() const
{
	double Min, Max, Avg;
	GetRenderThreadTimes(Min, Max, Avg);
	float RTBudgetFrac = Max / RenderThreadBudget;
	GetGameThreadTimes(Min, Max, Avg);
	float GTBudgetFrac = Max / GameThreadBudget;
	GetGPUTimes(Min, Max, Avg);
	float GPUBudgetFrac = Max / GPUBudget;

	return FString::Printf(TEXT("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
		*Name,
		Record.RenderThreadTimeTracker.GetMaxValue(),
		RenderThreadBudget,
		RTBudgetFrac,
		Record.GameThreadTimeTracker.GetMaxValue(),
		GameThreadBudget,
		GTBudgetFrac,
		Record.GPUTimeTracker.GetMaxValue(),
		GPUBudget,
		GPUBudgetFrac
		);
}

bool FPerfStatsRecord::IsWithinGPUBudget()const
{
	double Min, Max, Avg;
	GetGPUTimes(Min, Max, Avg);
	return Max <= GPUBudget;
}

bool FPerfStatsRecord::IsWithinGameThreadBudget()const
{
	double Min, Max, Avg;
	GetGameThreadTimes(Min, Max, Avg);
	return Max <= GameThreadBudget;
}

bool FPerfStatsRecord::IsWithinRenderThreadBudget()const
{
	double Min, Max, Avg;
	GetRenderThreadTimes(Min, Max, Avg);
	return Max <= RenderThreadBudget;
}

void FPerfStatsRecord::GetGPUTimes(double& OutMin, double& OutMax, double& OutAvg)const
{
	OutMin = Record.GPUTimeTracker.GetMinValue() - Baseline.GPUTimeTracker.GetMinValue();
	OutMax = Record.GPUTimeTracker.GetMaxValue() - Baseline.GPUTimeTracker.GetMaxValue();
	OutAvg = Record.GPUTimeTracker.GetAvgValue() - Baseline.GPUTimeTracker.GetAvgValue();
}

void FPerfStatsRecord::GetGameThreadTimes(double& OutMin, double& OutMax, double& OutAvg)const
{
	OutMin = Record.GameThreadTimeTracker.GetMinValue() - Baseline.GameThreadTimeTracker.GetMinValue();
	OutMax = Record.GameThreadTimeTracker.GetMaxValue() - Baseline.GameThreadTimeTracker.GetMaxValue();
	OutAvg = Record.GameThreadTimeTracker.GetAvgValue() - Baseline.GameThreadTimeTracker.GetAvgValue();
}

void FPerfStatsRecord::GetRenderThreadTimes(double& OutMin, double& OutMax, double& OutAvg)const
{
	OutMin = Record.RenderThreadTimeTracker.GetMinValue() - Baseline.RenderThreadTimeTracker.GetMinValue();
	OutMax = Record.RenderThreadTimeTracker.GetMaxValue() - Baseline.RenderThreadTimeTracker.GetMaxValue();
	OutAvg = Record.RenderThreadTimeTracker.GetAvgValue() - Baseline.RenderThreadTimeTracker.GetAvgValue();
}

void FPerfStatsRecord::Sample(UWorld* World, float DeltaSeconds, bool bBaseline)
{
	check(World);

	const FStatUnitData* StatUnitData = World->GetGameViewport()->GetStatUnitData();
	check(StatUnitData);

	if (bBaseline)
	{
		Baseline.FrameTimeTracker.AddSample(StatUnitData->RawFrameTime);
		Baseline.GameThreadTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GGameThreadTime));
		Baseline.RenderThreadTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GRenderThreadTime));
		Baseline.GPUTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GGPUFrameTime));
		Baseline.NumFrames++;
		Baseline.SumTimeSeconds += DeltaSeconds;
	}
	else
	{
		Record.FrameTimeTracker.AddSample(StatUnitData->RawFrameTime);
		Record.GameThreadTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GGameThreadTime));
		Record.RenderThreadTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GRenderThreadTime));
		Record.GPUTimeTracker.AddSample(FPlatformTime::ToMilliseconds(GGPUFrameTime));
		Record.NumFrames++;
		Record.SumTimeSeconds += DeltaSeconds;
	}
}

UAutomationPerformaceHelper::UAutomationPerformaceHelper()
: bRecordingBasicStats(false)
, bRecordingBaselineBasicStats(false)
, bRecordingCPUCapture(false)
, bRecordingStatsFile(false)
{
}

void UAutomationPerformaceHelper::BeginRecordingBaseline(FString RecordName)
{
	bRecordingBasicStats = true;
	bRecordingBaselineBasicStats = true;
	Records.Add(FPerfStatsRecord(RecordName));
	GEngine->SetEngineStat(GetOuter()->GetWorld(), GetOuter()->GetWorld()->GetGameViewport(), TEXT("Unit"), true);
}

void UAutomationPerformaceHelper::EndRecordingBaseline()
{
	bRecordingBaselineBasicStats = false;
	bRecordingBasicStats = false;
}

void UAutomationPerformaceHelper::BeginRecording(FString RecordName, float InGPUBudget, float InRenderThreadBudget, float InGameThreadBudget)
{
	//Ensure we're recording engine stats.
	GEngine->SetEngineStat(GetOuter()->GetWorld(), GetOuter()->GetWorld()->GetGameViewport(), TEXT("Unit"), true);
	bRecordingBasicStats = true;
	bRecordingBaselineBasicStats = false;

	FPerfStatsRecord* CurrRecord = GetCurrentRecord();
	if (!CurrRecord || CurrRecord->Name != RecordName)
	{
		Records.Add(FPerfStatsRecord(RecordName));
		CurrRecord = GetCurrentRecord();
	}

	check(CurrRecord);
	CurrRecord->SetBudgets(InGPUBudget, InRenderThreadBudget, InGameThreadBudget);
}

void UAutomationPerformaceHelper::EndRecording()
{
	if (const FPerfStatsRecord* Record = GetCurrentRecord())
	{
		UE_LOG(LogFunctionalTest, Log, TEXT("Finished Perf Stats Record:\n%s"), *Record->GetReportString());
	}
	bRecordingBasicStats = false;
}

void UAutomationPerformaceHelper::Tick(float DeltaSeconds)
{
	if (bRecordingBasicStats)
	{
		Sample(DeltaSeconds);
	}

	//Other stats need ticking?
}

void UAutomationPerformaceHelper::Sample(float DeltaSeconds)
{
	int32 Index = Records.Num() - 1;
	if (Index >= 0 && bRecordingBasicStats)
	{
		Records[Index].Sample(GetOuter()->GetWorld(), DeltaSeconds, bRecordingBaselineBasicStats);
	}
}

void UAutomationPerformaceHelper::WriteLogFile(const FString& CaptureDir, const FString& CaptureExtension)
{
	FString PathName = FPaths::ProfilingDir();
	if (!CaptureDir.IsEmpty())
	{
		PathName = PathName + (CaptureDir + TEXT("/"));
		IFileManager::Get().MakeDirectory(*PathName);
	}

	FString Extension = CaptureExtension;
	if (Extension.IsEmpty())
	{
		Extension = TEXT("perf.csv");
	}

	const FString Filename = OutputFileBase + Extension;
	const FString FilenameFull = PathName + Filename;
	
	const FString OverBudgetTableHeader = TEXT("TestName, MaxRT, RT Budget, RT Frac, MaxGT, GT Budget, GT Frac, MaxGPU, GPU Budget, GPU Frac\n");
	FString OverbudgetTable;
	const FString DataTableHeader = TEXT("TestName,MinFrameTime,AvgFrameTime,MaxFrameTime,MinRT,AvgRT,MaxRT,MinGT,AvgGT,MaxGT,MinGPU,AvgGPU,MaxGPU\n");
	FString AdjustedTable;
	FString RecordTable;
	FString BaselineTable;
	for (FPerfStatsRecord& Record : Records)
	{
		AdjustedTable += Record.GetReportString() + FString(TEXT("\n"));
		RecordTable += Record.GetRecordString() + FString(TEXT("\n"));
		BaselineTable += Record.GetBaselineString() + FString(TEXT("\n"));

		if (!Record.IsWithinGPUBudget() || !Record.IsWithinRenderThreadBudget() || !Record.IsWithinGameThreadBudget())
		{
			OverbudgetTable += Record.GetOverBudgetString() + FString(TEXT("\n"));
		}
	}

	FString FileContents = FString::Printf(TEXT("Over Budget Tests\n%s%s\nAdjusted Results\n%s%s\nRaw Results\n%s%s\nBaseline Results\n%s%s\n"), 
		*OverBudgetTableHeader, *OverbudgetTable, *DataTableHeader, *AdjustedTable, *DataTableHeader, *RecordTable, *DataTableHeader, *BaselineTable);

	FFileHelper::SaveStringToFile(FileContents, *FilenameFull);

	UE_LOG(LogTemp, Display, TEXT("Finished test, wrote file to %s"), *FilenameFull);

	Records.Empty();
	bRecordingBasicStats = false;
	bRecordingBaselineBasicStats = false;
}

bool UAutomationPerformaceHelper::IsRecording()const 
{
	return bRecordingBasicStats;
}

void UAutomationPerformaceHelper::OnBeginTests()
{
	OutputFileBase = CreateProfileFilename(TEXT(""), true);
	StartOfTestingTime = FDateTime::Now().ToString();
}

void UAutomationPerformaceHelper::OnAllTestsComplete()
{
	if (bRecordingBaselineBasicStats)
	{
		EndRecordingBaseline();
	}

	if (bRecordingBasicStats)
	{
		EndRecording();
	}

	if (bRecordingCPUCapture)
	{
		StopCPUProfiling();
	}

	if (bRecordingStatsFile)
	{
		EndStatsFile();
	}

	if (Records.Num() > 0)
	{
		WriteLogFile(TEXT(""), TEXT("perf.csv"));
	}
}

bool UAutomationPerformaceHelper::IsCurrentRecordWithinGPUBudget()const
{
	if (const FPerfStatsRecord* Curr = GetCurrentRecord())
	{
		return Curr->IsWithinGPUBudget();
	}
	return true;
}

bool UAutomationPerformaceHelper::IsCurrentRecordWithinGameThreadBudget()const
{
	if (const FPerfStatsRecord* Curr = GetCurrentRecord())
	{
		return Curr->IsWithinGameThreadBudget();
	}
	return true;
}

bool UAutomationPerformaceHelper::IsCurrentRecordWithinRenderThreadBudget()const
{
	if (const FPerfStatsRecord* Curr = GetCurrentRecord())
	{
		return Curr->IsWithinRenderThreadBudget();
	}
	return true;
}

const FPerfStatsRecord* UAutomationPerformaceHelper::GetCurrentRecord()const
{
	int32 Index = Records.Num() - 1;
	if (Index >= 0)
	{
		return &Records[Index];
	}
	return nullptr;
}
FPerfStatsRecord* UAutomationPerformaceHelper::GetCurrentRecord()
{
	int32 Index = Records.Num() - 1;
	if (Index >= 0)
	{
		return &Records[Index];
	}
	return nullptr;
}

void UAutomationPerformaceHelper::StartCPUProfiling()
{
	UE_LOG(LogFunctionalTest, Log, TEXT("START PROFILING..."));
	ExternalProfiler.StartProfiler(false);
}

void UAutomationPerformaceHelper::StopCPUProfiling()
{
	UE_LOG(LogFunctionalTest, Log, TEXT("STOP PROFILING..."));
	ExternalProfiler.StopProfiler();
}

void UAutomationPerformaceHelper::TriggerGPUTrace()
{
	// Need to look at Razor GPU to work out what can be done here.
}

void UAutomationPerformaceHelper::BeginStatsFile(const FString& RecordName)
{
	FString MapName = GetOuter()->GetWorld()->GetMapName();
	FString Cmd = FString::Printf(TEXT("Stat StartFile %s-%s/%s.ue4stats"), *MapName, *StartOfTestingTime, *RecordName);
	GEngine->Exec(GetOuter()->GetWorld(), *Cmd);
}

void UAutomationPerformaceHelper::EndStatsFile()
{
	GEngine->Exec(GetOuter()->GetWorld(), TEXT("Stat StopFile"));
}