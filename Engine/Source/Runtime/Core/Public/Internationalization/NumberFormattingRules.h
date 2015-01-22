// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealString.h"
#include "NegativePercentOutputFormatting.h"
#include "PositivePercentOutputFormatting.h"
#include "NegativeCurrencyOutputFormatting.h"
#include "PositiveCurrencyOutputFormatting.h"
#include "NegativeNumberOutputFormatting.h"

class FNumberFormattingRules
{

public:
	FNumberFormattingRules()
		: NumberNegativePattern(ENumberNegativePattern::MaxValue)
		, PercentNegativePattern(EPercentNegativePattern::MaxValue)
		, PercentPositivePattern(EPercentPositivePattern::MaxValue)
		, CurrencyNegativePattern(ECurrencyNegativePattern::MaxValue)
		, CurrencyPositivePattern(ECurrencyPositivePattern::MaxValue)
		, CurrencyDecimalDigits(0)
		, CurrencyDecimalSeparator(FString(TEXT("")))
		, CurrencyGroupSeparator(FString(TEXT("")))
		, CurrencySymbol(FString(TEXT("")))
		, NaNSymbol(FString(TEXT("")))
		, NegativeSign(FString(TEXT("")))
		, NumberDecimalDigits(0)
		, NumberDecimalSeparator(FString(TEXT("")))
		, NumberGroupSeparator(FString(TEXT("")))
		, PercentDecimalDigits(0)
		, PercentDecimalSeparator(FString(TEXT("")))
		, PercentGroupSeparator(FString(TEXT("")))
		, PercentSymbol(FString(TEXT("")))
		, PerMilleSymbol(FString(TEXT("")))
		, PositiveSign(FString(TEXT("")))
	{
	}

	FNumberFormattingRules(const int InCurrencyDecimalDigits,
		const FString& InCurrencyDecimalSeparator, 
		const FString& InCurrencyGroupSeparator, 
		const TArray<int>& InCurrencyGroupSizes, 
		const FString& InCurrencySymbol, 
		const FString& InNaNSymbol, 
		const TArray<FString>& InNativeDigits, 
		const FString& InNegativeSign, 
		const int InNumberDecimalDigits, 
		const FString& InNumberDecimalSeparator, 
		const FString& InNumberGroupSeparator, 
		const TArray<int>& InNumberGroupSizes, 
		const int InPercentDecimalDigits, 
		const FString& InPercentDecimalSeparator, 
		const FString& InPercentGroupSeparator, 
		const TArray<int>& InPercentGroupSizes, 
		const FString& InPercentSymbol, 
		const FString& InPerMilleSymbol, 
		const FString& InPositiveSign, 
		const ECurrencyNegativePattern::Type InCurrencyNegativePattern, 
		const ECurrencyPositivePattern::Type InCurrencyPositivePattern, 
		const EPercentNegativePattern::Type InPercentNegativePattern, 
		const EPercentPositivePattern::Type InPercentPositivePattern, 
		const ENumberNegativePattern::Type InNumberNegativePattern)
		: NumberNegativePattern(InNumberNegativePattern)
		, PercentNegativePattern(InPercentNegativePattern)
		, PercentPositivePattern(InPercentPositivePattern)
		, CurrencyNegativePattern(InCurrencyNegativePattern)
		, CurrencyPositivePattern(InCurrencyPositivePattern)
		, CurrencyDecimalDigits(InCurrencyDecimalDigits)
		, CurrencyDecimalSeparator(InCurrencyDecimalSeparator)
		, CurrencyGroupSeparator(InCurrencyGroupSeparator)
		, CurrencyGroupSizes(InCurrencyGroupSizes)
		, CurrencySymbol(InCurrencySymbol)
		, NaNSymbol(InNaNSymbol)
		, NativeDigits(InNativeDigits)
		, NegativeSign(InNegativeSign)
		, NumberDecimalDigits(InNumberDecimalDigits)
		, NumberDecimalSeparator(InNumberDecimalSeparator)
		, NumberGroupSeparator(InNumberGroupSeparator)
		, NumberGroupSizes(InNumberGroupSizes)
		, PercentDecimalDigits(InPercentDecimalDigits)
		, PercentDecimalSeparator(InPercentDecimalSeparator)
		, PercentGroupSeparator(InPercentGroupSeparator)
		, PercentGroupSizes(InPercentGroupSizes)
		, PercentSymbol(InPercentSymbol)
		, PerMilleSymbol(InPerMilleSymbol)
		, PositiveSign(InPositiveSign)
	{
		CreateOutputFormatting();
	}


protected:

