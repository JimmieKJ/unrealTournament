// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTasksPrivatePCH.h"
#include "GameplayTaskOwnerInterface.h"

void IGameplayTaskOwnerInterface::OnTaskInitialized(UGameplayTask& Task)
{

}

AActor* IGameplayTaskOwnerInterface::GetAvatarActor(const UGameplayTask* Task) const
{ 
	return GetOwnerActor(Task);
}
