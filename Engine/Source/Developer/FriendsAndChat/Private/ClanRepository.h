// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Maintains list of Clan items 
*/
class IClanRepository
{
public:

	virtual void EnumerateClanList(TArray<TSharedRef<class IClanInfo>>& OUTClanList) = 0;

	DECLARE_EVENT(IClanRepository, FOnClanListUpdated)
	virtual FOnClanListUpdated& OnClanListUpdated() = 0;

	DECLARE_EVENT(IClanRepository, FOnClanJoined)
	virtual FOnClanJoined& OnClanJoined() = 0;

	DECLARE_EVENT(IClanRepository, FOnClanLeft)
	virtual FOnClanLeft& OnClanLeft() = 0;
};

FACTORY(TSharedRef<IClanRepository>, FClanRepository)