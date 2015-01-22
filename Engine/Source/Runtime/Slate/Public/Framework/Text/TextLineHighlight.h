// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class ILineHighlighter;

struct SLATE_API FTextLineHighlight
{
	FTextLineHighlight( int32 InLineIndex, const FTextRange& InRange, int32 InZOrder, const TSharedRef< ILineHighlighter >& InHighlighter )
		: LineIndex( InLineIndex )
		, Range( InRange )
		, ZOrder( InZOrder )
		, Highlighter( InHighlighter )
	{

	}

	int32 LineIndex;
	FTextRange Range;
	int32 ZOrder;
	TSharedRef< ILineHighlighter > Highlighter;
};
