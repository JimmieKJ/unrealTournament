// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * Set of flags stored in the PhysX FilterData
 *
 * When this flag is saved in CreateShapeFilterData or CreateQueryFilterData, we only use 24 bits
 * If you plan to use more than 24 bits, you'll also need to change the format of ShapeFilterData,QueryFilterData
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
	ENGINE_API FPhysicsFilterBuilder(TEnumAsByte<enum ECollisionChannel> InObjectType, const struct FCollisionResponseContainer& ResponseToChannels);

	inline void ConditionalSetFlags(EPhysXFilterDataFlags Flag, bool bEnabled)
	{
		if (bEnabled)
		{
			Word3 |= Flag;
		}
	}

	inline void GetQueryData(uint32 ComponentID, uint32& OutWord0, uint32& OutWord1, uint32& OutWord2, uint32& OutWord3) const
	{
		/**
		 * Format for QueryData : 
		 *		word0 (object ID)
		 *		word1 (blocking channels)
		 *		word2 (touching channels)
		 *		word3 (MyChannel (top 8) as ECollisionChannel + Flags (lower 24))
		 */
		OutWord0 = ComponentID;
		OutWord1 = BlockingBits;
		OutWord2 = TouchingBits;
		OutWord3 = Word3;
	}

	inline void GetSimData(uint32 BodyIndex, uint32 SkelMeshCompID, uint32& OutWord0, uint32& OutWord1, uint32& OutWord2, uint32& OutWord3) const
	{
		/**
		 * Format for SimData : 
		 * 		word0 (body index)
		 *		word1 (blocking channels)
		 *		word2 (skeletal mesh component ID)
		 * 		word3 (MyChannel (top 8) + Flags (lower 24))
		 */
		OutWord0 = BodyIndex;
		OutWord1 = BlockingBits;
		OutWord2 = SkelMeshCompID;
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
	const int32 ComponentID,
	const FCollisionResponseContainer& ResponseToChannels,
	uint32 SkelMeshCompID,
	uint16 BodyIndex,
	PxFilterData& OutQueryData,
	PxFilterData& OutSimData,
	bool bEnableCCD,
	bool bEnableContactNotify,
	bool bStaticShape,
	bool bModifyContacts = false)
{
	FPhysicsFilterBuilder Builder((ECollisionChannel)MyChannel, ResponseToChannels);
	Builder.ConditionalSetFlags(EPDF_CCD, bEnableCCD);
	Builder.ConditionalSetFlags(EPDF_ContactNotify, bEnableContactNotify);
	Builder.ConditionalSetFlags(EPDF_StaticShape, bStaticShape);
	Builder.ConditionalSetFlags(EPDF_ModifyContacts, bModifyContacts);

	OutQueryData.setToDefault();
	OutSimData.setToDefault();
	Builder.GetQueryData(ComponentID, OutQueryData.word0, OutQueryData.word1, OutQueryData.word2, OutQueryData.word3);
	Builder.GetSimData(BodyIndex, SkelMeshCompID, OutSimData.word0, OutSimData.word1, OutSimData.word2, OutSimData.word3);
}
#endif //WITH_PHYSX

/** Utility for creating a PhysX PxFilterData for performing a query (trace) against the scene */
// PxFilterData ZZ_CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex,
// 	const FCollisionResponseContainer& InCollisionResponseContainer,
// 	const struct FCollisionObjectQueryParams & ObjectParam,
// 	const bool bMultitrace);
