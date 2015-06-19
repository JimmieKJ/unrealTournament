// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "PropertyBinding.h"

#define LOCTEXT_NAMESPACE "UMG"

DEFINE_STAT(STAT_UMGBinding);

UPropertyBinding::UPropertyBinding()
{
}

bool UPropertyBinding::IsSupportedSource(UProperty* Property) const
{
	return false;
}

bool UPropertyBinding::IsSupportedDestination(UProperty* Property) const
{
	return false;
}

void UPropertyBinding::Bind(UProperty* Property, FScriptDelegate* Delegate)
{
	static const FName BinderFunction(TEXT("GetValue"));
	Delegate->BindUFunction(this, BinderFunction);
}

#undef LOCTEXT_NAMESPACE
