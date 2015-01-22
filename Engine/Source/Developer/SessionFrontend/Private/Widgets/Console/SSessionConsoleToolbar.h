// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SSessionConsoleToolbar"


/**
 * Implements the device toolbar widget.
 */
class SSessionConsoleToolbar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionConsoleToolbar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param CommandList The command list to use.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<FUICommandList>& CommandList );
};


#undef LOCTEXT_NAMESPACE
