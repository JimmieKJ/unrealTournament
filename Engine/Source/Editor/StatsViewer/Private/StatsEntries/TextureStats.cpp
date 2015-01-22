// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StatsViewerPrivatePCH.h"
#include "TextureStats.h"

UTextureStats::UTextureStats(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	LastTimeRendered( FLT_MAX )
{
}
