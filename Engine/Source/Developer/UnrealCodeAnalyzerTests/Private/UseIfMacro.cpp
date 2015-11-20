// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "UseIfMacro.h"

static int Function_UseIfMacro()
{
#if IF_MACRO
	return 0;
#else
	return 1;
#endif
}
