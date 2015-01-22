// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Holds generic memory stats, internally implemented as a map. */
struct FGenericMemoryStats
{
	void Add( const TCHAR* StatDescription, const SIZE_T StatValue )
	{
		Data.Add( FString(StatDescription), StatValue );
	}

	TMap<FString, SIZE_T> Data;
};
