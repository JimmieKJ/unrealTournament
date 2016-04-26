// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FTracePath

#define TRACEPATH_DEBUG 0

class FTracePath
{
public:

	FTracePath()
		: CachedCrc(0U)
	{
	}

	FTracePath(const FTracePath& TracePathIn)
		: CachedCrc(TracePathIn.CachedCrc)
#if TRACEPATH_DEBUG
		, TraceStack(TracePathIn.TraceStack)
#endif
	{
	}

#if TRACEPATH_DEBUG
	/** Copy operator */
	FTracePath& operator = (const FTracePath& TracePathIn)
	{
		TraceStack = TracePathIn.TraceStack;
		CachedCrc = TracePathIn.CachedCrc;
		return *this;
	}
#endif

	/** Cast to uint32 operator */
	operator const uint32() const { return CachedCrc; }

	/** Add exit pin */
	void AddExitPin(const int32 ExitPinScriptOffset)
	{
#if TRACEPATH_DEBUG
		TraceStack.Add(ExitPinScriptOffset);
#endif
		CachedCrc = FCrc::MemCrc32(&ExitPinScriptOffset, 1, CachedCrc);
	}

	/** Reset */
	void Reset()
	{
#if TRACEPATH_DEBUG
		TraceStack.SetNum(0);
#endif
		CachedCrc = 0;

	}

#if TRACEPATH_DEBUG
	const FString GetPathString() const
	{
		FString PathString(TEXT("<"));
		for (auto Iter : TraceStack)
		{
			PathString = FString::Printf( TEXT( "%s:%i" ), *PathString, Iter);
		}
		PathString += TEXT(">");
		return PathString;
	}
#endif

private:

#if TRACEPATH_DEBUG
	TArray<int32> TraceStack;
#endif
	/** Cached crc */
	uint32 CachedCrc;

};
