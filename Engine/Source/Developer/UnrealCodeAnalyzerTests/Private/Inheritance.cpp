// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "Inheritance.h"

class FTestDerivedClass : public FTestBaseClass
{ };

static void Function_Inheritance()
{
#ifdef UNREAL_CODE_ANALYZER
	FTestDerivedClass TestDerivedClass;
#endif
}
