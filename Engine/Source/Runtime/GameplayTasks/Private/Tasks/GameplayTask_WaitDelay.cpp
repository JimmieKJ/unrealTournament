// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTasksPrivatePCH.h"
#include "Tasks/GameplayTask_WaitDelay.h"

UGameplayTask_WaitDelay::UGameplayTask_WaitDelay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Time = 0.f;
	TimeStarted = 0.f;
}

UGameplayTask_WaitDelay* UGameplayTask_WaitDelay::TaskWaitDelay(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, float Time)
{
	auto MyObj = NewTask<UGameplayTask_WaitDelay>(TaskOwner);
	if (MyObj)
	{
		MyObj->Time = Time;
	}
	return MyObj;
}

void UGameplayTask_WaitDelay::Activate()
{
	UWorld* World = GetWorld();
	TimeStarted = World->GetTimeSeconds();

	// Use a dummy timer handle as we don't need to store it for later but we don't need to look for something to clear
	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(TimerHandle, this, &UGameplayTask_WaitDelay::OnTimeFinish, Time, false);
}

void UGameplayTask_WaitDelay::OnTimeFinish()
{
	OnFinish.Broadcast();
	EndTask();
}

FString UGameplayTask_WaitDelay::GetDebugString() const
{
	float TimeLeft = Time - GetWorld()->TimeSince(TimeStarted);
	return FString::Printf(TEXT("WaitDelay. Time: %.2f. TimeLeft: %.2f"), Time, TimeLeft);
}