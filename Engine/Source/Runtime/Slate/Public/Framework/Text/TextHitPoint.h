// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

/** Describes how a line was hit when performing a screen-space -> view model conversion */
enum class ETextHitPoint : uint8
{
	/** The hit-point was within the text range */
	WithinText,
	/** The hit-point was to left of the text range (the gutter) */
	LeftGutter,
	/** The hit-point was to right of the text range (the gutter) */
	RightGutter,
};
