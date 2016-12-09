// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EStructSpecifier
{
	None = -1

	#define STRUCT_SPECIFIER(SpecifierName) , SpecifierName
		#include "StructSpecifiers.def"
	#undef STRUCT_SPECIFIER

	, Max
};

extern const TCHAR* GStructSpecifierStrings[(int32)EStructSpecifier::Max];
