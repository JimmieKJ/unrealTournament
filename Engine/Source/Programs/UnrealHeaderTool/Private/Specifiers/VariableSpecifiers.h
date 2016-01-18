// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EVariableSpecifier
{
	None = -1

	#define VARIABLE_SPECIFIER(SpecifierName) , SpecifierName
		#include "VariableSpecifiers.def"
	#undef VARIABLE_SPECIFIER

	, Max
};

extern const TCHAR* GVariableSpecifierStrings[(int32)EVariableSpecifier::Max];
