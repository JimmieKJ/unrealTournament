// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if UE_ENABLE_ICU
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(push)
	#pragma warning(disable:28251)
	#pragma warning(disable:28252)
	#pragma warning(disable:28253)
#endif
	#include <unicode/regex.h>
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(pop)
#endif
#include "Regex.h"
#include "ICUUtilities.h"

namespace
{
	TSharedRef<icu::RegexPattern> CreateRegexPattern(const FString& SourceString)
	{
		icu::UnicodeString ICUSourceString;
		ICUUtilities::ConvertString(SourceString, ICUSourceString);

		UErrorCode ICUStatus = U_ZERO_ERROR;
		return MakeShareable( icu::RegexPattern::compile(ICUSourceString, 0, ICUStatus) );
	}
}

class FRegexPatternImplementation
{
public:
	FRegexPatternImplementation(const FString& SourceString) : ICURegexPattern( CreateRegexPattern(SourceString) ) 
	{
	}

public:
	TSharedRef<icu::RegexPattern> ICURegexPattern;
};

FRegexPattern::FRegexPattern(const FString& SourceString) : Implementation(new FRegexPatternImplementation(SourceString))
{
}

namespace
{
	TSharedRef<icu::RegexMatcher> CreateRegexMatcher(const FRegexPatternImplementation& Pattern, const icu::UnicodeString& InputString)
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		return MakeShareable( Pattern.ICURegexPattern->matcher(InputString, ICUStatus) );
	}
}

class FRegexMatcherImplementation
{
public:
	FRegexMatcherImplementation(const FRegexPatternImplementation& Pattern, const FString& InputString) : ICUInputString(ICUUtilities::ConvertString(InputString)), ICURegexMatcher(CreateRegexMatcher(Pattern, ICUInputString)), OriginalString(InputString)
	{
	}

public:
	const icu::UnicodeString ICUInputString;
	TSharedRef<icu::RegexMatcher> ICURegexMatcher;
	FString OriginalString;
};

FRegexMatcher::FRegexMatcher(const FRegexPattern& Pattern, const FString& InputString) : Implementation(new FRegexMatcherImplementation(Pattern.Implementation.Get(), InputString))
{
}	

bool FRegexMatcher::FindNext()
{
	return Implementation->ICURegexMatcher->find() != 0;
}

int32 FRegexMatcher::GetMatchBeginning()
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher->start(ICUStatus);
}

int32 FRegexMatcher::GetMatchEnding()
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher->end(ICUStatus);
}

int32 FRegexMatcher::GetCaptureGroupBeginning(const int32 Index)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher->start(Index, ICUStatus);
}

int32 FRegexMatcher::GetCaptureGroupEnding(const int32 Index)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher->end(Index, ICUStatus);
}

FString FRegexMatcher::GetCaptureGroup(const int32 Index)
{
	return Implementation->OriginalString.Mid(GetCaptureGroupBeginning(Index), GetCaptureGroupEnding(Index) - GetCaptureGroupBeginning(Index));
}

int32 FRegexMatcher::GetBeginLimit()
{
	return Implementation->ICURegexMatcher->regionStart();
}

int32 FRegexMatcher::GetEndLimit()
{
	return Implementation->ICURegexMatcher->regionEnd();
}

void FRegexMatcher::SetLimits(const int32 BeginIndex, const int32 EndIndex)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	Implementation->ICURegexMatcher->region(BeginIndex, EndIndex, ICUStatus);
}
#endif