	//Negative pattern to use for formatting numbers
	const ENumberNegativePattern::Type NumberNegativePattern;

	//Negative pattern to use for formatting percents
	const EPercentNegativePattern::Type PercentNegativePattern;

	//Positive pattern to use for formatting percents
	const EPercentPositivePattern::Type PercentPositivePattern;

	//Negative pattern to use for formatting currency values
	const ECurrencyNegativePattern::Type CurrencyNegativePattern;

	//Positive pattern to use for formatting currency values
	const ECurrencyPositivePattern::Type CurrencyPositivePattern;

	// How many decimal digits to display for currency
	const int CurrencyDecimalDigits;

	// Symbol to use to separate the decimal part of a currency
	const FString CurrencyDecimalSeparator;

	// Symbol to use to separate the whole number part into periods of digits for currency
	const FString CurrencyGroupSeparator;

	// Number of digits belonging to periods of currency.

	/**
	 * Array to hold the number of digits belonging to groups of a currency
	 * From CurrencyGroupSizes[0]..CurrencyGroupSizes[x] the number of digits are grouped for right to left after the NumberDecimalSeparator
	 * If the last element of the array is 0 then the remainder of the digits are not grouped
	 * otherwise the last element of the array is used as the size to group any remaining elements
	 * Examples
	 * English (UK) {3} => 123,456,789.00
	 * Hindi {3,2} => 12,34,56,789.00
	 * {3,0} => 123456,789.00
	 */
	const TArray<int> CurrencyGroupSizes;
	
	// Symbol to use to for the currency
	const FString CurrencySymbol;

	// A value to indicate that the currency was not valid
	const FString NaNSymbol;

	// The digits which may belong to a number
	const TArray<FString> NativeDigits;

	// The symbol to use to indicate the value is negative
	const FString NegativeSign;

	// How many decimal digits to display for a number
	const int NumberDecimalDigits;

	// Symbol to use to separate the decimal part of a number
	const FString NumberDecimalSeparator;

	// Symbol to use to separate the whole number part into periods of digits for numbers
	const FString NumberGroupSeparator;
	
	/**
	 * Array to hold the number of digits belonging to groups of a number
	 * From NumberGroupSizes[0]..NumberGroupSizes[x] the number of digits are grouped for right to left after the NumberDecimalSeparator
	 * If the last element of the array is 0 then the remainder of the digits are not grouped
	 * otherwise the last element of the array is used as the size to group any remaining elements
	 * Examples
	 * English (UK) {3} => 123,456,789.00
	 * Hindi {3,2} => 12,34,56,789.00
	 * {3,0} => 123456,789.00
	 */	
	const TArray<int> NumberGroupSizes;

	// How many decimal digits to display for a percent
	const int PercentDecimalDigits;

	// Symbol to use to separate the decimal part of a percent
	const FString PercentDecimalSeparator;

	// Symbol to use to separate the whole number part into periods of digits for percents
	const FString PercentGroupSeparator;

	/**
	 * Array to hold the number of digits belonging to groups of a percent
	 * From PercentGroupSizes[0]..PercentGroupSizes[x] the number of digits are grouped for right to left after the NumberDecimalSeparator
	 * If the last element of the array is 0 then the remainder of the digits are not grouped
	 * otherwise the last element of the array is used as the size to group any remaining elements
	 * Examples
	 * English (UK) {3} => 123,456,789.00
	 * Hindi {3,2} => 12,34,56,789.00
	 * {3,0} => 123456,789.00
	 */
	const TArray<int> PercentGroupSizes;

	// Symbol to use for the percent
	const FString PercentSymbol;

	// Symbol to use for per mille values
	const FString PerMilleSymbol;

	// Symbol to use for positive values
	const FString PositiveSign;
	
