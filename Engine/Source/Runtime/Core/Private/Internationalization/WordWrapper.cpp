// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "WordWrapper.h"
#include "BreakIterator.h"

FWordWrapper::FWordWrapper(const TCHAR* const InString, const int32 InStringLength, FWrappedLineData* const OutWrappedLineData)
	: String(InString)
	, StringLength(InStringLength)
	, GraphemeBreakIterator(FBreakIterator::CreateCharacterBoundaryIterator())
	, LineBreakIterator(FBreakIterator::CreateLineBreakIterator())
	, StartIndex(0)
	, WrappedLineData(OutWrappedLineData)
{
	GraphemeBreakIterator->SetString(InString, InStringLength);
	LineBreakIterator->SetString(InString, InStringLength);

	if(WrappedLineData)
	{
		WrappedLineData->Empty();
	}
}

FWordWrapper::~FWordWrapper()
{
}

bool FWordWrapper::ProcessLine()
{
	bool bHasAddedLine = false;
	if(StartIndex < StringLength)
	{
		int32 BreakIndex = FindFirstMandatoryBreakBetween(StartIndex, StringLength);

		int32 NextStartIndex;
		if( BreakIndex == INDEX_NONE || !DoesSubstringFit(StartIndex, BreakIndex) )
		{
			BreakIndex = INDEX_NONE;
			int32 WrapIndex = FindIndexAtOrAfterWrapWidth(StartIndex);

			if(WrapIndex == StringLength)
			{
				BreakIndex = WrapIndex;
			}

			if(BreakIndex <= StartIndex) // No mandatory break.
			{
				BreakIndex = FindLastBreakCandidateBetween(StartIndex, WrapIndex);
			}

			if(BreakIndex <= StartIndex) // No candidate break.
			{
				// Break after minimum length that would preserve the meaning/appearance.
				BreakIndex = FindEndOfLastWholeGraphemeCluster(StartIndex, WrapIndex);
			}

			if(BreakIndex <= StartIndex) // No complete grapheme cluster.
			{
				BreakIndex = WrapIndex; // Break at wrap.
			}
			// Index for the next search
			NextStartIndex = BreakIndex;
		}
		else
		{
			// Index for the next search
			NextStartIndex = BreakIndex;
			// The index is inclusive of the break - we don't want the break char in the string
			--BreakIndex;			
		}

		while (BreakIndex > 0 && FText::IsWhitespace(String[BreakIndex - 1]))
		{
			--BreakIndex;
		}

		if( StartIndex <= BreakIndex )
		{
			AddLine(StartIndex, BreakIndex);

			bHasAddedLine = true;
		}

		while (NextStartIndex < StringLength && FText::IsWhitespace(String[NextStartIndex]))
		{
			++NextStartIndex;
		}

		if(WrappedLineData)
		{
			WrappedLineData->Add(TPairInitializer<int32, int32>(StartIndex, BreakIndex));
		}

		StartIndex = NextStartIndex;
	}
	return bHasAddedLine;
}

/*
bool FWordWrapper::DoesSubstringFit(const int32 StartIndex, const int32 EndIndex)
{
	return false;
}

int32 FWordWrapper::FindIndexAtOrAfterWrapLength(const int32 StartIndex)
{
	return INDEX_NONE;
}

void FWordWrapper::NewLine(const int32 StartIndex, const int32 EndIndex)
{

}
*/

int32 FWordWrapper::FindFirstMandatoryBreakBetween(const int32 InStartIndex, const int32 WrapIndex)
{
	int32 BreakIndex = INDEX_NONE;
	for(int32 i = InStartIndex + 1; i < WrapIndex; ++i)
	{
		const TCHAR Previous = String[i - 1];
		if( FChar::IsLinebreak(Previous) ) // Line break occurs *after* linebreak character.
		{
			const TCHAR* const Current = i < WrapIndex ? String + i : NULL;
			if(	Previous != FChar::CarriageReturn || !(Current && *Current == FChar::LineFeed) ) // Line break cannot occur within CR LF pair.
			{
				BreakIndex = i;
				break;
			}
		}
	}
	// If we reached the end of the string we must also check that the last char is not a newline
	if( BreakIndex == INDEX_NONE )
	{
		const TCHAR Previous = String[WrapIndex - 1];
		if( FChar::IsLinebreak(Previous) ) // Line break occurs *after* linebreak character.
		{
			BreakIndex = WrapIndex;
		}
	}
	return BreakIndex;
}

int32 FWordWrapper::FindLastBreakCandidateBetween(const int32 InStartIndex, const int32 WrapIndex)
{
	int32 BreakIndex = LineBreakIterator->MoveToCandidateBefore(WrapIndex + 1);
	if(BreakIndex < InStartIndex)
	{
		BreakIndex = INDEX_NONE;
	}
	return BreakIndex;
}

int32 FWordWrapper::FindEndOfLastWholeGraphemeCluster(const int32 InStartIndex, const int32 WrapIndex)
{
	int32 BreakIndex = GraphemeBreakIterator->MoveToCandidateBefore(WrapIndex + 1);
	if(BreakIndex < InStartIndex)
	{
		BreakIndex = INDEX_NONE;
	}
	return BreakIndex;
}