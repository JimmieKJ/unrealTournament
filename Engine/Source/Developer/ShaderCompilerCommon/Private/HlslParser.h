// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslParser.h - Interface for parsing hlsl.
=============================================================================*/

#pragma once

#include "HlslLexer.h"

namespace CrossCompiler
{
	enum class EParseResult
	{
		Matched,
		NotMatched,
		Error,
	};

	namespace Parser
	{
		// Returns true if successfully parsed
		bool Parse(const FString& Input, const FString& Filename, bool bDump);
	}
}
