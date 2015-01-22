// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealString.h"

class FTextFormattingRules
{

public:

	FTextFormattingRules(const bool InIsRightToLeft, const FString InListSeparator, const int InANSICodePage, const int InEBCDICCodePage, const int InMacCodePage, const int InOEMCodePage)
		: IsRightToLeft(InIsRightToLeft)
		, ListSeparator(InListSeparator)
		, ANSICodePage(InANSICodePage)
		, EBCDICCodePage(InEBCDICCodePage)
		, MacCodePage(InMacCodePage)
		, OEMCodePage(InOEMCodePage) 
	{ 
	}

	FTextFormattingRules()
		: IsRightToLeft(false)
		, ListSeparator(FString(TEXT("")))
		, ANSICodePage(0)
		, EBCDICCodePage(0)
		, MacCodePage(0)
		, OEMCodePage(0) 
	{ 
	}

	// Does this TextFormattingRule represent a right to left text flow
	const bool IsRightToLeft;

	// Character used to separate elements in a list
	const FString ListSeparator;

	// ANSI code page used by writing system of this TextFormattingRule
	const int ANSICodePage;

	// EBCDIC code page used by writing system of this TextFormattingRule
	const int EBCDICCodePage;

	// The Macintosh code page
	const int MacCodePage;

	// The OEM code page
	const int OEMCodePage;

};

