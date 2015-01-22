// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealString.h"

namespace ECurrencyPositivePattern
{
	/** 
	 * enum value associated with a pattern used to display positive currency values
	 * Below X represents the number being converted to display as a currency, $ will be replaced
	 * with the cultural values CurrencySymbol
	 *
	 * |-----|----------------------|
	 * | $n	 | CurrencySymbolX      |
	 * | n$	 | XCurrencySymbol      |
     * | $ n | CurrencySymbolSpaceX |
	 * | n $ | XSpaceCurrencySymbol |
	 * |-----|----------------------|
	 */
	enum Type
	{
		CurrencySymbolX,
		XCurrencySymbol,
		CurrencySymbolSpaceX,
		XSpaceCurrencySymbol,
		MaxValue
	};
}	

// Used by the globalisation system to format positive currency values based on the ECurrencyPositivePattern::Type value for the culture.
class FPositiveCurrencyOutputFormatting
{
public:
	FPositiveCurrencyOutputFormatting()
	{
	}

	FPositiveCurrencyOutputFormatting(const ECurrencyPositivePattern::Type CPPValue, const FString& CurrencySymbol)
	{
		check(0 <=CPPValue && CPPValue < ECurrencyPositivePattern::MaxValue);
		switch (CPPValue) 
		{
			case ECurrencyPositivePattern::CurrencySymbolX:
				PreNumberFormatting = CurrencySymbol;
				PostNumberFormatting = FString(TEXT(""));
				break;
			case ECurrencyPositivePattern::XCurrencySymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = CurrencySymbol;
				break;
			case ECurrencyPositivePattern::CurrencySymbolSpaceX:
				PreNumberFormatting = CurrencySymbol + FString(TEXT("\x00A0"));
				PostNumberFormatting = FString(TEXT(""));
				break;
			case ECurrencyPositivePattern::XSpaceCurrencySymbol:
				PreNumberFormatting = FString(TEXT(""));
				PostNumberFormatting = FString(TEXT("\x00A0")) + CurrencySymbol;
				break;
			default:
				// will have failed the check prior to this default.
				break;
		}
	}


	// Return a positively formatted culturally specific currency string from the string InValue;
	FString FormatString(FString InValue)const 
	{
		return PreNumberFormatting + InValue + PostNumberFormatting;
	}

protected:
	FString PreNumberFormatting;
	FString PostNumberFormatting;
};