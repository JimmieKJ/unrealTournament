// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_ICU
#include "unicode/unistr.h"

namespace ICUUtilities
{
	/** Convert FString <-> icu::UnicodeString */
	void ConvertString(const FString& Source, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
	icu::UnicodeString ConvertString(const FString& Source, const bool ShouldNullTerminate = true);
	void ConvertString(const icu::UnicodeString& Source, FString& Destination);
	FString ConvertString(const icu::UnicodeString& Source);
}
#endif