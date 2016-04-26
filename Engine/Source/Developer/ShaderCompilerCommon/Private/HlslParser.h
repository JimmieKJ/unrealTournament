// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslParser.h - Interface for parsing hlsl.
=============================================================================*/

#pragma once

#include "HlslLexer.h"

namespace CrossCompiler
{
	struct FLinearAllocator;
	namespace AST
	{
		class FNode;
	}

	enum class EParseResult
	{
		Matched,
		NotMatched,
		Error,
	};

	namespace Parser
	{
		typedef void TCallback(void* CallbackData, CrossCompiler::FLinearAllocator* Allocator, CrossCompiler::TLinearArray<AST::FNode*>& ASTNodes);

		// Returns true if successfully parsed
		bool Parse(const FString& Input, const FString& Filename, FCompilerMessages& OutCompilerMessages, TCallback* Callback = nullptr, void* CallbackData = nullptr);
	}
}