	/**
	 * Convert a double number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (double InNumber)const
	{
		return AsPositiveOrNegativeNumberFormatting(FString::Printf(TEXT("%.*f"), NumberDecimalDigits, InNumber));
	}

	/**
	 * Convert a float number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (float InNumber)const
	{
		return AsPositiveOrNegativeNumberFormatting(FString::Printf(TEXT("%.*f"), NumberDecimalDigits, InNumber));
	}

	/**
	 * Convert a long number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (long InNumber)const
	{
		return AsPositiveOrNegativeIntegralNumberFormatting(FString::Printf(TEXT("%d"),InNumber));
	}

	/**
	 * Convert an uint8 number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (uint8 InNumber)const
	{
		return AsNumberUnsignedIntegralPart(FString::Printf(TEXT("%d"),InNumber), NumberGroupSeparator, NumberGroupSizes);
	}

	/**
	 * Convert an uint16 number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (uint16 InNumber)const
	{
		return AsNumberUnsignedIntegralPart(FString::Printf(TEXT("%d"),InNumber), NumberGroupSeparator, NumberGroupSizes);
	}

	/**
	 * Convert an uint32 number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (uint32 InNumber)const
	{
		return AsNumberUnsignedIntegralPart(FString::Printf(TEXT("%u"),InNumber), NumberGroupSeparator, NumberGroupSizes);
	}

	/**
	 * Convert an uint64 number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (uint64 InNumber)const
	{
		return AsNumberUnsignedIntegralPart(FString::Printf(TEXT("%llu"),InNumber), NumberGroupSeparator, NumberGroupSizes);
	}

	/**
	 * Convert an int8 number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (int8 InNumber)const
	{	
		FString InNumberString;
		return AsPositiveOrNegativeIntegralNumberFormatting(FString::Printf(TEXT("%d"),InNumber));
	}

	/**
	 * Convert an int16 number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (int16 InNumber)const
	{
		return AsPositiveOrNegativeIntegralNumberFormatting(FString::Printf(TEXT("%d"),InNumber));
	}

	/**
	 * Convert an int32 number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (int32 InNumber)const
	{
		return AsPositiveOrNegativeIntegralNumberFormatting(FString::Printf(TEXT("%d"),InNumber));
	}

	/**
	 * Convert an int64 number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsNumber (int64 InNumber)const
	{
		return AsPositiveOrNegativeIntegralNumberFormatting(FString::Printf(TEXT("%lld"),InNumber));
	}

	/**
	 * Convert a double number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (double InNumber)const
	{
		return AsPositiveOrNegativeCurrencyFormatting(FString::Printf(TEXT("%.*f"), CurrencyDecimalDigits, InNumber));
	}

	/**
	 * Convert a float number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (float InNumber)const
	{
		return AsPositiveOrNegativeCurrencyFormatting(FString::Printf(TEXT("%.*f"), CurrencyDecimalDigits, InNumber));
	}

	/**
	 * Convert a long number to a formatted string with this cultures number syntax
	 * @param InNumber - number to convert
	 * @return String of number formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (long InNumber)const
	{
		return AsPositiveOrNegativeIntegralCurrencyFormatting(FString::Printf(TEXT("%ld"),InNumber));
	}

	/**
	 * Convert an uint8 number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (uint8 InNumber)const
	{
		const FString WholePart = FString::Printf(TEXT("%d"),InNumber);
		const FString FractionalPart = FString::Printf(TEXT("%.*u"), CurrencyDecimalDigits, 0);
		const bool bHasFractionalPart = FractionalPart.Len() > 0;

		return PositiveCurrencyFormatting.FormatString(AsNumberUnsignedIntegralPart(WholePart, CurrencyGroupSeparator, CurrencyGroupSizes) + (bHasFractionalPart ? AsNumberFractionalPart(FractionalPart, CurrencyDecimalSeparator, CurrencyDecimalDigits) : TEXT("")));
	}

	/**
	 * Convert an uint16 number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (uint16 InNumber)const
	{
		const FString WholePart = FString::Printf(TEXT("%d"),InNumber);
		const FString FractionalPart = FString::Printf(TEXT("%.*u"), CurrencyDecimalDigits, 0);
		const bool bHasFractionalPart = FractionalPart.Len() > 0;

		return PositiveCurrencyFormatting.FormatString(AsNumberUnsignedIntegralPart(WholePart, CurrencyGroupSeparator, CurrencyGroupSizes) + (bHasFractionalPart ? AsNumberFractionalPart(FractionalPart, CurrencyDecimalSeparator, CurrencyDecimalDigits) : TEXT("")));
	}

	/**
	 * Convert an uint32 number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (uint32 InNumber)const
	{
		const FString WholePart = FString::Printf(TEXT("%u"),InNumber);
		const FString FractionalPart = FString::Printf(TEXT("%.*u"), CurrencyDecimalDigits, 0);
		const bool bHasFractionalPart = FractionalPart.Len() > 0;

		return PositiveCurrencyFormatting.FormatString(AsNumberUnsignedIntegralPart(WholePart, CurrencyGroupSeparator, CurrencyGroupSizes) + (bHasFractionalPart ? AsNumberFractionalPart(FractionalPart, CurrencyDecimalSeparator, CurrencyDecimalDigits) : TEXT("")));
	}

	/**
	 * Convert an uint64 number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (uint64 InNumber)const
	{
		const FString WholePart = FString::Printf(TEXT("%llu"),InNumber);
		const FString FractionalPart = FString::Printf(TEXT("%.*u"), CurrencyDecimalDigits, 0);
		const bool bHasFractionalPart = FractionalPart.Len() > 0;

		return PositiveCurrencyFormatting.FormatString(AsNumberUnsignedIntegralPart(WholePart, CurrencyGroupSeparator, CurrencyGroupSizes) + (bHasFractionalPart ? AsNumberFractionalPart(FractionalPart, CurrencyDecimalSeparator, CurrencyDecimalDigits) : TEXT("")));
	}

	/**
	 * Convert an int8 number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (int8 InNumber)const
	{
		return AsPositiveOrNegativeIntegralCurrencyFormatting(FString::Printf(TEXT("%d"),InNumber));
	}

	/**
	 * Convert an int16 number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (int16 InNumber)const
	{
		return AsPositiveOrNegativeIntegralCurrencyFormatting(FString::Printf(TEXT("%d"),InNumber));
	}

	/**
	 * Convert an int32 number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (int32 InNumber)const
	{
		return AsPositiveOrNegativeIntegralCurrencyFormatting(FString::Printf(TEXT("%d"),InNumber));
	}

	/**
	 * Convert an int64 number to a formatted string with this cultures currency syntax
	 * @param InNumber - number to convert
	 * @return String of currency formatted given the syntax in this rule for this culture
	 */
	inline FString AsCurrency (int64 InNumber)const
	{
		return AsPositiveOrNegativeIntegralCurrencyFormatting(FString::Printf(TEXT("%lld"),InNumber));
	}

