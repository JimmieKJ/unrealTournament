// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "UseClassWithNonStaticField.h"

static void Function_UseClassWithNonStaticField()
{
#ifdef UNREAL_CODE_ANALYZER
	FTestClassWithNonStaticField TestClassWithNonStaticField;
#endif
}
