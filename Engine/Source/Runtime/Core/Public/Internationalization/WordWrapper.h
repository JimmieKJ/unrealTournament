// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IBreakIterator.h"

// Abstract Base Class for word wrapping functionality.
class CORE_API FWordWrapper
{
public:
	/** Array of indices where the wrapped lines begin and end in the source string */
	typedef TArray< TPair<int32, int32> > FWrappedLineData;

	/**
	 * Constructor for passing a string to be wrapped.
	 *
	 * @param	String	The string to be wrapped. Contents must not be modified after being passed in.
	 * @param	StringLength	The number of TCHAR elements in String.
	 * @param	OutWrappedLineData An optional array to fill with the indices from the source string marking the begin and end points of the wrapped lines
	 */
	FWordWrapper(const TCHAR* const String, const int32 StringLength, FWrappedLineData* const OutWrappedLineData);

	virtual ~FWordWrapper() = 0;

	/**
	 * Processes the string using a word wrapping algorithm, resulting in up to a single line.
	 *
	 * @return	True if a new line could be processed, false otherwise such as having reached the end of the string.
	 */
	bool ProcessLine();
protected:
	/**
	 * Stub method that should measure the substring of the range [StartIndex, EndIndex).
	 *
	 * @return	True if the substring fits the desired width.
	 */
	virtual bool DoesSubstringFit(const int32 StartIndex, const int32 EndIndex) = 0;

	/**
	 * Stub method that should measure the substring starting from StartIndex until the wrap width is found or no more indices remain.
	 *
	 * @return	The index of the character that is at or after the desired width.
	 */
	virtual int32 FindIndexAtOrAfterWrapWidth(const int32 StartIndex) = 0;

	/**
	 * Stub method for processing a single produced substring of the range [StartIndex, EndIndex) as a new line.
	 */
	virtual void AddLine(const int32 StartIndex, const int32 EndIndex) = 0;
private:
	int32 FindFirstMandatoryBreakBetween(const int32 StartIndex, const int32 WrapIndex);
	int32 FindLastBreakCandidateBetween(const int32 StartIndex, const int32 WrapIndex);
	int32 FindEndOfLastWholeGraphemeCluster(const int32 StartIndex, const int32 WrapIndex);
protected:
	const TCHAR* const String;
	const int32 StringLength;
private:
	TSharedPtr<IBreakIterator> GraphemeBreakIterator;
	TSharedPtr<IBreakIterator> LineBreakIterator;
	int32 StartIndex;
	FWrappedLineData* WrappedLineData;
};