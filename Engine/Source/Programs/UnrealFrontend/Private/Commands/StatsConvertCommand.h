// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	void WriteString( FArchive& Writer, const ANSICHAR* Format, ... );
	void CollectAndWriteStatsValues( FArchive& Writer );
	void ReadAndConvertStatMessages( FArchive& Reader, FArchive& Writer );

private:

	FStatsReadStream Stream;
	FStatsThreadState ThreadState;
	TArray<FName> StatList;
};