	/**
	 * Convert a double number to a formatted string with this cultures Percent syntax
	 * @param InNumber - number to convert
	 * @return String of Percent formatted given the syntax in this rule for this culture
	 */
	inline FString AsPercent (double InNumber)const
	{
		return AsPositiveOrNegativePercentFormatting(FString::Printf(TEXT("%.*f"), PercentDecimalDigits, InNumber * 100));
	}

	/**
	 * Convert a float number to a formatted string with this cultures Percent syntax
	 * @param InNumber - number to convert
	 * @return String of Percent formatted given the syntax in this rule for this culture
	 */
	inline FString AsPercent (float InNumber)const
	{
		return AsPositiveOrNegativePercentFormatting(FString::Printf(TEXT("%.*f"), PercentDecimalDigits, InNumber * 100));
	}

private:
	friend class FText;

	//Class to use for formatting negative numbers
	FNegativeNumberOutputFormatting NegativeNumberFormatting;

	//Class to use for formatting negative percents
	FNegativePercentOutputFormatting NegativePercentFormatting;

	//Class to use for formatting positive percents
	FPositivePercentOutputFormatting PositivePercentFormatting;

	//Class to use for formatting negative currency values
	FNegativeCurrencyOutputFormatting NegativeCurrencyFormatting;

	//Class to use for formatting positive currency values
	FPositiveCurrencyOutputFormatting PositiveCurrencyFormatting;

	// Set up the formatting to be applied to out strings required to meet the requirements of the culture
	void CreateOutputFormatting()
	{
		NegativeNumberFormatting = FNegativeNumberOutputFormatting(NumberNegativePattern,NegativeSign);

		NegativeCurrencyFormatting = FNegativeCurrencyOutputFormatting(CurrencyNegativePattern,CurrencySymbol,NegativeSign);
		PositiveCurrencyFormatting = FPositiveCurrencyOutputFormatting(CurrencyPositivePattern,CurrencySymbol);

		NegativePercentFormatting = FNegativePercentOutputFormatting(PercentNegativePattern,PercentSymbol,NegativeSign);
		PositivePercentFormatting = FPositivePercentOutputFormatting(PercentPositivePattern,PercentSymbol);
	}

