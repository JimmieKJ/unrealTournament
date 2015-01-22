// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

	FString GetDisplayName() const;

	FString GetEnglishName() const;

	int GetKeyboardLayoutId() const;

	int GetLCID() const;

	TArray<FString> GetPrioritizedParentCultureNames();

	static FString GetCanonicalName(const FString& Name);

	FString GetName() const;
	
	FString GetNativeName() const;

	FString GetUnrealLegacyThreeLetterISOLanguageName() const;

	FString GetThreeLetterISOLanguageName() const;

	FString GetTwoLetterISOLanguageName() const;

	FString GetNativeLanguage() const;

	FString GetRegion() const;

	FString GetNativeRegion() const;

	FString GetScript() const;

	FString GetVariant() const;

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
};

#include "CulturePointer.h"