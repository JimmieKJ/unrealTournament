// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#include "../PhysicsEngine/PhysXSupport.h"
#include "PhysicsFiltering.h"

//////////////////////////////////////////////////////////////////////////
// FPhysicsFilterBuilder

FPhysicsFilterBuilder::FPhysicsFilterBuilder(TEnumAsByte<enum ECollisionChannel> InObjectType, const struct FCollisionResponseContainer& ResponseToChannels)
	: BlockingBits(0)
	, TouchingBits(0)
	, Word3(0)
{
	for (int32 i = 0; i < ARRAY_COUNT(ResponseToChannels.EnumArray); i++)
	{
		if (ResponseToChannels.EnumArray[i] == ECR_Block)
		{
			const uint32 ChannelBit = CRC_TO_BITFIELD(i);
			BlockingBits |= ChannelBit;
		}
		else if (ResponseToChannels.EnumArray[i] == ECR_Overlap)
		{
			const uint32 ChannelBit = CRC_TO_BITFIELD(i);
			TouchingBits |= ChannelBit;
		}
	}

	uint8 MyChannel = InObjectType;

	Word3 = (MyChannel << 24);
}
