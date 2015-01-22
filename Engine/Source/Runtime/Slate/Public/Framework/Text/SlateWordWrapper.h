// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WordWrapper.h"

struct FWrappedStringSlice;

namespace SlateWordWrapper
{
	/**
	 * Produce hard-wrapped string
	 * 
	 * @param InString				The unwrapped text.
	 * @param InFontInfo			The font used to render the text.
	 * @param InWrapWidth			The width available.
	 * @param OutWrappedLineData	An optional array to fill with the indices from the source string marking the begin and end points of the wrapped lines
	 *
	 * @return The resulting string with newlines inserted.
	 */
	FString SLATE_API WrapText( const FString& InString, const FSlateFontInfo& InFontInfo, const int32 InWrapWidth, const float InFontScale, FWordWrapper::FWrappedLineData* const OutWrappedLineData = nullptr );

	/**
	 * Produce soft-wrapping information about TextLines
	 * 
	 * @param InTextLines           Lines of text, each string represents a run of text followed by a \n
	 * @param InWrapWidth           The width (in Slate Units) at which we want the text wrapped.
	 * @param OutWrappedText		Information about how the text would be wrapped given
	 * @param OutWrappedLineData	An optional array to fill with the indices from the source string marking the begin and end points of the wrapped lines
	 * 
	 */
	void SLATE_API WrapText( const TArray< TSharedRef<FString> >& InTextLines, const FSlateFontInfo& InFontInfo, const float InWrapWidth, const float InFontScale, TArray< FWrappedStringSlice >& OutWrappedText, FWordWrapper::FWrappedLineData* const OutWrappedLineData = nullptr );
}