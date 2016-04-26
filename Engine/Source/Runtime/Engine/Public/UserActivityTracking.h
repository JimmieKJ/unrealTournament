// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class ENGINE_API FUserActivityTracking : FNoncopyable
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnActivityChanged, const FUserActivity&);

	static void SetActivity(const FUserActivity& InUserActivity);
	static const FUserActivity& GetUserActivity() { return UserActivity; }

	// Called when the UserActivity changes
	static FOnActivityChanged OnActivityChanged;

private:
	static FUserActivity UserActivity;
};

