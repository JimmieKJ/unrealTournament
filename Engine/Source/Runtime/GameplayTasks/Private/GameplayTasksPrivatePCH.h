// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

#include "Core.h"
#include "Engine.h"

#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "VisualLogger.h"

#include "GameplayTasksComponent.h"
#include "GameplayTask.h"

DECLARE_STATS_GROUP(TEXT("GameplayTasks"), STATGROUP_GameplayTasks, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("TickGameplayTasks"), STAT_TickGameplayTasks, STATGROUP_GameplayTasks, );
