// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "UseClass.h"

static void Function_UseClass()
{
#ifdef UNREAL_CODE_ANALYZER
	FTestClass TestClass;
#endif
}
