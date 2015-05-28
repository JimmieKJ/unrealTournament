// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "FunctionalTest.generated.h"

class UBillboardComponent;

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

protected:
	void GoToObservationPoint();

public:
	FFunctionalTestDoneSignature TestFinishedObserver;

protected:
	uint32 bIsRunning;
	float TotalTime;

public:
	/** Returns SpriteComponent subobject **/
	UBillboardComponent* GetSpriteComponent();
};
