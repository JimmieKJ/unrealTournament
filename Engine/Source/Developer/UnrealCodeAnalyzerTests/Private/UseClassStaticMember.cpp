// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "UseClassStaticMember.h"

static void Function_UseClassStaticMember()
{
	FTestClassWithStaticMember::StaticMember();
}
