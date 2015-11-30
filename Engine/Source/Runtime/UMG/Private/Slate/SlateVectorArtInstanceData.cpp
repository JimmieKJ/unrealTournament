// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "SlateVectorArtInstanceData.h"

void FSlateVectorArtInstanceData::SetPosition(FVector2D Position)
{
	GetData().X = Position.X;
	GetData().Y = Position.Y;
}

void FSlateVectorArtInstanceData::SetScale(float Scale)
{
	GetData().Z = Scale;
}

void FSlateVectorArtInstanceData::SetBaseAddress(float Address)
{
	GetData().W = Address;
}
