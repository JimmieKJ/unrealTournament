// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EAllowOverscroll : uint8
{
	Yes,
	No
};

/**
 * Handles overscroll management.
 */
struct SLATE_API FOverscroll
{
public:

	FOverscroll();

	/** @return The Amount actually scrolled */
	float ScrollBy(float Delta);

	/** How far the user scrolled above/below the beginning/end of the list. */
	float GetOverscroll() const;

	/** Ticks the overscroll manager so it can animate. */
	void UpdateOverscroll(float InDeltaTime);

	/**
	 * Should ScrollDelta be applied to overscroll or to regular item scrolling.
	 *
	 * @param bIsAtStartOfList  Are we at the very beginning of the list (i.e. showing the first item at the top of the view)?
	 * @param bIsAtEndOfList    Are we showing the last item on the screen completely?
	 * @param ScrollDelta       How much the user is trying to scroll in Slate Units.
	 *
	 * @return true if the user's scrolling should be applied toward overscroll.
	 */
	bool ShouldApplyOverscroll(const bool bIsAtStartOfList, const bool bIsAtEndOfList, const float ScrollDelta) const;

private:
	/** How much we've over-scrolled above/below the beginning/end of the list, stored in log form */
	float OverscrollAmount;
};
