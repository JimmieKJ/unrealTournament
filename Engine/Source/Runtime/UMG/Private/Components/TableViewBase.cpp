// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Components/TableViewBase.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UTableViewBase

UTableViewBase::UTableViewBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
