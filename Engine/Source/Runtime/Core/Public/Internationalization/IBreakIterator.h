// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class IBreakIterator
{
public:
	virtual ~IBreakIterator() {}

	virtual void SetString(const FText& InText) = 0;
	virtual void SetString(const FString& InString) = 0;
	virtual void SetString(const TCHAR* const InString, const int32 InStringLength) = 0;
	virtual void ClearString() = 0;

	virtual int32 GetCurrentPosition() const = 0;

	virtual int32 ResetToBeginning() = 0;
	virtual int32 ResetToEnd() = 0;

	virtual int32 MoveToPrevious() = 0;
	virtual int32 MoveToNext() = 0;
	virtual int32 MoveToCandidateBefore(const int32 InIndex) = 0;
	virtual int32 MoveToCandidateAfter(const int32 InIndex) = 0;
};
