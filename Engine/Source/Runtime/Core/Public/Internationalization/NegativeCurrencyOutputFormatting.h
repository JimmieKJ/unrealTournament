// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealString.h"

namespace ECurrencyNegativePattern
{
	/** 
	 * enum value associated with a pattern used to display negative currency values
	 * Below X represents the number being converted to display as a currency, $ and - will be replaced
	 * with the cultural values CurrencySymbol and NegativeSign respectively
	 *
	 * |------------|------------------------------------|
	 * | ($X)		| BracketCurrencySymbolXBracket      |
	 * | -$X		| NegativeSymbolCurrencySymbolX      |
	 * | $-X		| CurrencySymbolNegativeSymbolX      |
	 * | $X-		| CurrencySymbolXNegativeSymbol      |
	 * | (X$)		| BracketXCurrencySymbolBracket      |
	 * | -X$		| NegativeSymbolXCurrencySymbol      |
	 * | X-$		| XNegativeSymbolCurrencySymbol      |
	 * | X$-		| XCurrencySymbolNegativeSymbol      |
	 * | -X $		| NegativeSymbolXSpaceCurrencySymbol |
	 * | -$ X		| NegativeSymbolCurrencySymbolSpaceX |
	 * | X $-		| XSpaceCurrencySymbolNegativeSymbol |
	 * | $ X-		| CurrencySymbolSpaceXNegativeSymbol |
	 * | $ -X		| CurrencySymbolSpaceNegativeSymbolX |
	 * | X- $		| XNegativeSymbolSpaceCurrencySymbol |
	 * | ($ X)	    | BracketCurrencySymbolSpaceXBracket |
	 * | (X $)		| BracketXSpaceCurrencySymbolBracket |
	 * |------------|------------------------------------|
	 */
	enum Type
	{
		BracketCurrencySymbolXBracket,
		NegativeSymbolCurrencySymbolX,
		CurrencySymbolNegativeSymbolX,
		CurrencySymbolXNegativeSymbol,
		BracketXCurrencySymbolBracket,
		NegativeSymbolXCurrencySymbol,
		XNegativeSymbolCurrencySymbol,
		XCurrencySymbolNegativeSymbol,
		NegativeSymbolXSpaceCurrencySymbol,
		NegativeSymbolCurrencySymbolSpaceX,
		XSpaceCurrencySymbolNegativeSymbol,
		CurrencySymbolSpaceXNegativeSymbol,
		CurrencySymbolSpaceNegativeSymbolX,
		XNegativeSymbolSpaceCurrencySymbol,
		BracketCurrencySymbolSpaceXBracket,
		BracketXSpaceCurrencySymbolBracket,
		MaxValue
	};
}

// Used by the globalisation system to format negative currency values based on the ECurrencyNegativePattern::Type value for the culture.
class FNegativeCurrencyOutputFormatting
{
public:
	FNegativeCurrencyOutputFormatting()
	{
	}

	FNegativeCurrencyOutputFormatting(const ECurrencyNegativePattern::Type CNPValue, const FString& CurrencySymbol, const FString& NegativeSymbol)
	{
		check (0 <= CNPValue && CNPValue < ECurrencyNegativePattern::MaxValue);
		switch (CNPValue) 
		{
			case ECurrencyNegativePattern::BracketCurrencySymbolXBracket:
				PreNumberFormatting = FString(TEXT("("))+CurrencySymbol;
				PostNumberFormatting = FString(TEXT(")"));
				break;
			case ECurrencyNegativePattern::NegativeSymbolCurrencySymbolX:
				PreNumberFormatting = NegativeSymbol + CurrencySymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case ECurrencyNegativePattern::CurrencySymbolNegativeSymbolX:
				PreNumberFormatting = CurrencySymbol + NegativeSymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case ECurrencyNegativePattern::CurrencySymbolXNegativeSymbol:
				PreNumberFormatting = CurrencySymbol;
				PostNumberFormatting = NegativeSymbol;
				break;
			case ECurrencyNegativePattern::BracketXCurrencySymbolBracket:
				PreNumberFormatting = FString(TEXT("("));
				PostNumberFormatting = CurrencySymbol + FString(TEXT(")"));
				break;
			case ECurrencyNegativePattern::NegativeSymbolXCurrencySymbol:
				PreNumberFormatting = NegativeSymbol;
				PostNumberFormatting = CurrencySymbol;
				break;
			case ECurrencyNegativePattern::XNegativeSymbolCurrencySymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = NegativeSymbol + CurrencySymbol;
				break;
			case ECurrencyNegativePattern::XCurrencySymbolNegativeSymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = CurrencySymbol + NegativeSymbol;
				break;
			case ECurrencyNegativePattern::NegativeSymbolXSpaceCurrencySymbol:
				PreNumberFormatting = NegativeSymbol;
				PostNumberFormatting = FString(TEXT("\x00A0")) + CurrencySymbol;
				break;
			case ECurrencyNegativePattern::NegativeSymbolCurrencySymbolSpaceX:
				PreNumberFormatting = NegativeSymbol + CurrencySymbol + FString(TEXT("\x00A0"));
				PostNumberFormatting = FString(TEXT(""));
				break;
			case ECurrencyNegativePattern::XSpaceCurrencySymbolNegativeSymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = FString(TEXT("\x00A0")) + CurrencySymbol + NegativeSymbol;
				break;
			case ECurrencyNegativePattern::CurrencySymbolSpaceXNegativeSymbol:
				PreNumberFormatting = CurrencySymbol + FString(TEXT("\x00A0"));
				PostNumberFormatting = NegativeSymbol;
				break;
			case ECurrencyNegativePattern::CurrencySymbolSpaceNegativeSymbolX:
				PreNumberFormatting = CurrencySymbol + FString(TEXT("\x00A0")) + NegativeSymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case ECurrencyNegativePattern::XNegativeSymbolSpaceCurrencySymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = NegativeSymbol + FString(TEXT("\x00A0")) + CurrencySymbol;
				break;
			case ECurrencyNegativePattern::BracketCurrencySymbolSpaceXBracket:
				PreNumberFormatting = FString(TEXT("(")) + CurrencySymbol + FString(TEXT("\x00A0"));
				PostNumberFormatting = FString(TEXT(")"));
				break;
			case ECurrencyNegativePattern::BracketXSpaceCurrencySymbolBracket:
				PreNumberFormatting = FString(TEXT("("));
				PostNumberFormatting = FString(TEXT("\x00A0")) + CurrencySymbol + FString(TEXT(")"));
				break;
			default:
				// will have failed the check prior to this default.
				break;
		}
	}

	// Return a negatively formatted culturally specific currency string from the string InValue;
	FString FormatString(FString InValue)const 
	{
		return PreNumberFormatting + InValue + PostNumberFormatting;
	}

protected:
	FString PreNumberFormatting;
	FString PostNumberFormatting;
};