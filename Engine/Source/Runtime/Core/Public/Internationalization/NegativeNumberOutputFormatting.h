// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealString.h"

namespace ENumberNegativePattern
{
	/** 
	 * enum value associated with a pattern used to display negative number values
	 * Below X represents the number being converted to display, - will be replaced
	 * with the cultural value NegativeSign
	 *
	 * |--------|----------------------|
	 * | (X)	| BracketXBracket      |
	 * | -X		| NegativeSymbolX      |
	 * | - X	| NegativeSymbolSpaceX |
	 * | X-		| XNegativeSymbol      |
	 * | X -	| XSpaceNegativeSymbol |
	 * |--------|----------------------|
	 */
	enum Type
	{
		BracketXBracket,
		NegativeSymbolX,
		NegativeSymbolSpaceX,
		XNegativeSymbol,
		XSpaceNegativeSymbol,
		MaxValue
	};
}

// Used by the globalisation system to format negative numbers based on the ENumberNegativePattern::Type value for the culture.
class FNegativeNumberOutputFormatting
{
public:
	FNegativeNumberOutputFormatting()
	{
	}

	FNegativeNumberOutputFormatting(const ENumberNegativePattern::Type NNPValue, const FString& NegativeSymbol)
	{
		check (0 <= NNPValue && NNPValue < ENumberNegativePattern::MaxValue);
		switch (NNPValue) 
		{
			case ENumberNegativePattern::BracketXBracket:
				PreNumberFormatting = FString(TEXT("("));
				PostNumberFormatting = FString(TEXT(")"));
				break;
			case ENumberNegativePattern::NegativeSymbolX:
				PreNumberFormatting = NegativeSymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case ENumberNegativePattern::NegativeSymbolSpaceX:
				PreNumberFormatting = NegativeSymbol + FString(TEXT("\x00A0"));				
				PostNumberFormatting = FString(TEXT(""));
				break;
			case ENumberNegativePattern::XNegativeSymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = NegativeSymbol;
				break;
			case ENumberNegativePattern::XSpaceNegativeSymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = FString(TEXT("\x00A0")) + NegativeSymbol;
				break;
			default:
				// will have failed the check prior to this default.
				break;
		}
	}

	// Return a negatively formatted culturally specific number string from the string InValue;
	FString FormatString(FString InValue)const
	{
		return PreNumberFormatting + InValue + PostNumberFormatting;
	}

protected:
	FString PreNumberFormatting;
	FString PostNumberFormatting;
};