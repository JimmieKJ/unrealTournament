// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UseClassWithNonStaticPtrField.h"

static void Function_UseClassWithNonStaticPtrField()
{
#ifdef UNREAL_CODE_ANALYZER
	FTestClassWithNonStaticPtrField TestClassWithNonStaticPtrField;
#endif
}
