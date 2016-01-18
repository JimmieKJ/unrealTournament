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
#else
FCulture::FCulture(const FText& InDisplayName, const FString& InEnglishName, const int InKeyboardLayoutId, const int InLCID, const FString& InName, const FString& InNativeName, const FString& InUnrealLegacyThreeLetterISOLanguageName, const FString& InThreeLetterISOLanguageName, const FString& InTwoLetterISOLanguageName, const FNumberFormattingRules& InNumberFormattingRule, const FTextFormattingRules& InTextFormattingRule, const FDateTimeFormattingRules& InDateTimeFormattingRule) 
	: Implementation( new FLegacyCultureImplementation(InDisplayName, InEnglishName, InKeyboardLayoutId, InLCID, InName, InNativeName, InUnrealLegacyThreeLetterISOLanguageName, InThreeLetterISOLanguageName, InTwoLetterISOLanguageName) )
	, DateTimeFormattingRule(InDateTimeFormattingRule)
	, TextFormattingRule(InTextFormattingRule)
	, NumberFormattingRule(InNumberFormattingRule)
#endif
	, CachedDisplayName(Implementation->GetDisplayName())
	, CachedEnglishName(Implementation->GetEnglishName())
	, CachedName(Implementation->GetName())
	, CachedNativeName(Implementation->GetNativeName())
	, CachedUnrealLegacyThreeLetterISOLanguageName(Implementation->GetUnrealLegacyThreeLetterISOLanguageName())
	, CachedThreeLetterISOLanguageName(Implementation->GetThreeLetterISOLanguageName())
	, CachedTwoLetterISOLanguageName(Implementation->GetTwoLetterISOLanguageName())
	, CachedNativeLanguage(Implementation->GetNativeLanguage())
#if UE_ENABLE_ICU
	, CachedRegion(Implementation->GetRegion())
#endif
	, CachedNativeRegion(Implementation->GetNativeRegion())
#if UE_ENABLE_ICU
	, CachedScript(Implementation->GetScript())
	, CachedVariant(Implementation->GetVariant())
#endif
{ 
}

const FString& FCulture::GetDisplayName() const
{
	return CachedDisplayName;
}

const FString& FCulture::GetEnglishName() const
{
	return CachedEnglishName;
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
		LocaleTagCombinations.Add(LanguageCode + TEXT("-") + ScriptCode + TEXT("-") + RegionCode);
	}
	if (!RegionCode.IsEmpty())
	{
		LocaleTagCombinations.Add(LanguageCode + TEXT("-") + RegionCode);
	}
	if (!ScriptCode.IsEmpty())
	{
		LocaleTagCombinations.Add(LanguageCode + TEXT("-") + ScriptCode);
	}
	LocaleTagCombinations.Add(LanguageCode);

	return LocaleTagCombinations;
}

FString FCulture::GetCanonicalName(const FString& Name)
{
	return FImplementation::GetCanonicalName(Name);
}

const FString& FCulture::GetName() const
{
	return CachedName;
}

const FString& FCulture::GetNativeName() const
{
	return CachedNativeName;
}

const FString& FCulture::GetUnrealLegacyThreeLetterISOLanguageName() const
{
	return CachedUnrealLegacyThreeLetterISOLanguageName;
}

const FString& FCulture::GetThreeLetterISOLanguageName() const
{
	return CachedThreeLetterISOLanguageName;
}

const FString& FCulture::GetTwoLetterISOLanguageName() const
{
	return CachedTwoLetterISOLanguageName;
}

const FString& FCulture::GetNativeLanguage() const
{
	return CachedNativeLanguage;
}

const FString& FCulture::GetRegion() const
{
	return CachedRegion;
}

const FString& FCulture::GetNativeRegion() const
{
	return CachedNativeRegion;
}

const FString& FCulture::GetScript() const
{
	return CachedScript;
}

const FString& FCulture::GetVariant() const
{
	return CachedVariant;
}
