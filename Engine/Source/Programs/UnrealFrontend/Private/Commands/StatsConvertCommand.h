// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StatsData.h"
#include "StatsFile.h"


class FStatsConvertCommand
{
public:

	/** Executes the command. */
	static void Run()
	{
		FStatsConvertCommand Instance;
		Instance.InternalRun();
	}

protected:

	void InternalRun();

};
