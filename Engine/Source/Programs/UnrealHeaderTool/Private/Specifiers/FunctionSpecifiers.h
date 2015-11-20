// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EFunctionSpecifier
{
	None = -1

	#define FUNCTION_SPECIFIER(SpecifierName) , SpecifierName
		#include "FunctionSpecifiers.def"
	#undef FUNCTION_SPECIFIER

	, Max
};

extern const TCHAR* GFunctionSpecifierStrings[(int32)EFunctionSpecifier::Max];
