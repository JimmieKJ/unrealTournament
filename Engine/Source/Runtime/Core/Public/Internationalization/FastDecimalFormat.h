// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Rules used to format a decimal number */
struct FDecimalNumberFormattingRules
{
	FDecimalNumberFormattingRules()
		: GroupingSeparatorCharacter(0)
		, DecimalSeparatorCharacter(0)
		, PrimaryGroupingSize(0)
		, SecondaryGroupingSize(0)
	{
	}

	/** Number formatting rules, typically extracted from the ICU decimal formatter for a given culture */
	FString NaNString;
	FString NegativePrefixString;
	FString NegativeSuffixString;
	FString PositivePrefixString;
	FString PositiveSuffixString;
	TCHAR GroupingSeparatorCharacter;
	TCHAR DecimalSeparatorCharacter;
	uint8 PrimaryGroupingSize;
	uint8 SecondaryGroupingSize;

	/** Default number formatting options for a given culture */
	FNumberFormattingOptions CultureDefaultFormattingOptions;
};

/**
 * Provides efficient and culture aware number formatting.
 * You would call FastDecimalFormat::NumberToString to convert a number to the correct decimal representation based on the given formatting rules and options.
 * The primary consumer of this is FText, however you can use it for other things. GetCultureAgnosticFormattingRules can provide formatting rules for cases where you don't care about culture.
 */
namespace FastDecimalFormat
{

namespace Internal
{

CORE_API FString IntegralToString(const bool bIsNegative, const uint64 InVal, const FDecimalNumberFormattingRules& InFormattingRules, FNumberFormattingOptions InFormattingOptions);
CORE_API FString FractionalToString(const double InVal, const FDecimalNumberFormattingRules& InFormattingRules, FNumberFormattingOptions InFormattingOptions);

} // namespace Internal

#define FAST_DECIMAL_FORMAT_SIGNED_IMPL(NUMBER_TYPE)																															\
	FORCEINLINE FString NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions)	\
	{																																											\
		const bool bIsNegative = InVal < 0;																																		\
		return Internal::IntegralToString(bIsNegative, static_cast<uint64>((bIsNegative) ? -InVal : InVal), InFormattingRules, InFormattingOptions);							\
	}

#define FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(NUMBER_TYPE)																															\
	FORCEINLINE FString NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions)	\
	{																																											\
		return Internal::IntegralToString(false, static_cast<uint64>(InVal), InFormattingRules, InFormattingOptions);															\
	}

#define FAST_DECIMAL_FORMAT_FRACTIONAL_IMPL(NUMBER_TYPE)																														\
	FORCEINLINE FString NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions)	\
	{																																											\
		return Internal::FractionalToString(static_cast<double>(InVal), InFormattingRules, InFormattingOptions);																\
	}

FAST_DECIMAL_FORMAT_SIGNED_IMPL(int8)
FAST_DECIMAL_FORMAT_SIGNED_IMPL(int16)
FAST_DECIMAL_FORMAT_SIGNED_IMPL(int32)
FAST_DECIMAL_FORMAT_SIGNED_IMPL(int64)

FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(uint8)
FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(uint16)
FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(uint32)
FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(uint64)

FAST_DECIMAL_FORMAT_FRACTIONAL_IMPL(float)
FAST_DECIMAL_FORMAT_FRACTIONAL_IMPL(double)

#undef FAST_DECIMAL_FORMAT_SIGNED_IMPL
#undef FAST_DECIMAL_FORMAT_UNSIGNED_IMPL
#undef FAST_DECIMAL_FORMAT_FRACTIONAL_IMPL

/**
 * Get the formatting rules to use when you don't care about culture.
 */
CORE_API FDecimalNumberFormattingRules GetCultureAgnosticFormattingRules();

} // namespace FastDecimalFormat
