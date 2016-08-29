// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * Set of flags stored in the PhysX FilterData
 *
 * When this flag is saved in CreateShapeFilterData or CreateQueryFilterData, we only use 23 bits
 * If you plan to use more than 23 bits, you'll also need to change the format of ShapeFilterData,QueryFilterData
 * Make sure you also change preFilter/SimFilterShader where it's used
 */
enum EPhysXFilterDataFlags
{

	EPDF_SimpleCollision	=	0x0001,
	EPDF_ComplexCollision	=	0x0002,
	EPDF_CCD				=	0x0004,
	EPDF_ContactNotify		=	0x0008,
	EPDF_StaticShape		=	0x0010,
	EPDF_ModifyContacts		=   0x0020
};

struct FPhysicsFilterBuilder
{
	ENGINE_API FPhysicsFilterBuilder(TEnumAsByte<enum ECollisionChannel> InObjectType, FMaskFilter MaskFilter, const struct FCollisionResponseContainer& ResponseToChannels);

	inline void ConditionalSetFlags(EPhysXFilterDataFlags Flag, bool bEnabled)
	{
		if (bEnabled)
		{
			Word3 |= Flag;
		}
	}

	inline void SetExtraFiltering(uint32 ExtraFiltering)
	{
		check(ExtraFiltering < 16);	//we only have 4 bits of extra filtering
		uint32 ExtraFilterMask = ExtraFiltering >> 28;
		Word3 |= ExtraFilterMask;
	}

	inline void GetQueryData(uint32 ActorID, uint32& OutWord0, uint32& OutWord1, uint32& OutWord2, uint32& OutWord3) const
	{
		/**
		 * Format for QueryData : 
		 *		word0 (object ID)
		 *		word1 (blocking channels)
		 *		word2 (touching channels)
		 *		word3 (ExtraFilter (top 4) MyChannel (top 5) as ECollisionChannel + Flags (lower 23))
		 */
		OutWord0 = ActorID;
		OutWord1 = BlockingBits;
		OutWord2 = TouchingBits;
		OutWord3 = Word3;
	}

	inline void GetSimData(uint32 BodyIndex, uint32 ComponentID, uint32& OutWord0, uint32& OutWord1, uint32& OutWord2, uint32& OutWord3) const
	{
		/**
		 * Format for SimData : 
		 * 		word0 (body index)
		 *		word1 (blocking channels)
		 *		word2 (skeletal mesh component ID)
		 * 		word3 (ExtraFilter (top 4) MyChannel (top 5) as ECollisionChannel + Flags (lower 23))
		 */
		OutWord0 = BodyIndex;
		OutWord1 = BlockingBits;
		OutWord2 = ComponentID;
		OutWord3 = Word3;
	}

	inline void GetCombinedData(uint32& OutBlockingBits, uint32& OutTouchingBits, uint32& OutObjectTypeAndFlags) const
	{
		OutBlockingBits = BlockingBits;
		OutTouchingBits = TouchingBits;
		OutObjectTypeAndFlags = Word3;
	}

private:
	uint32 BlockingBits;
	uint32 TouchingBits;
	uint32 Word3;
};

/** Utility for creating a PhysX PxFilterData for filtering query (trace) and sim (physics) from the Unreal filtering info. */
#if WITH_PHYSX
inline void CreateShapeFilterData(
	const uint8 MyChannel,
	const FMaskFilter MaskFilter,
	const int32 ActorID,
	const FCollisionResponseContainer& ResponseToChannels,
	uint32 ComponentID,
	uint16 BodyIndex,
	PxFilterData& OutQueryData,
	PxFilterData& OutSimData,
	bool bEnableCCD,
	bool bEnableContactNotify,
	bool bStaticShape,
	bool bModifyContacts = false)
{
	FPhysicsFilterBuilder Builder((ECollisionChannel)MyChannel, MaskFilter, ResponseToChannels);
	Builder.ConditionalSetFlags(EPDF_CCD, bEnableCCD);
	Builder.ConditionalSetFlags(EPDF_ContactNotify, bEnableContactNotify);
	Builder.ConditionalSetFlags(EPDF_StaticShape, bStaticShape);
	Builder.ConditionalSetFlags(EPDF_ModifyContacts, bModifyContacts);

	OutQueryData.setToDefault();
	OutSimData.setToDefault();
	Builder.GetQueryData(ActorID, OutQueryData.word0, OutQueryData.word1, OutQueryData.word2, OutQueryData.word3);
	Builder.GetSimData(BodyIndex, ComponentID, OutSimData.word0, OutSimData.word1, OutSimData.word2, OutSimData.word3);
}
#endif //WITH_PHYSX

inline ECollisionChannel GetCollisionChannel(uint32 Word3)
{
	uint32 NonFlagMask = Word3 >> 23;
	uint32 ChannelMask = NonFlagMask & 0x1F;	//we only want the first 5 bits because there's only 32 channels: 0b11111
	return (ECollisionChannel)ChannelMask;
}

inline ECollisionChannel GetCollisionChannelAndExtraFilter(uint32 Word3, FMaskFilter& OutMaskFilter)
{
	uint32 NonFlagMask = Word3 >> 23;
	uint32 ChannelMask = NonFlagMask & 0x1F;	//we only want the first 5 bits because there's only 32 channels: 0b11111
	OutMaskFilter = NonFlagMask >> 5;
	return (ECollisionChannel)ChannelMask;
}

inline uint32 CreateChannelAndFilter(ECollisionChannel CollisionChannel, FMaskFilter MaskFilter)
{
	uint32 ResultMask = (MaskFilter << 5) | (uint32)CollisionChannel;
	return ResultMask << 23;
}

inline void UpdateMaskFilter(uint32& Word3, FMaskFilter NewMaskFilter)
{
	Word3 &= 0x0FFFFFFF;	//we ignore the top 4 bits because that's where the new mask filter is going
	Word3 |= NewMaskFilter << 28;
}

/** Utility for creating a PhysX PxFilterData for performing a query (trace) against the scene */
// PxFilterData ZZ_CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex,
// 	const FCollisionResponseContainer& InCollisionResponseContainer,
// 	const struct FCollisionObjectQueryParams & ObjectParam,
// 	const bool bMultitrace);
