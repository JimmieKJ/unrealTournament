// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "UseElifMacro.h"

static int Function_UseElifMacro()
{
#if TEST_IF_MACRO
	return 0;
#elif ELIF_MACRO
	return 1;
#else
	return 2;
#endif
}
