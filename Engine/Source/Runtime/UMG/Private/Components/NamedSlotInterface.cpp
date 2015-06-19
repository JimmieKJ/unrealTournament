// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

UNamedSlotInterface::UNamedSlotInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool INamedSlotInterface::ContainsContent(UWidget* Content) const
{
	TArray<FName> SlotNames;
	GetSlotNames(SlotNames);

	for ( const FName& SlotName : SlotNames )
	{
		if ( GetContentForSlot(SlotName) == Content )
		{
			return true;
		}
	}

	return false;
}
