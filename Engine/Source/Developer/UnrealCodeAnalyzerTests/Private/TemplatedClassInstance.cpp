// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "TemplatedClassInstance.h"

static void Function_TemplatedClassInstance()
{
#ifdef UNREAL_CODE_ANALYZER
	FTemplateClass<FTestClass> TemplateClass;
#endif
}
