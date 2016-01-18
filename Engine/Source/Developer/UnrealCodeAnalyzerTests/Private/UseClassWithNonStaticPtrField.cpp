// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "UseClassWithNonStaticPtrField.h"

static void Function_UseClassWithNonStaticPtrField()
{
#ifdef UNREAL_CODE_ANALYZER
	FTestClassWithNonStaticPtrField TestClassWithNonStaticPtrField;
#endif
}
