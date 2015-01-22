// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealString.h"

namespace EPercentNegativePattern
{
	/** 
	 * enum value associated with a pattern used to display positive percent values
	 * Below X represents the number being converted to display as a percent, Y will be replaced
	 * with the cultural values PercentSymbol
	 *
	 * |------------|-------------------------|
	 * | -X Y		| NegativeSymbolXSpacePct |
	 * | -XY		| NegativeSymbolXPct      |
	 * | -YX		| NegativeSymbolPctX      |
	 * | Y-X		| PctNegativeSymbolX      |
	 * | YX-		| PctXNegativeSymbol      |
	 * | X-Y		| XNegativeSymbolPct      |
	 * | XY-		| XPctNegativeSymbol      |
	 * | -Y X		| NegativeSymbolPctSpaceX |
	 * | X Y-		| XSpacePctNegativeSymbol |
	 * | Y X-		| PctSpaceXNegativeSymbol |
	 * | Y -X		| PctSpaceNegativeSymbolX |
	 * | X- Y		| XNegativeSymbolSpacePct |
	 * |------------|-------------------------|
	 */
	enum Type
	{
		NegativeSymbolXSpacePct,
		NegativeSymbolXPct,
		NegativeSymbolPctX,
		PctNegativeSymbolX,
		PctXNegativeSymbol,
		XNegativeSymbolPct,
		XPctNegativeSymbol,
		NegativeSymbolPctSpaceX,
		XSpacePctNegativeSymbol,
		PctSpaceXNegativeSymbol,
		PctSpaceNegativeSymbolX,
		XNegativeSymbolSpacePct,
		MaxValue
	};
}

// Used by the globalisation system to format negative percent values based on the EPercentPositivePattern::Type value for the culture.
class FNegativePercentOutputFormatting
{
public:

	FNegativePercentOutputFormatting()
	{
	}

	FNegativePercentOutputFormatting(const EPercentNegativePattern::Type PNPValue, const FString& PercentSymbol, const FString& NegativeSymbol)
	{
		check(0 <=PNPValue && PNPValue < EPercentNegativePattern::MaxValue);
		switch (PNPValue) 
		{
			case EPercentNegativePattern::NegativeSymbolXSpacePct:
				PreNumberFormatting = NegativeSymbol;
				PostNumberFormatting = FString(TEXT("\x00A0")) + PercentSymbol;
				break;
			case EPercentNegativePattern::NegativeSymbolXPct:
				PreNumberFormatting = NegativeSymbol;
				PostNumberFormatting = PercentSymbol;
				break;
			case EPercentNegativePattern::NegativeSymbolPctX:
				PreNumberFormatting = NegativeSymbol + PercentSymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case EPercentNegativePattern::PctNegativeSymbolX:
				PreNumberFormatting = PercentSymbol + NegativeSymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case EPercentNegativePattern::PctXNegativeSymbol:
				PreNumberFormatting = PercentSymbol;
				PostNumberFormatting = NegativeSymbol;
				break;
			case EPercentNegativePattern::XNegativeSymbolPct:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = NegativeSymbol + PercentSymbol;
				break;
			case EPercentNegativePattern::XPctNegativeSymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = PercentSymbol + NegativeSymbol;
				break;
			case EPercentNegativePattern::NegativeSymbolPctSpaceX:
				PreNumberFormatting = NegativeSymbol + PercentSymbol + FString(TEXT("\x00A0"));
				PostNumberFormatting = FString(TEXT(""));
				break;
			case EPercentNegativePattern::XSpacePctNegativeSymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = FString(TEXT("\x00A0")) + PercentSymbol + NegativeSymbol;
				break;
			case EPercentNegativePattern::PctSpaceXNegativeSymbol:
				PreNumberFormatting = PercentSymbol + FString(TEXT("\x00A0"));
				PostNumberFormatting = NegativeSymbol;
				break;
			case EPercentNegativePattern::PctSpaceNegativeSymbolX:
				PreNumberFormatting = PercentSymbol + FString(TEXT("\x00A0")) + NegativeSymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case EPercentNegativePattern::XNegativeSymbolSpacePct:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = NegativeSymbol + FString(TEXT("\x00A0")) + PercentSymbol;
				break;
			default:
				// will have failed the check prior to this default.
				break;
		}
	}

	// Return a negatively formatted culturally specific percent string from the string InValue;
	FString FormatString(FString InValue)const 
	{
		return PreNumberFormatting + InValue + PostNumberFormatting;
	}

protected:
	FString PreNumberFormatting;
	FString PostNumberFormatting;
};
