// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICUUtilities.h"
#include "ICUCulture.h"
#include "TextHistory.h"
#include "unicode/utypes.h"
#include <unicode/numfmt.h>
#include "unicode/fmtable.h"
#include "unicode/unistr.h"

template<typename T1, typename T2>
FText FText::AsNumberTemplate(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCultureRef Culture = TargetCulture.IsValid() ? TargetCulture.ToSharedRef() : I18N.GetCurrentCulture();
	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DecimalFormat, ESPMode::ThreadSafe> ICUDecimalFormat( Culture->Implementation->GetDecimalFormatter(Options) );
	icu::Formattable FormattableVal(static_cast<T2>(Val));
	icu::UnicodeString FormattedString;
	ICUDecimalFormat->format(FormattableVal, FormattedString, ICUStatus);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ReturnText = FText::CreateNumericalText(MoveTemp(NativeString));
	ReturnText.History = MakeShareable(new FTextHistory_AsNumber(Val, Options, TargetCulture));
	return ReturnText;
}

template<typename T1, typename T2>
FText FText::AsCurrencyTemplate(T1 Val, const FString& CurrencyCode, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCultureRef Culture = TargetCulture.IsValid() ? TargetCulture.ToSharedRef() : I18N.GetCurrentCulture();
	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DecimalFormat> ICUDecimalFormat(Culture->Implementation->GetCurrencyFormatter(CurrencyCode, Options));
	icu::Formattable FormattableVal(static_cast<T2>(Val));
	icu::UnicodeString FormattedString;
	ICUDecimalFormat->format(FormattableVal, FormattedString, ICUStatus);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ReturnText = FText::CreateNumericalText(MoveTemp(NativeString));
	ReturnText.History = MakeShareable(new FTextHistory_AsCurrency(Val, CurrencyCode, Options, TargetCulture));
	return ReturnText;
}

template<typename T1, typename T2>
FText FText::AsPercentTemplate(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCultureRef Culture = TargetCulture.IsValid() ? TargetCulture.ToSharedRef() : I18N.GetCurrentCulture();
	UErrorCode ICUStatus = U_ZERO_ERROR;
	const TSharedRef<const icu::DecimalFormat> ICUDecimalFormat( Culture->Implementation->GetPercentFormatter(Options) );
	icu::Formattable FormattableVal(static_cast<T2>(Val));
	icu::UnicodeString FormattedString;
	ICUDecimalFormat->format(FormattableVal, FormattedString, ICUStatus);

	FString NativeString;
	ICUUtilities::ConvertString(FormattedString, NativeString);

	FText ReturnText = FText::CreateNumericalText(MoveTemp(NativeString));
	ReturnText.History = MakeShareable(new FTextHistory_AsPercent(Val, Options, TargetCulture));
	return ReturnText;
}