	/**
	 * Taking a positive or negative number string value convert to culture specific number value
	 *
	 * @param InNumberAsString - number to convert
	 * @return Positive or Negative cultural relevant number as string
	 */
	inline FString AsPositiveOrNegativeNumberFormatting (const FString& InNumberAsString)const 
	{
		if (InNumberAsString.StartsWith(TEXT("-")))
		{
			return NegativeNumberFormatting.FormatString(AsNumber(InNumberAsString.RightChop(1), NumberDecimalSeparator, NumberGroupSeparator, NumberGroupSizes, NumberDecimalDigits));
		}
		else
		{
			return AsNumber(InNumberAsString, NumberDecimalSeparator, NumberGroupSeparator, NumberGroupSizes, NumberDecimalDigits);
		}
	}

	/**
	 * Taking a positive or negative integral number string value convert to culture specific number value
	 *
	 * @param InNumberAsString - number to convert
	 * @return Positive or Negative cultural relevant number as string
	 */
	inline FString AsPositiveOrNegativeIntegralNumberFormatting (const FString& InNumberAsString)const 
	{
		if (InNumberAsString.StartsWith(TEXT("-")))
		{
			return NegativeNumberFormatting.FormatString(AsNumberUnsignedIntegralPart(InNumberAsString.RightChop(1), NumberGroupSeparator, NumberGroupSizes));
		}
		else
		{
			return AsNumberUnsignedIntegralPart(InNumberAsString, NumberGroupSeparator, NumberGroupSizes);
		}
	}

	/**
	 * Taking a positive or negative currency string value convert to culture specific percentage value
	 *
	 * @param InNumberAsString - number to convert
	 * @return Positive or Negative cultural relevant Percent as string
	 */
	inline FString AsPositiveOrNegativeCurrencyFormatting (const FString& InNumberAsString)const 
	{
		if (InNumberAsString.StartsWith(TEXT("-")))
		{
			return NegativeCurrencyFormatting.FormatString(AsNumber(InNumberAsString.RightChop(1), CurrencyDecimalSeparator, CurrencyGroupSeparator, CurrencyGroupSizes, CurrencyDecimalDigits));
		}
		else
		{
			return PositiveCurrencyFormatting.FormatString(AsNumber(InNumberAsString, CurrencyDecimalSeparator, CurrencyGroupSeparator, CurrencyGroupSizes, CurrencyDecimalDigits));
		}
	}

	/**
	 * Taking a positive or negative integral currency string value convert to culture specific currency value
	 *
	 * @param InNumberAsString - number to convert
	 * @return Positive or Negative cultural relevant Percent as string
	 */
	inline FString AsPositiveOrNegativeIntegralCurrencyFormatting (const FString& InNumberAsString)const 
	{
		const FString& WholePart = InNumberAsString;
		const FString FractionalPart = FString::Printf(TEXT("%.*u"), CurrencyDecimalDigits, 0);

		const bool bHasFractionalPart = FractionalPart.Len() > 0;

		if (InNumberAsString.StartsWith(TEXT("-")))
		{
			return NegativeCurrencyFormatting.FormatString(AsNumberUnsignedIntegralPart(WholePart.RightChop(1), CurrencyGroupSeparator, CurrencyGroupSizes) + (bHasFractionalPart ? AsNumberFractionalPart(FractionalPart, CurrencyDecimalSeparator, CurrencyDecimalDigits) : TEXT("")));
		}
		else
		{
			return PositiveCurrencyFormatting.FormatString(AsNumberUnsignedIntegralPart(WholePart, CurrencyGroupSeparator, CurrencyGroupSizes) + (bHasFractionalPart ? AsNumberFractionalPart(FractionalPart, CurrencyDecimalSeparator, CurrencyDecimalDigits) : TEXT("")));
		}
	}


	/**
	 * Taking a positive or negative Percent string value convert to culture specific percentage value
	 *
	 * @param InNumberAsString - number to convert
	 * @return Positive or Negative cultural relevant Percent as string
	 */
	inline FString AsPositiveOrNegativePercentFormatting (const FString& InNumberAsString)const 
	{
		if (InNumberAsString.StartsWith(TEXT("-")))
		{
			return NegativePercentFormatting.FormatString(AsNumber(InNumberAsString.RightChop(1), PercentDecimalSeparator, PercentGroupSeparator, PercentGroupSizes, PercentDecimalDigits));
		}
		else
		{
			return PositivePercentFormatting.FormatString(AsNumber(InNumberAsString, PercentDecimalSeparator, PercentGroupSeparator, PercentGroupSizes, PercentDecimalDigits));
		}
	}

