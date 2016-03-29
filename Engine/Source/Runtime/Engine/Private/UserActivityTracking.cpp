// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "UserActivityTracking.h"

FUserActivityTracking::FOnActivityChanged FUserActivityTracking::OnActivityChanged;

FUserActivity FUserActivityTracking::UserActivity;

void FUserActivityTracking::SetActivity(const FUserActivity& InUserActivity)
{ 
	UserActivity = InUserActivity;
	OnActivityChanged.Broadcast(UserActivity);
	FCoreDelegates::UserActivityStringChanged.Broadcast(UserActivity.ActionName);
}
