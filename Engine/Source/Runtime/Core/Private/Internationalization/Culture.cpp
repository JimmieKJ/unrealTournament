// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU
#include "ICUCulture.h"
#else
#include "LegacyCulture.h"
#endif

#if UE_ENABLE_ICU
FCulturePtr FCulture::Create(const FString& LocaleName)
{
	return MakeShareable(new FCulture(LocaleName));
}
#else
FCulturePtr FCulture::Create(const FText& InDisplayName, const FString& InEnglishName, const int InKeyboardLayoutId, const int InLCID, const FString& InName, const FString& InNativeName, const FString& InUnrealLegacyThreeLetterISOLanguageName, const FString& InThreeLetterISOLanguageName, const FString& InTwoLetterISOLanguageName, const FNumberFormattingRules& InNumberFormattingRule, const FTextFormattingRules& InTextFormattingRule, const FDateTimeFormattingRules& InDateTimeFormattingRule)
{
	return MakeShareable(new FCulture(InDisplayName, InEnglishName, InKeyboardLayoutId, InLCID, InName, InNativeName, InUnrealLegacyThreeLetterISOLanguageName, InThreeLetterISOLanguageName, InTwoLetterISOLanguageName, InNumberFormattingRule, InTextFormattingRule, InDateTimeFormattingRule));
}
#endif

#if UE_ENABLE_ICU
FCulture::FCulture(const FString& LocaleName)
	: Implementation( new FICUCultureImplementation( LocaleName ) )
{

}
#else
FCulture::FCulture(const FText& InDisplayName, const FString& InEnglishName, const int InKeyboardLayoutId, const int InLCID, const FString& InName, const FString& InNativeName, const FString& InUnrealLegacyThreeLetterISOLanguageName, const FString& InThreeLetterISOLanguageName, const FString& InTwoLetterISOLanguageName, const FNumberFormattingRules& InNumberFormattingRule, const FTextFormattingRules& InTextFormattingRule, const FDateTimeFormattingRules& InDateTimeFormattingRule) 
	: Implementation( new FLegacyCultureImplementation(InDisplayName, InEnglishName, InKeyboardLayoutId, InLCID, InName, InNativeName, InUnrealLegacyThreeLetterISOLanguageName, InThreeLetterISOLanguageName, InTwoLetterISOLanguageName) )
	, DateTimeFormattingRule(InDateTimeFormattingRule)
	, TextFormattingRule(InTextFormattingRule)
	, NumberFormattingRule(InNumberFormattingRule)
{ 
}
#endif

FString FCulture::GetDisplayName() const
{
	return Implementation->GetDisplayName();
}

FString FCulture::GetEnglishName() const
{
	return Implementation->GetEnglishName();
}

int FCulture::GetKeyboardLayoutId() const
{
	return Implementation->GetKeyboardLayoutId();
}

int FCulture::GetLCID() const
{
	return Implementation->GetLCID();
}

TArray<FString> FCulture::GetPrioritizedParentCultureNames()
{
	const FString LanguageCode = GetTwoLetterISOLanguageName();
	const FString ScriptCode = GetScript();
	const FString RegionCode = GetRegion();

	TArray<FString> LocaleTagCombinations;
	if (!ScriptCode.IsEmpty() && !RegionCode.IsEmpty())
	{
		LocaleTagCombinations.Add(LanguageCode + TEXT("_") + ScriptCode + TEXT("_") + RegionCode);
	}
	if (!RegionCode.IsEmpty())
	{
		LocaleTagCombinations.Add(LanguageCode + TEXT("_") + RegionCode);
	}
	if (!ScriptCode.IsEmpty())
	{
		LocaleTagCombinations.Add(LanguageCode + TEXT("_") + ScriptCode);
	}
	LocaleTagCombinations.Add(LanguageCode);

	return LocaleTagCombinations;
}

FString FCulture::GetCanonicalName(const FString& Name)
{
	return FImplementation::GetCanonicalName(Name);
}

FString FCulture::GetName() const
{
	return Implementation->GetName();
}

FString FCulture::GetNativeName() const
{
	return Implementation->GetNativeName();
}

FString FCulture::GetUnrealLegacyThreeLetterISOLanguageName() const
{
	return Implementation->GetUnrealLegacyThreeLetterISOLanguageName();
}

FString FCulture::GetThreeLetterISOLanguageName() const
{
	return Implementation->GetThreeLetterISOLanguageName();
}

FString FCulture::GetTwoLetterISOLanguageName() const
{
	return Implementation->GetTwoLetterISOLanguageName();
}

FString FCulture::GetNativeLanguage() const
{
	return Implementation->GetNativeLanguage();
}

FString FCulture::GetRegion() const
{
#if UE_ENABLE_ICU
	return Implementation->GetRegion();
#else
	return TEXT("");
#endif
}

FString FCulture::GetNativeRegion() const
{
	return Implementation->GetNativeRegion();
}

FString FCulture::GetScript() const
{
#if UE_ENABLE_ICU
	return Implementation->GetScript();
#else
	return TEXT("");
#endif
}

FString FCulture::GetVariant() const
{
#if UE_ENABLE_ICU
	return Implementation->GetVariant();
#else
	return TEXT("");
#endif
}
