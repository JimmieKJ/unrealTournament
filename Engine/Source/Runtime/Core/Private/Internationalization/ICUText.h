// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICUUtilities.h"
#include "ICUCulture.h"
#include "TextData.h"
#include "TextHistory.h"
#include "FastDecimalFormat.h"
#include "unicode/utypes.h"
#include <unicode/numfmt.h>
#include "unicode/fmtable.h"
#include "unicode/unistr.h"

#define EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT 1

template<typename T1, typename T2>
FText FText::AsNumberTemplate(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();

	FString NativeString;

#if EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT
	const FDecimalNumberFormattingRules& FormattingRules = Culture.Implementation->GetDecimalNumberFormattingRules();
	const FNumberFormattingOptions& FormattingOptions = (Options) ? *Options : FormattingRules.CultureDefaultFormattingOptions;
	NativeString = FastDecimalFormat::NumberToString(Val, FormattingRules, FormattingOptions);
#else // EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT
	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DecimalFormat, ESPMode::ThreadSafe>& ICUDecimalFormat( Culture.Implementation->GetDecimalFormatter(Options) );
	icu::Formattable FormattableVal(static_cast<T2>(Val));
	icu::UnicodeString FormattedString;
	ICUDecimalFormat->format(FormattableVal, FormattedString, ICUStatus);

	ICUUtilities::ConvertString(FormattedString, NativeString);
#endif // EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT

	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsNumber>(MoveTemp(NativeString), FTextHistory_AsNumber(Val, Options, TargetCulture))));
}

template<typename T1, typename T2>
FText FText::AsCurrencyTemplate(T1 Val, const FString& CurrencyCode, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();

	FString NativeString;

#if EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT
	const FDecimalNumberFormattingRules& FormattingRules = Culture.Implementation->GetCurrencyFormattingRules(CurrencyCode);
	const FNumberFormattingOptions& FormattingOptions = (Options) ? *Options : FormattingRules.CultureDefaultFormattingOptions;
	NativeString = FastDecimalFormat::NumberToString(Val, FormattingRules, FormattingOptions);
#else // EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT
	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DecimalFormat>& ICUDecimalFormat(Culture.Implementation->GetCurrencyFormatter(CurrencyCode, Options));
	icu::Formattable FormattableVal(static_cast<T2>(Val));
	icu::UnicodeString FormattedString;
	ICUDecimalFormat->format(FormattableVal, FormattedString, ICUStatus);

	ICUUtilities::ConvertString(FormattedString, NativeString);
#endif // EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT

	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsCurrency>(MoveTemp(NativeString), FTextHistory_AsCurrency(Val, CurrencyCode, Options, TargetCulture))));
}

template<typename T1, typename T2>
FText FText::AsPercentTemplate(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();

	FString NativeString;

#if EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT
	const FDecimalNumberFormattingRules& FormattingRules = Culture.Implementation->GetPercentFormattingRules();
	const FNumberFormattingOptions& FormattingOptions = (Options) ? *Options : FormattingRules.CultureDefaultFormattingOptions;
	NativeString = FastDecimalFormat::NumberToString(Val * static_cast<T1>(100), FormattingRules, FormattingOptions);
#else // EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT
	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DecimalFormat>& ICUDecimalFormat( Culture.Implementation->GetPercentFormatter(Options) );
	icu::Formattable FormattableVal(static_cast<T2>(Val));
	icu::UnicodeString FormattedString;
	ICUDecimalFormat->format(FormattableVal, FormattedString, ICUStatus);

	ICUUtilities::ConvertString(FormattedString, NativeString);
#endif // EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT

	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsPercent>(MoveTemp(NativeString), FTextHistory_AsPercent(Val, Options, TargetCulture))));
}

#undef EXPERIMENTAL_TEXT_FAST_DECIMAL_FORMAT
