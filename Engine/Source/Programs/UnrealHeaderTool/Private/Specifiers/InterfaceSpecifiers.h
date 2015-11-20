// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EInterfaceSpecifier
{
	None = -1

	#define INTERFACE_SPECIFIER(SpecifierName) , SpecifierName
		#include "InterfaceSpecifiers.def"
	#undef INTERFACE_SPECIFIER

	, Max
};

extern const TCHAR* GInterfaceSpecifierStrings[(int32)EInterfaceSpecifier::Max];
