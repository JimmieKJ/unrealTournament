// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "LinkerPlaceholderFunction.h"

//------------------------------------------------------------------------------
ULinkerPlaceholderFunction::ULinkerPlaceholderFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
IMPLEMENT_CORE_INTRINSIC_CLASS(ULinkerPlaceholderFunction, UFunction,
	{
	}
);
