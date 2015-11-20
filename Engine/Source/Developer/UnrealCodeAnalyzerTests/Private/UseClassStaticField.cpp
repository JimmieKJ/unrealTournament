// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "UseClassStaticField.h"

static void Function_UseClassStaticField()
{
	FTestClassWithStaticField::StaticField = 10;
}
