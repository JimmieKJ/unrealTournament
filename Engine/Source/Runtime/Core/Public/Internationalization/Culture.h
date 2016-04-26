// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"

#if !UE_ENABLE_ICU
#include "NumberFormattingRules.h"
#include "TextFormattingRules.h"
#include "DateTimeFormattingRules.h"
#endif

class CORE_API FCulture
{
public:
#if UE_ENABLE_ICU
	static FCulturePtr Create(const FString& LocaleName);
#else
	static FCulturePtr Create(	const FText& InDisplayName, 
									const FString& InEnglishName, 
									const int InKeyboardLayoutId, 
									const int InLCID, 
									const FString& InName, 
									const FString& InNativeName, 
									const FString& InUnrealLegacyThreeLetterISOLanguageName, 
									const FString& InThreeLetterISOLanguageName, 
									const FString& InTwoLetterISOLanguageName, 
									const FNumberFormattingRules& InNumberFormattingRule, 
									const FTextFormattingRules& InTextFormattingRule, 
									const FDateTimeFormattingRules& InDateTimeFormattingRule);
#endif

	const FString& GetDisplayName() const;

	const FString& GetEnglishName() const;

	int GetKeyboardLayoutId() const;

	int GetLCID() const;

	TArray<FString> GetPrioritizedParentCultureNames() const;

	static TArray<FString> GetPrioritizedParentCultureNames(const FString& LanguageCode, const FString& ScriptCode, const FString& RegionCode);

	static FString GetCanonicalName(const FString& Name);

	const FString& GetName() const;
	
	const FString& GetNativeName() const;

	const FString& GetUnrealLegacyThreeLetterISOLanguageName() const;

	const FString& GetThreeLetterISOLanguageName() const;

	const FString& GetTwoLetterISOLanguageName() const;

	const FString& GetNativeLanguage() const;

	const FString& GetRegion() const;

	const FString& GetNativeRegion() const;

	const FString& GetScript() const;

	const FString& GetVariant() const;

	void HandleCultureChanged();

public:
#if UE_ENABLE_ICU
	class FICUCultureImplementation;
	typedef FICUCultureImplementation FImplementation;
	TSharedRef<FICUCultureImplementation> Implementation;
#else
	class FLegacyCultureImplementation;
	typedef FLegacyCultureImplementation FImplementation;
	TSharedRef<FLegacyCultureImplementation> Implementation;
#endif

protected:
#if UE_ENABLE_ICU
	FCulture(const FString& LocaleName);
#else
	FCulture(	const FText& InDisplayName, 
				const FString& InEnglishName, 
				const int InKeyboardLayoutId, 
				const int InLCID, 
				const FString& InName, 
				const FString& InNativeName, 
				const FString& InUnrealLegacyThreeLetterISOLanguageName, 
				const FString& InThreeLetterISOLanguageName, 
				const FString& InTwoLetterISOLanguageName, 
				const FNumberFormattingRules& InNumberFormattingRule, 
				const FTextFormattingRules& InTextFormattingRule, 
				const FDateTimeFormattingRules& InDateTimeFormattingRule);
#endif

private:
#if !UE_ENABLE_ICU
	friend class FText;
	const FDateTimeFormattingRules DateTimeFormattingRule;
	const FTextFormattingRules TextFormattingRule;
	const FNumberFormattingRules NumberFormattingRule;
#endif

	FString CachedDisplayName;
	FString CachedEnglishName;
	FString CachedName;
	FString CachedNativeName;
	FString CachedUnrealLegacyThreeLetterISOLanguageName;
	FString CachedThreeLetterISOLanguageName;
	FString CachedTwoLetterISOLanguageName;
	FString CachedNativeLanguage;
	FString CachedRegion;
	FString CachedNativeRegion;
	FString CachedScript;
	FString CachedVariant;
};

#include "CulturePointer.h"