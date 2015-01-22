// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !UE_ENABLE_ICU
#include "Internationalization/Text.h"

class FCulture::FLegacyCultureImplementation
{
	friend FCulture;

	FLegacyCultureImplementation(	const FText& InDisplayName, 
									const FString& InEnglishName, 
									const int InKeyboardLayoutId, 
									const int InLCID, 
									const FString& InName, 
									const FString& InNativeName, 
									const FString& InUnrealLegacyThreeLetterISOLanguageName, 
									const FString& InThreeLetterISOLanguageName, 
									const FString& InTwoLetterISOLanguageName);

	FString GetDisplayName() const;

	FString GetEnglishName() const;

	int GetKeyboardLayoutId() const;

	int GetLCID() const;

	static FString GetCanonicalName(const FString& Name);

	FString GetName() const;

	FString GetNativeName() const;

	FString GetUnrealLegacyThreeLetterISOLanguageName() const;

	FString GetThreeLetterISOLanguageName() const;

	FString GetTwoLetterISOLanguageName() const;

	FString GetNativeLanguage() const;

	FString GetNativeRegion() const;

	// Full localized culture name
	const FText DisplayName;

	// The English name of the culture in format languagefull [country/regionfull]
	const FString EnglishName;

	// Keyboard input locale id
	const int KeyboardLayoutId;

	// id for this Culture
	const int LCID;

	// Name of the culture in languagecode2-country/regioncode2 format
	const FString Name;

	// The culture name, consisting of the language, the country/region, and the optional script
	const FString NativeName;

	// ISO 639-2 three letter code of the language - for the purpose of supporting legacy Unreal documentation.
	const FString UnrealLegacyThreeLetterISOLanguageName;

	// ISO 639-2 three letter code of the language
	const FString ThreeLetterISOLanguageName;

	// ISO 639-1 two letter code of the language
	const FString TwoLetterISOLanguageName;
};
#endif