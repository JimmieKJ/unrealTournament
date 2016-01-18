// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "CookingStatsPCH.h"
#include "CookingStatsModule.h"

#include "Developer/MessageLog/Public/MessageLogModule.h"

#define LOCTEXT_NAMESPACE "CookingStats"


IMPLEMENT_MODULE(FCookingStatsModule, CookingStats);

void FCookingStatsModule::StartupModule()
{
	CookingStats = new FCookingStats();

}


void FCookingStatsModule::ShutdownModule()
{
	if (CookingStats)
	{
		delete CookingStats;
		CookingStats = NULL;
	}
}

ICookingStats& FCookingStatsModule::Get() const
{
	check(CookingStats);
	return *CookingStats;
}

#undef LOCTEXT_NAMESPACE