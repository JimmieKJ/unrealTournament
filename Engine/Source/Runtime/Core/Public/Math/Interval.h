// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Interval.h: Declares the FInterval structure.
=============================================================================*/

#pragma once


/**
 * Structure for intervals of floating-point numbers.
 */
struct FInterval
{
	/** Holds the lower bound of the interval. */
	float Min;
	
	/** Holds the upper bound of the interval. */
	float Max;
	
	/** Holds a flag indicating whether the interval is empty. */
	bool bIsEmpty;

public:

	/**
	 * Default constructor.
	 *
	 * The interval is initialized to [0, 0].
	 */
	FInterval( )
		: Min(0.0f)
		, Max(0.0f)
		, bIsEmpty(true)
	{ }

    /**
	 * Creates and initializes a new interval with the specified lower and upper bounds.
	 *
	 * @param InMin The lower bound of the constructed interval.
	 * @param InMax The upper bound of the constructed interval.
	 */
	FInterval( float InMin, float InMax )
		: Min(InMin)
		, Max(InMax)
		, bIsEmpty(InMin >= InMax)
	{ }

public:

	/**
	 * Offset the interval by adding X.
	 *
	 * @param X The offset.
	 */
	void operator+= ( float X )
	{
		if (!bIsEmpty)
		{
			Min += X;
			Max += X;
		}
	}

	/**
	 * Offset the interval by subtracting X.
	 *
	 * @param X The offset.
	 */
	void operator-= ( float X )
	{
		if (!bIsEmpty)
		{
			Min -= X;
			Max -= X;
		}
	}

public:

	/**
	 * Expands this interval to both sides by the specified amount.
	 *
	 * @param ExpandAmount The amount to expand by.
	 */
	void Expand( float ExpandAmount )
	{
		if (!bIsEmpty)
		{
			Min -= ExpandAmount;
			Max += ExpandAmount;
		}
	}

	/**
	 * Expands this interval if necessary to include the specified element.
	 *
	 * @param X The element to include.
	 */
	FORCEINLINE void Include( float X );

public:

	/**
	 * Calculates the intersection of two intervals.
	 *
	 * @param A The first interval.
	 * @param B The second interval.
	 *
	 * @return The intersection.
	 */
	friend FInterval Intersect( const FInterval& A, const FInterval& B )
	{
		if (A.bIsEmpty || B.bIsEmpty)
		{
			return FInterval();
		}

		return FInterval(FMath::Max(A.Min, B.Min), FMath::Min(A.Max, B.Max));
	}
};


/* FInterval inline functions
 *****************************************************************************/

FORCEINLINE void FInterval::Include( float X )
{
	if (bIsEmpty)
	{
		Min = X;
		Max = X;
		bIsEmpty = false;
	}
	else
	{
		if (X < Min)
		{
			Min = X;
		}

		if (X > Max)
		{
			Max = X;
		}
	}
}
