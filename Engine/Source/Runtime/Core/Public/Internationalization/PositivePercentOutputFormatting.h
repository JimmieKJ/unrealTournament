// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealString.h"

namespace EPercentPositivePattern
{
	/** 
	 * enum value associated with a pattern used to display positive percent values
	 * Below X represents the number being converted to display as a percent, Y will be replaced
	 * with the cultural values PercentSymbol
	 *
	 * |-----|------------|
	 * | X Y | XSpacePct  |
	 * | XY	 | XPct       |
     * | YX	 | PctX       |
	 * | Y X | PctSpaceX  |
	 * |-----|------------|
	 */
	enum Type
	{
		XSpacePct,
		XPct,
		PctX,
		PctSpaceX,
		MaxValue
	};
}

// Used by the globalisation system to format positive percent values based on the EPercentPositivePattern::Type value for the culture.
class FPositivePercentOutputFormatting
{
public:
	FPositivePercentOutputFormatting()
	{
	}

	FPositivePercentOutputFormatting(const EPercentPositivePattern::Type PPPValue, const FString& PercentSymbol)
	{
		check(0 <=PPPValue && PPPValue < EPercentPositivePattern::MaxValue);
		switch (PPPValue) 
		{
			case EPercentPositivePattern::XSpacePct:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = FString(TEXT("\x00A0")) + PercentSymbol;
				break;
			case EPercentPositivePattern::XPct:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = PercentSymbol;
				break;
			case EPercentPositivePattern::PctX:
				PreNumberFormatting = PercentSymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case EPercentPositivePattern::PctSpaceX:
				PreNumberFormatting = PercentSymbol + FString(TEXT("\x00A0"));
				PostNumberFormatting = FString(TEXT(""));
				break;
			default:
				// will have failed the check prior to this default.
				break;
		}
	}

	// Return a positively formatted culturally specific percent string from the string InValue;
	FString FormatString(FString InValue)const 
	{
		return PreNumberFormatting + InValue + PostNumberFormatting;
	}

protected:
	FString PreNumberFormatting;
	FString PostNumberFormatting;
};
