// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UserActivityTracking.h"
#include "Misc/CoreDelegates.h"

FUserActivityTracking::FOnActivityChanged FUserActivityTracking::OnActivityChanged;

FUserActivity FUserActivityTracking::UserActivity;

void FUserActivityTracking::SetActivity(const FUserActivity& InUserActivity)
{ 
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FUserActivityTracking_SetActivity);
	UserActivity = InUserActivity;
	OnActivityChanged.Broadcast(UserActivity);
	FCoreDelegates::UserActivityStringChanged.Broadcast(UserActivity.ActionName);
}
