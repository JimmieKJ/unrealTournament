// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if !UE_ENABLE_ICU
#include "LegacyCulture.h"

FCulture::FLegacyCultureImplementation::FLegacyCultureImplementation(const FText& InDisplayName, const FString& InEnglishName, const int InKeyboardLayoutId, const int InLCID, const FString& InName, const FString& InNativeName, const FString& InUnrealLegacyThreeLetterISOLanguageName, const FString& InThreeLetterISOLanguageName, const FString& InTwoLetterISOLanguageName)
	: DisplayName(InDisplayName)
	, EnglishName(InEnglishName)
	, KeyboardLayoutId(InKeyboardLayoutId)
	, LCID(InLCID)
	, Name( InName )
	, NativeName(InNativeName)
	, UnrealLegacyThreeLetterISOLanguageName( InUnrealLegacyThreeLetterISOLanguageName )
	, ThreeLetterISOLanguageName( InThreeLetterISOLanguageName )
	, TwoLetterISOLanguageName( InTwoLetterISOLanguageName )
{ 
}

FString FCulture::FLegacyCultureImplementation::GetDisplayName() const
{
	return DisplayName.ToString();
}

FString FCulture::FLegacyCultureImplementation::GetEnglishName() const
{
	return EnglishName;
}

int FCulture::FLegacyCultureImplementation::GetKeyboardLayoutId() const
{
	return KeyboardLayoutId;
}

int FCulture::FLegacyCultureImplementation::GetLCID() const
{
	return LCID;
}

FString FCulture::FLegacyCultureImplementation::GetName() const
{
	return Name;
}

FString FCulture::FLegacyCultureImplementation::GetCanonicalName(const FString& Name)
{
	return Name;
}

FString FCulture::FLegacyCultureImplementation::GetNativeName() const
{
	return NativeName;
}

FString FCulture::FLegacyCultureImplementation::GetNativeLanguage() const
{
	int32 LastBracket = INDEX_NONE;
	int32 FirstBracket = INDEX_NONE;
	if ( NativeName.FindLastChar( ')', LastBracket ) && NativeName.FindChar( '(', FirstBracket ) && LastBracket != FirstBracket )
	{
		return NativeName.Left( FirstBracket-1 );
	}
	return NativeName;
}

FString FCulture::FLegacyCultureImplementation::GetNativeRegion() const
{
	int32 LastBracket = INDEX_NONE;
	int32 FirstBracket = INDEX_NONE;
	if ( NativeName.FindLastChar( ')', LastBracket ) && NativeName.FindChar( '(', FirstBracket ) && LastBracket != FirstBracket )
	{
		return NativeName.Mid( FirstBracket+1, LastBracket-FirstBracket-1 );
	}
	return NativeName;
}

FString FCulture::FLegacyCultureImplementation::GetUnrealLegacyThreeLetterISOLanguageName() const
{
	return UnrealLegacyThreeLetterISOLanguageName;
}

FString FCulture::FLegacyCultureImplementation::GetThreeLetterISOLanguageName() const
{
	return ThreeLetterISOLanguageName;
}

FString FCulture::FLegacyCultureImplementation::GetTwoLetterISOLanguageName() const
{
	return TwoLetterISOLanguageName;
}
#endif