// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "BreakIterator.h"
#include "CamelCaseBreakIterator.h"

#if UE_ENABLE_ICU
#include "ICUTextCharacterIterator.h"
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(push)
	#pragma warning(disable:28251)
	#pragma warning(disable:28252)
	#pragma warning(disable:28253)
#endif
	#include <unicode/uchar.h>
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(pop)
#endif

class FICUCamelCaseBreakIterator : public FCamelCaseBreakIterator
{
protected:
	virtual void TokenizeString(TArray<FToken>& OutTokens) override;
};

void FICUCamelCaseBreakIterator::TokenizeString(TArray<FToken>& OutTokens)
{
	OutTokens.Empty(String.Len());

	FICUTextCharacterIterator CharIter(String);
	for(CharIter.setToStart(); CharIter.current32() != FICUTextCharacterIterator::DONE; CharIter.next32PostInc())
	{
		const UChar32 CurrentChar = CharIter.current32();

		ETokenType TokenType = ETokenType::Other;
		if(u_isULowercase(CurrentChar))
		{
			TokenType = ETokenType::Lowercase;
		}
		else if(u_isUUppercase(CurrentChar))
		{
			TokenType = ETokenType::Uppercase;
		}
		else if(u_isdigit(CurrentChar))
		{
			TokenType = ETokenType::Digit;
		}

		OutTokens.Emplace(FToken(TokenType, CharIter.getIndex()));
	}

	OutTokens.Emplace(FToken(ETokenType::Null, String.Len()));

	// There should always be at least one token for the end of the string
	check(OutTokens.Num());
}

TSharedRef<IBreakIterator> FBreakIterator::CreateCamelCaseBreakIterator()
{
	return MakeShareable(new FICUCamelCaseBreakIterator());
}

#endif
