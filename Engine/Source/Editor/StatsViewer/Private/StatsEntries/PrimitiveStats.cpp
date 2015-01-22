// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StatsViewerPrivatePCH.h"
#include "PrimitiveStats.h"

UPrimitiveStats::UPrimitiveStats(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPrimitiveStats::UpdateNames()
{
	if( Object.IsValid() )
	{
		Type = Object.Get()->GetClass()->GetName();
	}
}