	/**
	 * Format a positive integral part of a string number using this rules definition
	 * 
	 * @param IntegralPartOfNumber - integral string number to be converted, expected to have been tested to ensure this is a number prior to this function
	 * @param GroupSeparator - symbol to use for the group separator
	 * @param GroupSizes - sizes of the groups
	 * @return FString after conversion using this rule
	 */
	inline FString AsNumberUnsignedIntegralPart (FString IntegralPartOfNumber, const FString& GroupSeparator, const TArray<int>& GroupSizes)const 
	{
		FString OutString;

		//Store last array group size to be used for rest of string after each array size has been taken into consideration, -1 indicates no value
		int LastArrayGroupSize=-1;

		// for each of the groupsizes specified construct the OutString for the whole part.
		for (auto GroupSizeItr = GroupSizes.CreateConstIterator(); GroupSizeItr; ++GroupSizeItr)
		{
			if (OutString.Len() == 0)
			{
				OutString = IntegralPartOfNumber.Right(*GroupSizeItr);
			}
			else
			{
				OutString = IntegralPartOfNumber.Right(*GroupSizeItr) + GroupSeparator + OutString;
			}

			IntegralPartOfNumber = IntegralPartOfNumber.LeftChop(*GroupSizeItr);

			if (IntegralPartOfNumber.Len()>0)
			{
				LastArrayGroupSize = *GroupSizeItr; // store the last value for use outside this loop for processing the rest of the string
			}
			else
			{
				LastArrayGroupSize=-1; // no need to work on the string outside this for loop.
				break;
			}
		}

		if (LastArrayGroupSize>0)
		{
			//for positive non-negative value use the last value to cut up the remainder of the WholePart
			while (IntegralPartOfNumber.Len() >0)
			{
				OutString = IntegralPartOfNumber.Right(LastArrayGroupSize) + GroupSeparator + OutString;
				IntegralPartOfNumber = IntegralPartOfNumber.LeftChop(LastArrayGroupSize);
			}
		}
		else if (LastArrayGroupSize==0)
		{
			//if 0 then rest of whole number is just concatenated onto output
			OutString = IntegralPartOfNumber + GroupSeparator + OutString;
		}
		// do nothing for <0 it is an indication there was no wholepart left to subdivide

		return OutString;
	}
	
	/**
	 * Format the Fractional part of a string number using this rules definition
	 * 
	 * @param FractionalPartOfNumber - fractional string number to be converted, expected to have been tested to ensure this is a number prior to this function
	 * @param DecimalSeparator - symbol to use for the decimal separator
	 * @param DecimalDigits - number of digits after the decimal point
	 * @return FString after conversion using this rule or empty string if no decimal part given
	 */
	inline FString AsNumberFractionalPart (const FString& FractionalPartOfNumber, const FString& DecimalSeparator, const int DecimalDigits)const 
	{
		if (FractionalPartOfNumber.Len()>0)
		{
			//@todo this does not round the number
			return DecimalSeparator + FractionalPartOfNumber.Left(DecimalDigits);
		}
		else
		{
			return FString();
		}
	}

	/**
	 * Convert a string number which may be negative and have a fractional part to a string formatted using this rule 
	 * 
	 * @param InNumber - string number to be converted
	 * @param DecimalSeparator - symbol to use for the decimal separator
	 * @param GroupSeparator - symbol to use for the group separator
	 * @param GroupSizes - sizes of the groups
	 * @param DecimalDigits - number of digits after the decimal point
	 * @return FString after conversion using this rule
	 */
	inline FString AsNumber (const FString& InNumber, const FString& DecimalSeparator, const FString& GroupSeparator, const TArray<int>& GroupSizes, const int DecimalDigits)const 
	{
		FString WholePart;
		FString FractionalPart;
		if( !( InNumber.Split(TEXT("."),&WholePart, &FractionalPart, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ) )
		{
			WholePart = InNumber;
		}

		if (FractionalPart.Len() > 0)
			return AsNumberUnsignedIntegralPart(WholePart, GroupSeparator, GroupSizes) + AsNumberFractionalPart(FractionalPart, DecimalSeparator, DecimalDigits);
		else
		{
			return AsNumberUnsignedIntegralPart(WholePart, GroupSeparator, GroupSizes);
		}
	}
};