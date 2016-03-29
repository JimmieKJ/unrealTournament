// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextData.h"

template<typename T1, typename T2>
FText FText::AsNumberTemplate(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();
	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsNumber>(Culture.NumberFormattingRule.AsNumber(Val), FTextHistory_AsNumber(Val, Options, TargetCulture))));
}

template<typename T1, typename T2>
FText FText::AsCurrencyTemplate(T1 Val, const FString& CurrencyCode, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();
	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsCurrency>(Culture.NumberFormattingRule.AsCurrency(Val), FTextHistory_AsCurrency(Val, CurrencyCode, Options, TargetCulture))));
}

template<typename T1, typename T2>
FText FText::AsPercentTemplate(T1 Val, const FNumberFormattingOptions* const Options, const FCulturePtr& TargetCulture)
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentCulture();
	return FText::CreateNumericalText(MakeShareable(new TGeneratedTextData<FTextHistory_AsPercent>(Culture.NumberFormattingRule.AsPercent(Val), FTextHistory_AsPercent(Val, Options, TargetCulture))));
}