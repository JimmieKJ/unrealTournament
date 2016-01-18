// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Interface.h"
#include "GameplayTaskOwnerInterface.generated.h"

class UGameplayTasksComponent;
class UGameplayTask;
class AActor;

namespace FGameplayTasks
{
	static const uint8 DefaultPriority = 127;
}

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGameplayTaskOwnerInterface : public UInterface
{
	GENERATED_BODY()
};

class GAMEPLAYTASKS_API IGameplayTaskOwnerInterface
{
	GENERATED_BODY()
public:
	virtual void OnTaskInitialized(UGameplayTask& Task);
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const = 0;
	/** this gets called both when task starts and when task gets resumed. Check Task.GetStatus() if you want to differenciate */
	virtual void OnTaskActivated(UGameplayTask& Task) = 0;
	/** this gets called both when task finished and when task gets paused. Check Task.GetStatus() if you want to differenciate */
	virtual void OnTaskDeactivated(UGameplayTask& Task) = 0;
	virtual AActor* GetOwnerActor(const UGameplayTask* Task) const = 0;
	virtual AActor* GetAvatarActor(const UGameplayTask* Task) const;
	virtual uint8 GetDefaultPriority() const { return FGameplayTasks::DefaultPriority; }
};