// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "AbilityTask_PlayMontageAndWait.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMontageWaitSimpleDelegate);

UCLASS(MinimalAPI)
class UAbilityTask_PlayMontageAndWait : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FMontageWaitSimpleDelegate	OnComplete;

	UPROPERTY(BlueprintAssignable)
	FMontageWaitSimpleDelegate	OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FMontageWaitSimpleDelegate	OnCancelled;

	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (DisplayName="PlayMontageAndWait",
		HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_PlayMontageAndWait* CreatePlayMontageAndWaitProxy(UObject* WorldContextObject,
		FName TaskInstanceName, UAnimMontage *MontageToPlay, float Rate = 1.f, FName StartSection = NAME_None);

	virtual void Activate() override;

	/** Called when the ability is asked to cancel from an outside node. What this means depends on the individual task. By default, this does nothing other than ending the task. */
	virtual void ExternalCancel() override;

	virtual FString GetDebugString() const override;

private:

	virtual void OnDestroy(bool AbilityEnded) override;

	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FDelegateHandle InterruptedHandle;

	UAnimMontage* MontageToPlay;
	float Rate;	
	FName StartSection;
};