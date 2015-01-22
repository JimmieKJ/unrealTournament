// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class IRunRenderer;

struct SLATE_API FTextRunRenderer
{
	FTextRunRenderer( int32 InLineIndex, const FTextRange& InRange, const TSharedRef< IRunRenderer >& InRenderer )
		: LineIndex( InLineIndex )
		, Range( InRange )
		, Renderer( InRenderer )
	{

	}

	int32 LineIndex;
	FTextRange Range;
	TSharedRef< IRunRenderer > Renderer;
};
