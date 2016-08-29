// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "Collision.h"
#include "CollisionDebugDrawingPublic.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysXSupport.h"
#include "PhysicsFiltering.h"

#if WITH_BOX2D
#include "../PhysicsEngine2D/Box2DIntegration.h"
#include "CollisionConversions.h"

class FRaycastHelperBox2D_Base : public b2RayCastCallback, public b2QueryCallback
{
public:
	enum EResponse
	{
		ResponseNone = 0,
		ResponseTouch = 1,
		ResponseBlock = 2,
	};

	FRaycastHelperBox2D_Base(ECollisionChannel InTraceChannel, const struct FCollisionQueryParams& InParams, const FCollisionResponseContainer& InCollisionResponseContainer, const FCollisionObjectQueryParams& InObjectParam, bool bInMultitrace)
		: TraceChannel(InTraceChannel)
		, Params(InParams)
		, CollisionResponseContainer(InCollisionResponseContainer)
		, ObjectParam(InObjectParam)
		, QueryBlockingChannels(0)
		, QueryTouchingChannels(0)
		, bMultitrace(bInMultitrace)
	{
		// Box2D is not thread safe
		check(IsInGameThread());

		if (!ObjectParam.IsValid())
		{
			for (int32 i = 0; i < ARRAY_COUNT(CollisionResponseContainer.EnumArray); i++)
			{
				if (CollisionResponseContainer.EnumArray[i] == ECR_Block)
				{
					QueryBlockingChannels |= CRC_TO_BITFIELD(i);
				}
				else if (CollisionResponseContainer.EnumArray[i] == ECR_Overlap)
				{
					QueryTouchingChannels |= CRC_TO_BITFIELD(i);
				}
			}
		}
	}

protected:
	const ECollisionChannel TraceChannel;
	const struct FCollisionQueryParams& Params;
	const FCollisionResponseContainer& CollisionResponseContainer;
	const FCollisionObjectQueryParams& ObjectParam;
	uint32 QueryBlockingChannels;
	uint32 QueryTouchingChannels;
	bool bMultitrace;

protected:
	// Determines how the query and the fixture interact (either no interaction or touching/blocking each other)
	EResponse CalcQueryHitType(b2Fixture* Fixture) const;

	// Converts the query results to a hit result
	void ConvertToHitResult_Internal(FHitResult& OutResult, const FVector& Start, const FVector& End, const float InHitTime, b2Fixture* InFixture, const FVector& InNormal, const EResponse InHitType)
	{
		//@TODO: BOX2D: How to deal with initial penetration (PxU32)(PHit.hadInitialOverlap());
#if 0
		if ((InFixture != nullptr) && PHit.hadInitialOverlap())
		{
			ConvertOverlappedShapeToImpactHit(PHit, StartLoc, EndLoc, OutResult, *InFixture, QueryTM, QueryFilter, Params.bReturnPhysicalMaterial);
			return;
		}
#endif
		OutResult.bStartPenetrating = false;

		check(InFixture);
		FBodyInstance* OwningBodyInstance = static_cast<FBodyInstance*>(InFixture->GetBody()->GetUserData());
		UPrimitiveComponent* OwningComponent = OwningBodyInstance->OwnerComponent.Get();

		/** Set info in the HitResult (Actor, Component, PhysMaterial, BoneName, Item) based on the supplied shape and face index */
		OutResult.PhysMaterial = NULL;

		// Grab actor/component
		if (OwningComponent)
		{
			OutResult.Actor = OwningComponent->GetOwner();
			OutResult.Component = OwningComponent;

			if (Params.bReturnPhysicalMaterial)
			{
				OutResult.PhysMaterial = OwningBodyInstance->GetSimplePhysicalMaterial();
			}
		}

		// Body index / bone name
		OutResult.Item = OwningBodyInstance->InstanceBodyIndex;
		if (OwningBodyInstance->BodySetup.IsValid())
		{
			OutResult.BoneName = OwningBodyInstance->BodySetup->BoneName;
		}

		OutResult.FaceIndex = INDEX_NONE;

		// figure out where the the "safe" location for this shape is by moving from the startLoc toward the ImpactPoint
		const FVector TraceDir = End - Start;
		const FVector SafeLocationToFitShape = Start + (InHitTime * TraceDir);

		// Other info
		OutResult.Location = SafeLocationToFitShape;
		OutResult.ImpactPoint = SafeLocationToFitShape;
		OutResult.Normal = InNormal;
		OutResult.ImpactNormal = OutResult.Normal;

		OutResult.TraceStart = Start;
		OutResult.TraceEnd = End;
		OutResult.Time = InHitTime;

		// See if this is a 'blocking' hit
		OutResult.bBlockingHit = (InHitType == ResponseBlock);
	}
};

FRaycastHelperBox2D_Base::EResponse FRaycastHelperBox2D_Base::CalcQueryHitType(b2Fixture* Fixture) const
{
	const b2Filter& FixtureFilterData = Fixture->GetFilterData();
	const ECollisionChannel ShapeChannel = GetCollisionChannel(FixtureFilterData.ObjectTypeAndFlags);
	const uint32 ShapeBit = ECC_TO_BITFIELD(ShapeChannel);

	EResponse Result = ResponseNone;

	if (ObjectParam.IsValid())
	{
		// do I belong to one of objects of interest?
		if ((ShapeBit & ObjectParam.GetQueryBitfield()) != 0)
		{
			Result = bMultitrace ? ResponseTouch : ResponseBlock;
		}
	}
	else
	{
		// Then see if the channel wants to be blocked
		const uint32 TraceBit = ECC_TO_BITFIELD(TraceChannel);

		// check if the fixture has a hit or touch
		EResponse TraceHitType = ResponseNone;
		if ((TraceBit & FixtureFilterData.BlockingChannels) != 0)
		{
			TraceHitType = ResponseBlock;
		}
		else if ((TraceBit & FixtureFilterData.TouchingChannels) != 0)
		{
			TraceHitType = ResponseTouch;
		}

		// Check if the shape has a hit or touch
		EResponse ShapeHitType = ResponseNone;
		if ((ShapeBit & QueryBlockingChannels) != 0)
		{
			ShapeHitType = ResponseBlock;
		}
		else if ((ShapeBit & QueryTouchingChannels) != 0)
		{
			ShapeHitType = ResponseTouch;
		}

		// return minimum agreed-upon interaction
		Result = FMath::Min(TraceHitType, ShapeHitType);
	}

	// Single traces should ignore touches
	if (!bMultitrace && (Result == ResponseTouch))
	{
		Result = ResponseNone;
	}

	// Check to see if the component ultimately owning the fixture should be ignored
	if (Result != ResponseNone)
	{
		FBodyInstance* OwnerBodyInstance = static_cast<FBodyInstance*>(Fixture->GetBody()->GetUserData());
		UPrimitiveComponent* OwnerComponent = OwnerBodyInstance->OwnerComponent.Get();
		
		if ((OwnerComponent != nullptr) && Params.GetIgnoredComponents().Contains(OwnerComponent->GetUniqueID()))
		{
			Result = ResponseNone;
		}
	}

	return Result;
}

class FRaycastHelperBox2D_RayCastBase : public FRaycastHelperBox2D_Base
{
public:
	FRaycastHelperBox2D_RayCastBase(ECollisionChannel InTraceChannel, const struct FCollisionQueryParams& InParams, const FCollisionResponseContainer& InCollisionResponseContainer, const FCollisionObjectQueryParams& InObjectParam, bool bInMultitrace)
		: FRaycastHelperBox2D_Base(InTraceChannel, InParams, InCollisionResponseContainer, InObjectParam, bInMultitrace)
	{
	}

	// b2RayCastCallback interface
	virtual float32 ReportFixture(b2Fixture* Fixture, const b2Vec2& Point, const b2Vec2& Normal, float32 Fraction) override
	{
		const EResponse ResponseMode = CalcQueryHitType(Fixture);

		if (ResponseMode != ResponseNone)
		{
			ProcessHit(Fixture, Fraction, FPhysicsIntegration2D::ConvertBoxVectorToUnreal(Normal).GetSafeNormal(), ResponseMode);
		}

		switch (ResponseMode)
		{
		case ResponseBlock:
			return Fraction;
		case ResponseTouch:
			return 1.0f;
		default:
		case ResponseNone:
			return -1.0f;
		}
	}
	// End of b2RayCastCallback interface

	// b2QueryCallback interface
	virtual bool ReportFixture(b2Fixture* Fixture) override
	{
		const EResponse ResponseMode = CalcQueryHitType(Fixture);

		bool bReportHit = (ResponseMode == ResponseBlock);

		if (bReportHit)
		{
			// Figure out 'when' the ray hit the fixture
			FBodyInstance* OwningBodyInstance = static_cast<FBodyInstance*>(Fixture->GetBody()->GetUserData());
			UPrimitiveComponent* OwningComponent = OwningBodyInstance->OwnerComponent.Get();
			const float IntersectTime = (OwningComponent != nullptr) ? FPhysicsIntegration2D::ConvertUnrealVectorToPerpendicularDistance(OwningComponent->GetComponentLocation()) : PointQueryStartTime;
			const float Fraction = (IntersectTime - PointQueryStartTime) * PointQueryScaleFactor;

			ProcessHit(Fixture, Fraction, PointQueryNormal, ResponseMode);
		}

		switch (ResponseMode)
		{
		case ResponseBlock:
			return true;
		case ResponseTouch:
			return false;
		default:
		case ResponseNone:
			return false;
		}
	}
	// End of b2QueryCallback interface

protected:
	// Only initialized / used for point queries
	float PointQueryStartTime;
	float PointQueryScaleFactor;
	FVector PointQueryNormal;

protected:
	virtual void ProcessHit(b2Fixture* Fixture, const float HitTime, const FVector& Normal, const EResponse ResponseMode) = 0;

	void RayCastInternal(const b2World* BoxWorld, const FVector& Start, const FVector& End)
	{
		const b2Vec2 BoxStart(FPhysicsIntegration2D::ConvertUnrealVectorToBox(Start));
		const b2Vec2 BoxEnd(FPhysicsIntegration2D::ConvertUnrealVectorToBox(End));

		// Now check the length of the 2D query, we may end up having to do an AABB test instead if it's a ray cast along the 'Z' axis of the 2D scene
		const b2Vec2 BoxDelta = BoxEnd - BoxStart;
		if (BoxDelta.LengthSquared() < SMALL_NUMBER)
		{
			// Tracing directly into the scene, so do a point query instead
			PointQueryStartTime = FPhysicsIntegration2D::ConvertUnrealVectorToPerpendicularDistance(Start);
			const float PointQueryEndTime = FPhysicsIntegration2D::ConvertUnrealVectorToPerpendicularDistance(End);
			PointQueryScaleFactor = 1.0f / (PointQueryEndTime - PointQueryStartTime);
			PointQueryNormal = (Start - End).GetSafeNormal();

			// Run the query
			b2AABB QueryBox;
			QueryBox.lowerBound = BoxStart;
			QueryBox.upperBound = BoxStart;
			BoxWorld->QueryAABB(this, QueryBox);
		}
		else
		{
			// Cast the ray
			BoxWorld->RayCast(this, BoxStart, BoxEnd);
		}
	}
};

class FRaycastHelperBox2D_RayCastSingle : public FRaycastHelperBox2D_RayCastBase
{
public:
	FRaycastHelperBox2D_RayCastSingle(ECollisionChannel InTraceChannel, const struct FCollisionQueryParams& InParams, const FCollisionResponseContainer& InCollisionResponseContainer, const FCollisionObjectQueryParams& InObjectParam)
		: FRaycastHelperBox2D_RayCastBase(InTraceChannel, InParams, InCollisionResponseContainer, InObjectParam, /*bInMultitrace=*/ false)
		, BestHitTime(MAX_FLT)
		, BestFixture(nullptr)
		, bHasBlockingHit(false)
	{
	}

	bool RayCastSingle(const b2World* BoxWorld, const FVector& Start, const FVector& End, bool bAlreadyHadHit, FHitResult& InOutResult)
	{
		RayCastInternal(BoxWorld, Start, End);

		// Accept or merge the result
		if (bHasBlockingHit)
		{
			if (!bAlreadyHadHit || (BestHitTime < InOutResult.Time))
			{
				ConvertToHitResult_Internal(InOutResult, Start, End, BestHitTime, BestFixture, BestNormal, BestHitType);
			}
		}

		return bAlreadyHadHit || bHasBlockingHit;
	}

protected:
	// FRaycastHelperBox2D_RayCastBase interface
	virtual void ProcessHit(b2Fixture* Fixture, const float HitTime, const FVector& Normal, const EResponse ResponseMode) override
	{
		if (ResponseMode == ResponseBlock)
		{
			if (HitTime < BestHitTime)
			{
				BestFixture = Fixture;
				BestHitTime = HitTime;
				BestNormal = Normal;
				BestHitType = ResponseMode;
				bHasBlockingHit = true;
			}
		}
	}
	// End of FRaycastHelperBox2D_RayCastBase interface

protected:
	FVector BestNormal;
	float BestHitTime;
	b2Fixture* BestFixture;
	EResponse BestHitType;

	bool bHasBlockingHit;
};


class FRaycastHelperBox2D_RayCastMultiple : public FRaycastHelperBox2D_RayCastBase
{
public:
	FRaycastHelperBox2D_RayCastMultiple(ECollisionChannel InTraceChannel, const struct FCollisionQueryParams& InParams, const FCollisionResponseContainer& InCollisionResponseContainer, const FCollisionObjectQueryParams& InObjectParam, TArray<struct FHitResult>& InHitsArray, const FVector& InStart, const FVector& InEnd)
		: FRaycastHelperBox2D_RayCastBase(InTraceChannel, InParams, InCollisionResponseContainer, InObjectParam, /*bInMultitrace=*/ true)
		, HitsArray(InHitsArray)
		, Start(InStart)
		, End(InEnd)
	{
	}

	bool RayCastMultiple(const b2World* BoxWorld)
	{
		const int32 PreCount = HitsArray.Num();

		RayCastInternal(BoxWorld, Start, End);

		if (PreCount != HitsArray.Num())
		{
			// Sort the newly added results from first to last hit
			HitsArray.Sort(FCompareFHitResultTime());
		}

		return HitsArray.Num() > 0;
	}

protected:
	TArray<struct FHitResult>& HitsArray;
	const FVector& Start;
	const FVector& End;

protected:
	// FRaycastHelperBox2D_RayCastBase interface
	virtual void ProcessHit(b2Fixture* Fixture, const float HitTime, const FVector& Normal, const EResponse ResponseMode) override
	{
		if (ResponseMode == ResponseBlock)
		{
			FHitResult& NewResult = *new (HitsArray) FHitResult();
			ConvertToHitResult_Internal(NewResult, Start, End, HitTime, Fixture, Normal, ResponseMode);
		}
	}
	// End of FRaycastHelperBox2D_RayCastBase interface
};


class FRaycastHelperBox2D_RayTest : public FRaycastHelperBox2D_Base
{
public:
	FRaycastHelperBox2D_RayTest(ECollisionChannel InTraceChannel, const struct FCollisionQueryParams& InParams, const FCollisionResponseContainer& InCollisionResponseContainer, const FCollisionObjectQueryParams& InObjectParam)
		: FRaycastHelperBox2D_Base(InTraceChannel, InParams, InCollisionResponseContainer, InObjectParam, /*bInMultitrace=*/ false)
		, bHasBlockingHit(false)
	{
	}

	bool RayTest(const b2World* BoxWorld, const FVector& Start, const FVector& End)
	{
		const b2Vec2 BoxStart(FPhysicsIntegration2D::ConvertUnrealVectorToBox(Start));
		const b2Vec2 BoxEnd(FPhysicsIntegration2D::ConvertUnrealVectorToBox(End));

		// Now check the length of the 2D query, we may end up having to do an AABB test instead if it's a ray cast along the 'Z' axis of the 2D scene
		const b2Vec2 BoxDelta = BoxEnd - BoxStart;
		if (BoxDelta.LengthSquared() < SMALL_NUMBER)
		{
			// Trace into the scene, do a point query instead
			b2AABB QueryBox;
			QueryBox.lowerBound = BoxStart;
			QueryBox.upperBound = BoxStart;
			BoxWorld->QueryAABB(this, QueryBox);
		}
		else
		{
			// Cast the ray
			BoxWorld->RayCast(this, BoxStart, BoxEnd);
		}

		return bHasBlockingHit;
	}

	// b2RayCastCallback interface
	virtual float32 ReportFixture(b2Fixture* Fixture, const b2Vec2& Point, const b2Vec2& Normal, float32 Fraction) override
	{
		const EResponse ResponseMode = CalcQueryHitType(Fixture);

		switch (ResponseMode)
		{
		case ResponseBlock:
			bHasBlockingHit = true;
			return 0.0f;
		case ResponseTouch:
			return 1.0f;
		default:
		case ResponseNone:
			return -1.0f;
		}
	}
	// End of b2RayCastCallback interface

	// b2QueryCallback interface
	virtual bool ReportFixture(b2Fixture* Fixture) override
	{
		const EResponse ResponseMode = CalcQueryHitType(Fixture);

		if (ResponseMode == ResponseBlock)
		{
			bHasBlockingHit = true;
			return true;
		}
		else
		{
			return false;
		}
	}
	// End of b2QueryCallback interface

private:
	bool bHasBlockingHit;
};

#endif	//WITH_BOX2D

#if WITH_PHYSX

#include "../PhysicsEngine/PhysXSupport.h"
#include "PhysXCollision.h"
#include "CollisionDebugDrawing.h"
#include "CollisionConversions.h"

float DebugLineLifetime = 2.f;

/**
 * Helper to lock/unlock multiple scenes that also makes sure to unlock everything when it goes out of scope.
 * Multiple locks on the same scene are NOT SAFE. You can't call LockRead() if already locked.
 * Multiple unlocks on the same scene are safe (repeated unlocks do nothing after the first successful unlock).
 */
struct FScopedMultiSceneReadLock
{
	FScopedMultiSceneReadLock()
	{
		for (int32 i=0; i < ARRAY_COUNT(SceneLocks); i++)
		{
			SceneLocks[i] = nullptr;
		}
	}

	~FScopedMultiSceneReadLock()
	{
		UnlockAll();
	}

	inline void LockRead(PxScene* Scene, int32 SceneIndex)
	{
		checkSlow(SceneLocks[SceneIndex] == nullptr); // no nested locks allowed.
		SCENE_LOCK_READ(Scene);
		SceneLocks[SceneIndex] = Scene;
	}

	inline void UnlockRead(PxScene* Scene, int32 SceneIndex)
	{
		checkSlow(SceneLocks[SceneIndex] == Scene || SceneLocks[SceneIndex] == nullptr);
		SCENE_UNLOCK_READ(Scene);
		SceneLocks[SceneIndex] = nullptr;
	}

	inline void UnlockAll()
	{
		for (int32 i=0; i < ARRAY_COUNT(SceneLocks); i++)
		{
			SCENE_UNLOCK_READ(SceneLocks[i]);
			SceneLocks[i] = nullptr;
		}
	}

	PxScene* SceneLocks[PST_MAX];
};


/** 
 * Type of query for object type or trace type
 * Trace queries correspond to trace functions with TravelChannel/ResponseParams
 * Object queries correspond to trace functions with Object types 
 */
namespace ECollisionQuery
{
	enum Type
	{
		ObjectQuery=0,
		TraceQuery=1
	};
}

#define TRACE_MULTI		1
#define TRACE_SINGLE	0

PxSceneQueryHitType::Enum FPxQueryFilterCallback::CalcQueryHitType(const PxFilterData &PQueryFilter, const PxFilterData &PShapeFilter, bool bPreFilter)
{
	ECollisionQuery::Type QueryType = (ECollisionQuery::Type)PQueryFilter.word0;
	FMaskFilter QuerierMaskFilter;
	const ECollisionChannel QuerierChannel = GetCollisionChannelAndExtraFilter(PQueryFilter.word3, QuerierMaskFilter);

	FMaskFilter ShapeMaskFilter;
	const ECollisionChannel ShapeChannel = GetCollisionChannelAndExtraFilter(PShapeFilter.word3, ShapeMaskFilter);

	if ((QuerierMaskFilter & ShapeMaskFilter) != 0)	//If ignore mask hit something, ignore it
	{
		return PxSceneQueryHitType::eNONE;
	}

	const PxU32 ShapeBit = ECC_TO_BITFIELD(ShapeChannel);
	if (QueryType == ECollisionQuery::ObjectQuery)
	{
		const int32 MultiTrace = (int32) QuerierChannel;
		// do I belong to one of objects of interest?
		if ( ShapeBit & PQueryFilter.word1)
		{
			if (bPreFilter)	//In the case of an object query we actually want to return all object types (or first in single case). So in PreFilter we have to trick physx by not blocking in the multi case, and blocking in the single case.
			{

				return MultiTrace ? PxSceneQueryHitType::eTOUCH : PxSceneQueryHitType::eBLOCK;
			}
			else
			{
				return PxSceneQueryHitType::eBLOCK;	//In the case where an object query is being resolved for the user we just return a block because object query doesn't have the concept of overlap at all and block seems more natural
			}
		}
	}
	else
	{
		// Then see if the channel wants to be blocked
		// @todo delete once we fix up object/trace APIs to work separate
		PxU32 ShapeFlags = PShapeFilter.word3 & 0xFFFFFF;
		bool bStaticShape = ((ShapeFlags & EPDF_StaticShape) != 0);

		// if query channel is Touch All, then just return touch
		if (QuerierChannel == ECC_OverlapAll_Deprecated)
		{
			return PxSceneQueryHitType::eTOUCH;
		}
		// @todo delete once we fix up object/trace APIs to work separate

		PxU32 const QuerierBit = ECC_TO_BITFIELD(QuerierChannel);
		PxSceneQueryHitType::Enum QuerierHitType = PxSceneQueryHitType::eNONE;
		PxSceneQueryHitType::Enum ShapeHitType = PxSceneQueryHitType::eNONE;

		// check if Querier wants a hit
		if ((QuerierBit & PShapeFilter.word1) != 0)
		{
			QuerierHitType = PxSceneQueryHitType::eBLOCK;
		}
		else if ((QuerierBit & PShapeFilter.word2)!=0)
		{
			QuerierHitType = PxSceneQueryHitType::eTOUCH;
		}

		if ((ShapeBit & PQueryFilter.word1) != 0)
		{
			ShapeHitType = PxSceneQueryHitType::eBLOCK;
		}
		else if ((ShapeBit & PQueryFilter.word2)!=0)
		{
			ShapeHitType = PxSceneQueryHitType::eTOUCH;
		}

		// return minimum agreed-upon interaction
		return FMath::Min(QuerierHitType, ShapeHitType);
	}

	return PxSceneQueryHitType::eNONE;
}

PxSceneQueryHitType::Enum FPxQueryFilterCallback::preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxSceneQueryFlags& queryFlags)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_PreFilter);

	// Check if the shape is the right complexity for the trace 
	PxFilterData ShapeFilter = shape->getQueryFilterData();
	PxFilterData ShapeSimFilter = shape->getSimulationFilterData();	//This is a bit of a hack. We do this because word2 has our component ID
#define ENABLE_PREFILTER_LOGGING 0
#if ENABLE_PREFILTER_LOGGING
	static bool bLoggingEnabled=false;
	if (bLoggingEnabled)
	{
		FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(actor->userData);
		if ( BodyInst && BodyInst->OwnerComponent.IsValid() )
		{
			UE_LOG(LogCollision, Warning, TEXT("[PREFILTER] against %s[%s] : About to check "),
				(BodyInst->OwnerComponent.Get()->GetOwner())? *BodyInst->OwnerComponent.Get()->GetOwner()->GetName():TEXT("NO OWNER"),
				*BodyInst->OwnerComponent.Get()->GetName());
		}

		UE_LOG(LogCollision, Warning, TEXT("ShapeFilter : %x %x %x %x"), ShapeFilter.word0, ShapeFilter.word1, ShapeFilter.word2, ShapeFilter.word3);
		UE_LOG(LogCollision, Warning, TEXT("FilterData : %x %x %x %x"), filterData.word0, filterData.word1, filterData.word2, filterData.word3);
	}
#endif // ENABLE_PREFILTER_LOGGING

	// Shape : shape's Filter Data
	// Querier : filterData that owns the trace
	PxU32 ShapeFlags = ShapeFilter.word3 & 0xFFFFFF;
	PxU32 QuerierFlags = filterData.word3 & 0xFFFFFF;
	PxU32 CommonFlags = ShapeFlags & QuerierFlags;

	// First check complexity, none of them matches
	if(!(CommonFlags & EPDF_SimpleCollision) && !(CommonFlags & EPDF_ComplexCollision))
	{
		return (PrefilterReturnValue = PxSceneQueryHitType::eNONE);
	}

	PxSceneQueryHitType::Enum Result = FPxQueryFilterCallback::CalcQueryHitType(filterData, ShapeFilter, true);

	if (Result == PxSceneQueryHitType::eTOUCH && bIgnoreTouches)
	{
		Result = PxSceneQueryHitType::eNONE;
	}

	if (Result == PxSceneQueryHitType::eBLOCK && bIgnoreBlocks)
	{
		Result = PxSceneQueryHitType::eNONE;
	}

	// If not already rejected, check ignore actor and component list.
	if (Result != PxSceneQueryHitType::eNONE)
	{
		// See if we are ignoring the actor this shape belongs to (word0 of shape filterdata is actorID) or the component (word2 of shape sim filter data is componentID)
		if (IgnoreActors.Contains(ShapeFilter.word0) || IgnoreComponents.Contains(ShapeSimFilter.word2))
		{
			//UE_LOG(LogTemp, Log, TEXT("Ignoring Actor: %d"), ShapeFilter.word0);
			Result = PxSceneQueryHitType::eNONE;
		}
	}

#if ENABLE_PREFILTER_LOGGING
	if (bLoggingEnabled)
	{
		FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(actor->userData);
		if ( BodyInst && BodyInst->OwnerComponent.IsValid() )
		{
			ECollisionChannel QuerierChannel = GetCollisionChannel(filterData.word3);
			UE_LOG(LogCollision, Log, TEXT("[PREFILTER] Result for Querier [CHANNEL: %d, FLAG: %x] %s[%s] [%d]"), 
				(int32)QuerierChannel, QuerierFlags,
				(BodyInst->OwnerComponent.Get()->GetOwner())? *BodyInst->OwnerComponent.Get()->GetOwner()->GetName():TEXT("NO OWNER"),
				*BodyInst->OwnerComponent.Get()->GetName(), (int32)Result);
		}
	}
#endif // ENABLE_PREFILTER_LOGGING

	return  (PrefilterReturnValue = Result);
}


PxSceneQueryHitType::Enum FPxQueryFilterCallbackSweep::postFilter(const PxFilterData& filterData, const PxSceneQueryHit& hit)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_PostFilter);

	PxSweepHit& SweepHit = (PxSweepHit&)hit;
	const bool bIsOverlap = SweepHit.hadInitialOverlap();

	if (bIsOverlap && DiscardInitialOverlaps)
	{
		return PxSceneQueryHitType::eNONE;
	}
	else
	{
		if (bIsOverlap && PrefilterReturnValue == PxSceneQueryHitType::eBLOCK)
		{
			// We want to keep initial blocking overlaps and continue the sweep until a non-overlapping blocking hit.
			// We will later report this hit as a blocking hit when we compute the hit type (using FPxQueryFilterCallback::CalcQueryHitType).
			return PxSceneQueryHitType::eTOUCH;
		}
		
		return PrefilterReturnValue;
	}
}

#endif // WITH_PHYSX

//////////////////////////////////////////////////////////////////////////

//@TODO: BOX2D: Can we break the collision analyzer's dependence on PhysX?
#if ENABLE_COLLISION_ANALYZER

#include "CollisionAnalyzerModule.h"

bool GCollisionAnalyzerIsRecording = false;

bool bSkipCapture = false;

/** Util to convert from PhysX shape and rotation to unreal shape enum, dimension vector and rotation */
static void P2UGeomAndRot(const PxGeometry& PGeom, const PxQuat& PRot, ECAQueryShape::Type& OutQueryShape, FVector& OutDims, FQuat& OutQuat)
{
	OutQueryShape = ECAQueryShape::Capsule;
	OutDims = FVector(0, 0, 0);
	OutQuat = FQuat::Identity;

	switch (PGeom.getType())
	{
	case PxGeometryType::eCAPSULE:
	{
		OutQueryShape = ECAQueryShape::Capsule;
		PxCapsuleGeometry* PCapsuleGeom = (PxCapsuleGeometry*)&PGeom;
		OutDims = FVector(PCapsuleGeom->radius, PCapsuleGeom->radius, PCapsuleGeom->halfHeight + PCapsuleGeom->radius);
		OutQuat = ConvertToUECapsuleRot(PRot);
		break;
	}

	case PxGeometryType::eSPHERE:
	{
		OutQueryShape = ECAQueryShape::Sphere;
		PxSphereGeometry* PSphereGeom = (PxSphereGeometry*)&PGeom;
		OutDims = FVector(PSphereGeom->radius);
		break;
	}

	case PxGeometryType::eBOX:
	{
		OutQueryShape = ECAQueryShape::Box;
		PxBoxGeometry* PBoxGeom = (PxBoxGeometry*)&PGeom;
		OutDims = P2UVector(PBoxGeom->halfExtents);
		OutQuat = P2UQuat(PRot);
		break;
	}

	case PxGeometryType::eCONVEXMESH:
	{
		OutQueryShape = ECAQueryShape::Convex;
		break;
	}

	default:
		UE_LOG(LogCollision, Warning, TEXT("CaptureGeomSweep: Unknown geom type."));
	}
}

/** Util to extract type and dimensions from physx geom being swept, and pass info to CollisionAnalyzer, if its recording */
void CaptureGeomSweep(const UWorld* World, const FVector& Start, const FVector& End, const PxQuat& PRot, ECAQueryMode::Type QueryMode, const PxGeometry& PGeom, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams, const TArray<FHitResult>& Results, double CPUTime)
{
	if(bSkipCapture || !GCollisionAnalyzerIsRecording || !IsInGameThread())
	{
		return;
	}

	// Convert from PhysX to Unreal types
	ECAQueryShape::Type QueryShape = ECAQueryShape::Sphere;
	FVector Dims(0,0,0);
	FQuat Rot = FQuat::Identity;
	P2UGeomAndRot(PGeom, PRot, QueryShape, Dims, Rot);

	// Do a touch all query to find things we _didn't_ hit
	bSkipCapture = true;
	TArray<FHitResult> TouchAllResults;
	GeomSweepMulti_PhysX(World, PGeom, PRot, TouchAllResults, Start, End, DefaultCollisionChannel, Params, ResponseParams, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllObjects));
	bSkipCapture = false;

	// Now tell analyzer
	FCollisionAnalyzerModule::Get()->CaptureQuery(Start, End, Rot, ECAQueryType::GeomSweep, QueryShape, QueryMode, Dims, TraceChannel, Params, ResponseParams, ObjectParams, Results, TouchAllResults, CPUTime);
}

/** Util to capture a raycast with the CollisionAnalyzer if recording */
void CaptureRaycast(const UWorld* World, const FVector& Start, const FVector& End, ECAQueryMode::Type QueryMode, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams, const TArray<FHitResult>& Results, double CPUTime)
{
	if(bSkipCapture || !GCollisionAnalyzerIsRecording || !IsInGameThread())
	{
		return;
	}

	// Do a touch all query to find things we _didn't_ hit
	bSkipCapture = true;
	TArray<FHitResult> TouchAllResults;
	RaycastMulti(World, TouchAllResults, Start, End, DefaultCollisionChannel, Params, ResponseParams, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllObjects));
	bSkipCapture = false;

	FCollisionAnalyzerModule::Get()->CaptureQuery(Start, End, FQuat::Identity, ECAQueryType::Raycast, ECAQueryShape::Sphere, QueryMode, FVector(0,0,0), TraceChannel, Params, ResponseParams, ObjectParams, Results, TouchAllResults, CPUTime);
}

void CaptureOverlap(const UWorld* World, const PxGeometry& PGeom, const PxTransform& PGeomPose, ECAQueryMode::Type QueryMode, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams, TArray<FOverlapResult>& Results, double CPUTime)
{
	if (bSkipCapture || !GCollisionAnalyzerIsRecording || !IsInGameThread())
	{
		return;
	}

	ECAQueryShape::Type QueryShape = ECAQueryShape::Sphere;
	FVector Dims(0, 0, 0);
	FQuat Rot = FQuat::Identity;
	P2UGeomAndRot(PGeom, PGeomPose.q, QueryShape, Dims, Rot);

	TArray<FHitResult> HitResults;
	for(const FOverlapResult& OverlapResult : Results)
	{
		FHitResult NewResult = FHitResult(0.f);
		NewResult.bBlockingHit = OverlapResult.bBlockingHit;
		NewResult.Actor = OverlapResult.Actor;
		NewResult.Component = OverlapResult.Component;
		NewResult.Item = OverlapResult.ItemIndex;
		HitResults.Add(NewResult);
	}

	TArray<FHitResult> TouchAllResults;
	// TODO: Fill in 'all results' for overlaps

	FCollisionAnalyzerModule::Get()->CaptureQuery(P2UVector(PGeomPose.p), FVector(0,0,0), Rot, ECAQueryType::GeomOverlap, QueryShape, QueryMode, Dims, TraceChannel, Params, ResponseParams, ObjectParams, HitResults, TouchAllResults, CPUTime);
}

#define STARTQUERYTIMER() double StartTime = FPlatformTime::Seconds()
#define CAPTUREGEOMSWEEP(World, Start, End, Rot, QueryMode, PGeom, TraceChannel, Params, ResponseParam, ObjectParam, Results) if (GCollisionAnalyzerIsRecording && IsInGameThread()) { CaptureGeomSweep(World, Start, End, Rot, QueryMode, PGeom, TraceChannel, Params, ResponseParam, ObjectParam, Results, FPlatformTime::Seconds() - StartTime); }
#define CAPTURERAYCAST(World, Start, End, QueryMode, TraceChannel, Params, ResponseParam, ObjectParam, Results)	if (GCollisionAnalyzerIsRecording && IsInGameThread()) { CaptureRaycast(World, Start, End, QueryMode, TraceChannel, Params, ResponseParam, ObjectParam, Results, FPlatformTime::Seconds() - StartTime); }
#define CAPTUREGEOMOVERLAP(World, PGeom, PGeomPose, QueryMode, TraceChannel, Params, ResponseParams, ObjectParams, Results)	if (GCollisionAnalyzerIsRecording && IsInGameThread()) { CaptureOverlap(World, PGeom, PGeomPose, QueryMode, TraceChannel, Params, ResponseParams, ObjectParams, Results, FPlatformTime::Seconds() - StartTime); }


#else

#define STARTQUERYTIMER() 
#define CAPTUREGEOMSWEEP(...)
#define CAPTURERAYCAST(...)
#define CAPTUREGEOMOVERLAP(...)

#endif // ENABLE_COLLISION_ANALYZER

#if WITH_PHYSX
PxQueryFlags StaticDynamicQueryFlags(const FCollisionQueryParams& Params)
{
	switch(Params.MobilityType)
	{
		case EQueryMobilityType::Any: return  PxSceneQueryFilterFlag::eSTATIC | PxSceneQueryFilterFlag::eDYNAMIC;
		case EQueryMobilityType::Static: return  PxSceneQueryFilterFlag::eSTATIC;
		case EQueryMobilityType::Dynamic: return  PxSceneQueryFilterFlag::eDYNAMIC;
		default: check(0);
	}

	check(0);
	return PxSceneQueryFilterFlag::eSTATIC | PxSceneQueryFilterFlag::eDYNAMIC;
}
#endif


//////////////////////////////////////////////////////////////////////////
// RAYCAST

#if UE_WITH_PHYSICS

bool RaycastTest(const UWorld* World, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastAny);
	STARTQUERYTIMER();

	bool bHaveBlockingHit = false; // Track if we get any 'blocking' hits

	FVector Delta = End - Start;
	float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
#if WITH_PHYSX
		{
			const PxVec3 PDir = U2PVector(Delta / DeltaMag);
			PxRaycastHit PHit;

			// Create filter data used to filter collisions
			PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, false);
			PxSceneQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxSceneQueryFilterFlag::ePREFILTER);
			PxSceneQueryFlags POutputFlags = PxHitFlags();
			FPxQueryFilterCallback PQueryCallback(Params);
			PQueryCallback.bIgnoreTouches = true; // pre-filter to ignore touches and only get blocking hits.

			FPhysScene* PhysScene = World->GetPhysicsScene();
			{
				// Enable scene locks, in case they are required
				PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);
				SCOPED_SCENE_READ_LOCK(SyncScene);
				bHaveBlockingHit = SyncScene->raycastAny(U2PVector(Start), PDir, DeltaMag, PHit, PQueryFilterData, &PQueryCallback);
			}

			// Test async scene if we have no blocking hit, and async tests are requested
			if (!bHaveBlockingHit && Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
			{
				PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);
				SCOPED_SCENE_READ_LOCK(AsyncScene);
				bHaveBlockingHit = AsyncScene->raycastAny(U2PVector(Start), PDir, DeltaMag, PHit, PQueryFilterData, &PQueryCallback);
			}
		}
#endif // WITH_PHYSX

#if WITH_BOX2D
		if (!bHaveBlockingHit && GetDefault<UPhysicsSettings>()->bEnable2DPhysics)
		{
			if (const b2World* BoxWorld = FPhysicsIntegration2D::FindAssociatedWorld(const_cast<UWorld*>(World)))
			{
				FRaycastHelperBox2D_RayTest BoxQuery(TraceChannel, Params, ResponseParams.CollisionResponse, ObjectParams);
				bHaveBlockingHit = BoxQuery.RayTest(BoxWorld, Start, End);
			}
		}
#endif // WITH_BOX2D
	}

	TArray<FHitResult> Hits;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ((World->DebugDrawTraceTag != NAME_None) && (World->DebugDrawTraceTag == Params.TraceTag) && IsInGameThread())
	{
		DrawLineTraces(World, Start, End, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	CAPTURERAYCAST(World, Start, End, ECAQueryMode::Test, TraceChannel, Params, ResponseParams, ObjectParams, Hits);

	return bHaveBlockingHit;
}

bool RaycastSingle(const UWorld* World, struct FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastSingle);
	STARTQUERYTIMER();

	OutHit = FHitResult();
	OutHit.TraceStart = Start;
	OutHit.TraceEnd = End;

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	bool bHaveBlockingHit = false; // Track if we get any 'blocking' hits

	FVector Delta = End - Start;
	float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
#if WITH_PHYSX
		{
			FScopedMultiSceneReadLock SceneLocks;

			// Create filter data used to filter collisions
			PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, false);
			PxSceneQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxSceneQueryFilterFlag::ePREFILTER);
			PxSceneQueryFlags POutputFlags = PxSceneQueryFlag::ePOSITION | PxSceneQueryFlag::eNORMAL | PxSceneQueryFlag::eDISTANCE | PxSceneQueryFlag::eMTD;
			FPxQueryFilterCallback PQueryCallback(Params);
			PQueryCallback.bIgnoreTouches = true; // pre-filter to ignore touches and only get blocking hits.

			PxVec3 PStart = U2PVector(Start);
			PxVec3 PDir = U2PVector(Delta / DeltaMag);

			FPhysScene* PhysScene = World->GetPhysicsScene();
			PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);

			// Enable scene locks, in case they are required
			SceneLocks.LockRead(SyncScene, PST_Sync);

			PxRaycastHit PHit;
			bHaveBlockingHit = SyncScene->raycastSingle(U2PVector(Start), PDir, DeltaMag, POutputFlags, PHit, PQueryFilterData, &PQueryCallback);
			if (!bHaveBlockingHit)
			{
				// Not going to use anything from this scene, so unlock it now.
				SceneLocks.UnlockRead(SyncScene, PST_Sync);
			}

			// Test async scene if async tests are requested
			if (Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
			{
				PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);
				SceneLocks.LockRead(AsyncScene, PST_Async);
				PxRaycastHit PHitAsync;
				const bool bHaveBlockingHitAsync = AsyncScene->raycastSingle(U2PVector(Start), PDir, DeltaMag, POutputFlags, PHitAsync, PQueryFilterData, &PQueryCallback);

				// If we have a blocking hit in the async scene and there was no sync blocking hit, or if the async blocking hit came first,
				// then this becomes the blocking hit.  We can test the PxSceneQueryImpactHit::distance since the DeltaMag is the same for both queries.
				if (bHaveBlockingHitAsync && (!bHaveBlockingHit || PHitAsync.distance < PHit.distance))
				{
					PHit = PHitAsync;
					bHaveBlockingHit = true;
				}
				else
				{
					// Not going to use anything from this scene, so unlock it now.
					SceneLocks.UnlockRead(AsyncScene, PST_Async);
				}
			}

			if (bHaveBlockingHit) // If we got a hit
			{
				PxTransform PStartTM(U2PVector(Start));
				if (ConvertQueryImpactHit(World, PHit, OutHit, DeltaMag, PFilter, Start, End, NULL, PStartTM, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Invalid)
				{
					bHaveBlockingHit = false;
					UE_LOG(LogCollision, Error, TEXT("RaycastSingle resulted in a NaN/INF in PHit!"));
#if ENABLE_NAN_DIAGNOSTIC
					UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
					UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
					UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
					UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
				}
			}
		}
#endif //WITH_PHYSX

#if WITH_BOX2D
		if (GetDefault<UPhysicsSettings>()->bEnable2DPhysics)
		{
			if (const b2World* BoxWorld = FPhysicsIntegration2D::FindAssociatedWorld(const_cast<UWorld*>(World)))
			{
				FRaycastHelperBox2D_RayCastSingle BoxQuery(TraceChannel, Params, ResponseParams.CollisionResponse, ObjectParams);

				bHaveBlockingHit = BoxQuery.RayCastSingle(BoxWorld, Start, End, bHaveBlockingHit, OutHit);
			}
		}
#endif // WITH_BOX2D
	}


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ((World->DebugDrawTraceTag != NAME_None) && (World->DebugDrawTraceTag == Params.TraceTag) && IsInGameThread())
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(OutHit);
		}
		DrawLineTraces(World, Start, End, Hits, DebugLineLifetime);	
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording && IsInGameThread())
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(OutHit);
		}
		CAPTURERAYCAST(World, Start, End, ECAQueryMode::Single, TraceChannel, Params, ResponseParams, ObjectParams, Hits);
	}
#endif

	return bHaveBlockingHit;
}

bool RaycastMulti(const UWorld* World, TArray<struct FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastMultiple);
	STARTQUERYTIMER();

	OutHits.Reset();

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bHaveBlockingHit = false;

	FVector Delta = End - Start;
	float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
#if WITH_PHYSX
		// Create filter data used to filter collisions
		PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, true);
		PxSceneQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxSceneQueryFilterFlag::ePREFILTER);
		PxSceneQueryFlags POutputFlags = PxSceneQueryFlag::ePOSITION | PxSceneQueryFlag::eNORMAL | PxSceneQueryFlag::eDISTANCE | PxSceneQueryFlag::eMTD;
		FPxQueryFilterCallback PQueryCallback(Params);

		// Create buffer for hits. Note: memory is not initialized (for perf reasons), since API does not require it.
		TTypeCompatibleBytes<PxRaycastHit> RawPHits[HIT_BUFFER_SIZE];
		PxRaycastHit* PHits = (PxRaycastHit*)RawPHits;

		bool bBlockingHit = false;
		const PxVec3 PDir = U2PVector(Delta/DeltaMag);

		// Enable scene locks, in case they are required
		FPhysScene* PhysScene = World->GetPhysicsScene();
		PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);

		FScopedMultiSceneReadLock SceneLocks;
		SceneLocks.LockRead(SyncScene, PST_Sync);

		PxI32 NumHits = SyncScene->raycastMultiple(U2PVector(Start), PDir, DeltaMag, POutputFlags, PHits, HIT_BUFFER_MAX_SYNC_QUERIES, bBlockingHit, PQueryFilterData, &PQueryCallback);

		if(NumHits == -1)
		{
			UE_LOG(LogCollision, Warning, TEXT("RaycastMulti : Buffer overflow from synchronous scene query"));
			UE_LOG(LogCollision, Warning, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
			UE_LOG(LogCollision, Warning, TEXT("--------%s"), *Params.ToString());
			// raycastMultiple still fills in the buffer with an arbitrary set of overlapping objects, so we have a full buffer of results
			// we can use, though that is not the entire set
			NumHits = HIT_BUFFER_MAX_SYNC_QUERIES;
		}
		else if (NumHits == 0)
		{
			// Not going to use anything from this scene, so unlock it now.
			SceneLocks.UnlockRead(SyncScene, PST_Sync);
		}

		// Test async scene if async tests are requested and there was no overflow
		if( Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
		{
			PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);
			SceneLocks.LockRead(AsyncScene, PST_Async);

			// Write into the same PHits buffer
			PxRaycastHit* PHitsAsync = PHits+NumHits;
			const PxI32 AsyncBufferSize = (ARRAY_COUNT(RawPHits) - NumHits);
			check(AsyncBufferSize > 0);
			bool bBlockingHitAsync = false;

			// If we have a blocking hit from the sync scene, there is no need to raycast past that hit
			const float RayLength = bBlockingHit ? PHits[NumHits-1].distance : DeltaMag;

			PxI32 NumAsyncHits = 0;
			if(RayLength > SMALL_NUMBER) // don't bother doing the trace if the sync scene trace gave a hit time of zero
			{
				NumAsyncHits = AsyncScene->raycastMultiple(U2PVector(Start), PDir, RayLength, POutputFlags, PHitsAsync, AsyncBufferSize, bBlockingHitAsync, PQueryFilterData, &PQueryCallback);
			}

			if(NumAsyncHits == -1)
			{
				UE_LOG(LogCollision, Log, TEXT("RaycastMulti : Buffer overflow from asyncrhonous scene query"));
				UE_LOG(LogCollision, Warning, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Warning, TEXT("--------%s"), *Params.ToString());
				// raycastMultiple still fills in the buffer with an arbitrary set of overlapping objects, so we have a full buffer of results
				// we can use, though that is not the entire set
				NumAsyncHits = AsyncBufferSize;
			}
			else if (NumAsyncHits == 0)
			{
				// Not going to use anything from this scene, so unlock it now.
				SceneLocks.UnlockRead(AsyncScene, PST_Async);
			}

			PxI32 TotalNumHits = NumHits + NumAsyncHits;

			// If there is a blocking hit in the sync scene, and it is closer than the blocking hit in the async scene (or there is no blocking hit in the async scene),
			// then move it to the end of the array to get it out of the way.
			if (bBlockingHit)
			{
				if (!bBlockingHitAsync || PHits[NumHits-1].distance < PHits[TotalNumHits-1].distance)
				{
					PHits[TotalNumHits-1] = PHits[NumHits-1];
				}
			}

			// Merge results
			NumHits = TotalNumHits;

			bBlockingHit = bBlockingHit || bBlockingHitAsync;

			// Now eliminate hits which are farther than the nearest blocking hit, or even those that are the exact same distance as the blocking hit,
			// to ensure the blocking hit is the last in the array after sorting in ConvertRaycastResults (below).
			if (bBlockingHit)
			{
				const PxF32 MaxDistance = PHits[NumHits-1].distance;
				PxI32 TestHitCount = NumHits-1;
				for (PxI32 HitNum = TestHitCount; HitNum-- > 0;)
				{
					if (PHits[HitNum].distance >= MaxDistance)
					{
						PHits[HitNum] = PHits[--TestHitCount];
					}
				}
				if (TestHitCount < NumHits-1)
				{
					PHits[TestHitCount] = PHits[NumHits-1];
					NumHits = TestHitCount + 1;
				}
			}
		}

		if (NumHits > 0)
		{
			if (ConvertRaycastResults(bBlockingHit, World, NumHits, PHits, DeltaMag, PFilter, OutHits, Start, End, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Invalid)
			{
				// We don't need to change bBlockingHit, that's done by ConvertRaycastResults if it removed the blocking hit.
				UE_LOG(LogCollision, Error, TEXT("RaycastMulti resulted in a NaN/INF in PHit!"));
#if ENABLE_NAN_DIAGNOSTIC
				UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
			}
		}

		bHaveBlockingHit = bBlockingHit;
#endif // WITH_PHYSX

#if WITH_BOX2D && 0
		if (GetDefault<UPhysicsSettings>()->bEnable2DPhysics)
		{
			if (const b2World* BoxWorld = FPhysicsIntegration2D::FindAssociatedWorld(const_cast<UWorld*>(World)))
			{
				FRaycastHelperBox2D_RayCastMultiple BoxQuery(TraceChannel, Params, ResponseParams.CollisionResponse, ObjectParams, OutHits, Start, End);

				if (BoxQuery.RayCastMultiple(BoxWorld))
				{
					bHaveBlockingHit = true;
				}
			}
		}
#endif // WITH_BOX2D
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if((World->DebugDrawTraceTag != NAME_None) && (World->DebugDrawTraceTag == Params.TraceTag))
	{
		DrawLineTraces(World, Start, End, OutHits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	CAPTURERAYCAST(World, Start, End, ECAQueryMode::Multi, TraceChannel, Params, ResponseParams, ObjectParams, OutHits);

	return bHaveBlockingHit;
}

//////////////////////////////////////////////////////////////////////////
// GEOM SWEEP

bool GeomSweepTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepAny);
	STARTQUERYTIMER();

	bool bHaveBlockingHit = false; // Track if we get any 'blocking' hits

#if WITH_PHYSX
	FPhysXShapeAdaptor ShapeAdaptor(Rot, CollisionShape);
	const PxGeometry& PGeom = ShapeAdaptor.GetGeometry();
	const PxQuat& PGeomRot = ShapeAdaptor.GetGeomOrientation();

	const FVector Delta = End - Start;
	const float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
		// Create filter data used to filter collisions
		PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, false);
		PxSceneQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxSceneQueryFilterFlag::ePREFILTER | PxSceneQueryFilterFlag::ePOSTFILTER);
		PxSceneQueryFlags POutputFlags; 

		FPxQueryFilterCallbackSweep PQueryCallbackSweep(Params);
		PQueryCallbackSweep.bIgnoreTouches = true; // pre-filter to ignore touches and only get blocking hits.

		PxTransform PStartTM(U2PVector(Start), PGeomRot);
		PxVec3 PDir = U2PVector(Delta/DeltaMag);

		FPhysScene* PhysScene = World->GetPhysicsScene();
		{
			// Enable scene locks, in case they are required
			PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);
			SCOPED_SCENE_READ_LOCK(SyncScene);
			PxSweepHit PQueryHit;
			bHaveBlockingHit = SyncScene->sweepAny(PGeom, PStartTM, PDir, DeltaMag, POutputFlags, PQueryHit, PQueryFilterData, &PQueryCallbackSweep);
		}

		// Test async scene if async tests are requested and there was no blocking hit was found in the sync scene (since no hit info other than a boolean yes/no is recorded)
		if( !bHaveBlockingHit && Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
		{
			PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);
			SCOPED_SCENE_READ_LOCK(AsyncScene);
			PxSweepHit PTestHit;
			bHaveBlockingHit = AsyncScene->sweepAny(PGeom, PStartTM, PDir, DeltaMag, POutputFlags, PTestHit, PQueryFilterData, &PQueryCallbackSweep);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if((World->DebugDrawTraceTag != NAME_None) && (World->DebugDrawTraceTag == Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		DrawGeomSweeps(World, Start, End, PGeom, PGeomRot, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording)
	{
		TArray<FHitResult> Hits;
		CAPTUREGEOMSWEEP(World, Start, End, PGeomRot, ECAQueryMode::Test, PGeom, TraceChannel, Params, ResponseParams, ObjectParams, Hits);
	}
#endif // ENABLE_COLLISION_ANALYZER

#endif // WITH_PHYSX

	//@TODO: BOX2D: Implement GeomSweepTest

	return bHaveBlockingHit;
}

bool GeomSweepSingle(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FHitResult& OutHit, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepSingle);
	STARTQUERYTIMER();

	OutHit = FHitResult();
	OutHit.TraceStart = Start;
	OutHit.TraceEnd = End;

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bHaveBlockingHit = false;

#if WITH_PHYSX
	FPhysXShapeAdaptor ShapeAdaptor(Rot, CollisionShape);
	const PxGeometry& PGeom = ShapeAdaptor.GetGeometry();
	const PxQuat& PGeomRot = ShapeAdaptor.GetGeomOrientation();

	const FVector Delta = End - Start;
	const float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
		// Create filter data used to filter collisions
		PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, false);
		//UE_LOG(LogCollision, Log, TEXT("PFilter: %x %x %x %x"), PFilter.word0, PFilter.word1, PFilter.word2, PFilter.word3);
		PxSceneQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxSceneQueryFilterFlag::ePREFILTER);
		PxSceneQueryFlags POutputFlags = PxSceneQueryFlag::ePOSITION | PxSceneQueryFlag::eNORMAL | PxSceneQueryFlag::eDISTANCE | PxSceneQueryFlag::eMTD;
		FPxQueryFilterCallbackSweep PQueryCallbackSweep(Params);
		PQueryCallbackSweep.bIgnoreTouches = true; // pre-filter to ignore touches and only get blocking hits.

		PxTransform PStartTM(U2PVector(Start), PGeomRot);
		PxVec3 PDir = U2PVector(Delta/DeltaMag);

		FPhysScene* PhysScene = World->GetPhysicsScene();
		PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);

		// Enable scene locks, in case they are required
		FScopedMultiSceneReadLock SceneLocks;
		SceneLocks.LockRead(SyncScene, PST_Sync);

		PxSweepHit PHit;
		bHaveBlockingHit = SyncScene->sweepSingle(PGeom, PStartTM, PDir, DeltaMag, POutputFlags, PHit, PQueryFilterData, &PQueryCallbackSweep);

		if (!bHaveBlockingHit)
		{
			// Not using anything from this scene, so unlock it.
			SceneLocks.UnlockRead(SyncScene, PST_Sync);
		}

		// Test async scene if async tests are requested
		if( Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
		{
			PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);
			SceneLocks.LockRead(AsyncScene, PST_Async);

			bool bHaveBlockingHitAsync;
			PxSweepHit PHitAsync;
			bHaveBlockingHitAsync = AsyncScene->sweepSingle(PGeom, PStartTM, PDir, DeltaMag, POutputFlags, PHitAsync, PQueryFilterData, &PQueryCallbackSweep);

			// If we have a blocking hit in the async scene and there was no sync blocking hit, or if the async blocking hit came first,
			// then this becomes the blocking hit.  We can test the PxSceneQueryImpactHit::distance since the DeltaMag is the same for both queries.
			if( bHaveBlockingHitAsync && (!bHaveBlockingHit || PHitAsync.distance < PHit.distance) )
			{
				PHit = PHitAsync;
				bHaveBlockingHit = true;
			}
			else
			{
				// Not using anything from this scene, so unlock it.
				SceneLocks.UnlockRead(AsyncScene, PST_Async);
			}
		}

		if(bHaveBlockingHit) // If we got a hit, convert it to unreal type
		{
			if (ConvertQueryImpactHit(World, PHit, OutHit, DeltaMag, PFilter, Start, End, &PGeom, PStartTM, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Invalid)
			{
				bHaveBlockingHit = false;
				UE_LOG(LogCollision, Error, TEXT("GeomSweepSingle resulted in a NaN/INF in PHit!"));
#if ENABLE_NAN_DIAGNOSTIC
				UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if((World->DebugDrawTraceTag != NAME_None) && (World->DebugDrawTraceTag == Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(OutHit);
		}
		DrawGeomSweeps(World, Start, End, PGeom, PGeomRot, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording)
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(OutHit);
		}
		CAPTUREGEOMSWEEP(World, Start, End, PGeomRot, ECAQueryMode::Single, PGeom, TraceChannel, Params, ResponseParams, ObjectParams, Hits);
	}
#endif

#endif // WITH_PHYSX

	//@TODO: BOX2D: Implement GeomSweepSingle

	return bHaveBlockingHit;
}

bool GeomSweepMulti_PhysX(const UWorld* World, const PxGeometry& PGeom, const PxQuat& PGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepMultiple);
	STARTQUERYTIMER();
	bool bBlockingHit = false;

	const int32 InitialHitCount = OutHits.Num();

	// Create filter data used to filter collisions
	PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, true);
	PxSceneQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxSceneQueryFilterFlag::ePREFILTER | PxSceneQueryFilterFlag::ePOSTFILTER);
	PxSceneQueryFlags POutputFlags = PxSceneQueryFlag::ePOSITION | PxSceneQueryFlag::eNORMAL | PxSceneQueryFlag::eDISTANCE | PxSceneQueryFlag::eMTD;
	FPxQueryFilterCallbackSweep PQueryCallbackSweep(Params);

	const FVector Delta = End - Start;
	const float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
		FPhysScene* PhysScene = World->GetPhysicsScene();
		PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);

		// Lock scene
		FScopedMultiSceneReadLock SceneLocks;
		SceneLocks.LockRead(SyncScene, PST_Sync);

		const PxTransform PStartTM(U2PVector(Start), PGeomRot);
		const PxVec3 PDir = U2PVector(Delta / DeltaMag);

		// Keep track of closest blocking hit distance.
		float MinBlockDistance = DeltaMag;

		// Create buffer for hits. Note: memory is not initialized (for perf reasons), since API does not require it.
		TTypeCompatibleBytes<PxSweepHit> RawPHits[HIT_BUFFER_SIZE];
		PxSweepHit* PHits = (PxSweepHit*)RawPHits;

		bool bBlockingHitSync;
		PxI32 NumHits = SyncScene->sweepMultiple(PGeom, PStartTM, PDir, DeltaMag, POutputFlags, PHits, HIT_BUFFER_MAX_SYNC_QUERIES, bBlockingHitSync, PQueryFilterData, &PQueryCallbackSweep);
		if (NumHits == -1)
		{
			UE_LOG(LogCollision, Warning, TEXT("GeomSweepMulti : Buffer overflow from synchronous scene query"));
			UE_LOG(LogCollision, Warning, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
			UE_LOG(LogCollision, Warning, TEXT("--------%s"), *Params.ToString());
			// sweepMultiple still fills in the buffer with the blocking hit and an arbitrary subset of nearer touches
			NumHits = HIT_BUFFER_MAX_SYNC_QUERIES;
		}

		if (bBlockingHitSync)
		{
			/** If we got a blocking hit, it will be the last in the results - get the blocking distance */
			MinBlockDistance = PHits[NumHits - 1].distance;
			bBlockingHit = true;
		}
		else if (NumHits == 0)
		{
			// Not using anything from this scene, so unlock it.
			SceneLocks.UnlockRead(SyncScene, PST_Sync);
		}

		// Test async scene if async tests are requested and there was no overflow
		if (Params.bTraceAsyncScene && MinBlockDistance > SMALL_NUMBER && PhysScene->HasAsyncScene())
		{
			PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);
			SceneLocks.LockRead(AsyncScene, PST_Async);

			// Write into the same PHits buffer
			PxSweepHit* PHitsAsync = PHits + NumHits;
			const PxI32 AsyncBufferSize = (ARRAY_COUNT(RawPHits) - NumHits);
			check(AsyncBufferSize > 0);
			bool bBlockingHitAsync = false;
			PxI32 NumAsyncHits = AsyncScene->sweepMultiple(PGeom, PStartTM, PDir, MinBlockDistance, POutputFlags, PHitsAsync, AsyncBufferSize, bBlockingHitAsync, PQueryFilterData, &PQueryCallbackSweep);
			if (NumAsyncHits == -1)
			{
				UE_LOG(LogCollision, Log, TEXT("GeomSweepMulti : Buffer overflow from asynchronous scene query"));
				UE_LOG(LogCollision, Warning, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Warning, TEXT("--------%s"), *Params.ToString());
				// sweepMultiple still fills in the buffer with the blocking hit and an arbitrary subset of nearer touches
				NumAsyncHits = AsyncBufferSize;
			}
			else if (NumAsyncHits == 0)
			{
				// Not using anything from this scene, so unlock it.
				SceneLocks.UnlockRead(AsyncScene, PST_Async);
			}

			NumHits += NumAsyncHits;

			if (bBlockingHitAsync)
			{
				MinBlockDistance = FMath::Min<float>(PHits[NumHits - 1].distance, MinBlockDistance);
				bBlockingHit = true;
			}
		}

		// Convert all hits to unreal structs. This will remove any hits further than MinBlockDistance, and sort results.
		if (NumHits > 0)
		{
			if (AddSweepResults(bBlockingHit, World, NumHits, PHits, DeltaMag, PFilter, OutHits, Start, End, PGeom, PStartTM, MinBlockDistance, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Invalid)
			{
				// We don't need to change bBlockingHit, that's done by AddSweepResults if it removed the blocking hit.
				UE_LOG(LogCollision, Error, TEXT("GeomSweepMulti resulted in a NaN/INF in PHit!"));
#if ENABLE_NAN_DIAGNOSTIC				
				UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ((World->DebugDrawTraceTag != NAME_None) && (World->DebugDrawTraceTag == Params.TraceTag) && IsInGameThread())
	{
		TArray<FHitResult> OnlyMyHits(OutHits);
		OnlyMyHits.RemoveAt(0, InitialHitCount, false); // Remove whatever was there initially.
		DrawGeomSweeps(World, Start, End, PGeom, PGeomRot, OnlyMyHits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording)
	{
		TArray<FHitResult> OnlyMyHits(OutHits);
		OnlyMyHits.RemoveAt(0, InitialHitCount, false); // Remove whatever was there initially.
		CAPTUREGEOMSWEEP(World, Start, End, PGeomRot, ECAQueryMode::Multi, PGeom, TraceChannel, Params, ResponseParams, ObjectParams, OnlyMyHits);
	}
#endif // ENABLE_COLLISION_ANALYZER

	return bBlockingHit;
}


bool GeomSweepMulti(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepMultiple);

	OutHits.Reset();

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bBlockingHit = false;

#if WITH_PHYSX
	FPhysXShapeAdaptor ShapeAdaptor(Rot, CollisionShape);
	const PxGeometry& PGeom = ShapeAdaptor.GetGeometry();
	const PxQuat& PGeomRot = ShapeAdaptor.GetGeomOrientation();

	bBlockingHit = GeomSweepMulti_PhysX(World, PGeom, PGeomRot, OutHits, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
#endif // WITH_PHYSX

	//@TODO: BOX2D: Implement GeomSweepMulti

	return bBlockingHit;
}

//////////////////////////////////////////////////////////////////////////
// GEOM OVERLAP

#if WITH_PHYSX

namespace EQueryInfo
{
	//This is used for templatizing code based on the info we're trying to get out.
	enum Type
	{
		GatherAll,		//get all data and actually return it
		IsBlocking,		//is any of the data blocking? only return a bool so don't bother collecting
		IsAnything		//is any of the data blocking or touching? only return a bool so don't bother collecting
	};
}

template <EQueryInfo::Type InfoType>
bool GeomOverlapMultiImp_PhysX(const UWorld* World, const PxGeometry& PGeom, const PxTransform& PGeomPose, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapMultiple);
	STARTQUERYTIMER();

	bool bHaveBlockingHit = false;

	// overlapMultiple only supports sphere/capsule/box 
	if (PGeom.getType()==PxGeometryType::eSPHERE || PGeom.getType()==PxGeometryType::eCAPSULE || PGeom.getType()==PxGeometryType::eBOX || PGeom.getType()==PxGeometryType::eCONVEXMESH )
	{
		// Create filter data used to filter collisions
		PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, InfoType != EQueryInfo::IsAnything);
		PxSceneQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxSceneQueryFilterFlag::ePREFILTER);
		FPxQueryFilterCallback PQueryCallback(Params);
		PQueryCallback.bIgnoreTouches = (InfoType == EQueryInfo::IsBlocking); // pre-filter to ignore touches and only get blocking hits, if that's what we're after.

		// Enable scene locks, in case they are required
		FScopedMultiSceneReadLock SceneLocks;
		FPhysScene* PhysScene = World->GetPhysicsScene();
		PxScene* SyncScene = PhysScene->GetPhysXScene(PST_Sync);

		// we can't use scoped because we later do a conversion which depends on these results and it should all be atomic
		SceneLocks.LockRead(SyncScene, PST_Sync);

		// Create buffer for hits. Note: memory is not initialized (for perf reasons), since API does not require it.
		TTypeCompatibleBytes<PxOverlapHit> RawPOverlapArray[OVERLAP_BUFFER_SIZE];
		PxOverlapHit* POverlapArray = (PxOverlapHit*)RawPOverlapArray;
		
		PxOverlapHit POverlapResult;	//IsAnything code path only needs one so we use this instead of array. Compiler will optimize out the buffer since code won't use it

		PxI32 NumHits = 0;
		
		if ((InfoType == EQueryInfo::IsAnything) || (InfoType == EQueryInfo::IsBlocking))
		{
			if (SyncScene->overlapAny(PGeom, PGeomPose, POverlapResult, PQueryFilterData, &PQueryCallback))
			{
				return true;
			}
		}
		else
		{
			checkSlow(InfoType == EQueryInfo::GatherAll);
			NumHits = SyncScene->overlapMultiple(PGeom, PGeomPose, POverlapArray, OVERLAP_BUFFER_SIZE_MAX_SYNC_QUERIES, PQueryFilterData, &PQueryCallback);
			if (NumHits == -1)
			{
				UE_LOG(LogCollision, Warning, TEXT("GeomOverlapMulti : Buffer overflow from synchronous scene query"));
				UE_LOG(LogCollision, Warning, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Warning, TEXT("--------%s"), *Params.ToString());
				// overlapMultiple still fills in the buffer with an arbitrary set of overlapping objects, so we have a full buffer of results
				// we can use, though that is not the entire set
				NumHits = OVERLAP_BUFFER_SIZE_MAX_SYNC_QUERIES;
			}
			else if (NumHits == 0)
			{
				// Not using anything from this scene, so unlock it.
				SceneLocks.UnlockRead(SyncScene, PST_Sync);
			}
		}

		// Test async scene if async tests are requested and there was no overflow
		if (Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
		{		
			PxScene* AsyncScene = PhysScene->GetPhysXScene(PST_Async);
			
			// we can't use scoped because we later do a conversion which depends on these results and it should all be atomic
			SceneLocks.LockRead(AsyncScene, PST_Async);
		
			if ((InfoType == EQueryInfo::IsAnything) || (InfoType == EQueryInfo::IsBlocking))
			{
				if (AsyncScene->overlapAny(PGeom, PGeomPose, POverlapResult, PQueryFilterData, &PQueryCallback))
				{
					return true;
				}
			}
			else
			{
				checkSlow(InfoType == EQueryInfo::GatherAll);
				// Write into the same PHits buffer
				PxOverlapHit* PAsyncOverlapArray = POverlapArray + NumHits;
				const PxI32 AsyncBufferSize = (ARRAY_COUNT(RawPOverlapArray) - NumHits);
				check(AsyncBufferSize > 0);

				PxI32 NumAsyncHits = AsyncScene->overlapMultiple(PGeom, PGeomPose, PAsyncOverlapArray, AsyncBufferSize, PQueryFilterData, &PQueryCallback);

				if (NumAsyncHits == -1)
				{
					UE_LOG(LogCollision, Log, TEXT("GeomOverlapMulti : Buffer overflow from asynchronous scene query"));
					UE_LOG(LogCollision, Warning, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
					UE_LOG(LogCollision, Warning, TEXT("--------%s"), *Params.ToString());
					// overlapMultiple still fills in the buffer with an arbitrary set of overlapping objects, so we have a full buffer of results
					// we can use, though that is not the entire set
					NumAsyncHits = AsyncBufferSize;
				}
				else if (NumAsyncHits == 0)
				{
					// Not using anything from this scene, so unlock it.
					SceneLocks.UnlockRead(AsyncScene, PST_Async);
				}

				NumHits += NumAsyncHits;
			}
		}

		if (InfoType == EQueryInfo::GatherAll)	//if we are gathering all we need to actually convert to UE format
		{
			if (NumHits > 0)
			{
				bHaveBlockingHit = ConvertOverlapResults(NumHits, POverlapArray, PFilter, OutOverlaps);
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if ((World->DebugDrawTraceTag != NAME_None) && (World->DebugDrawTraceTag == Params.TraceTag) && IsInGameThread())
			{
				DrawGeomOverlaps(World, PGeom, PGeomPose, OutOverlaps, DebugLineLifetime);
			}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}

#if ENABLE_COLLISION_ANALYZER
		if (GCollisionAnalyzerIsRecording)
		{
			// Determine query mode ('single' doesn't really exist for overlaps)
			ECAQueryMode::Type QueryMode = (InfoType == EQueryInfo::GatherAll)  ? ECAQueryMode::Multi : ECAQueryMode::Test;

			CAPTUREGEOMOVERLAP(World, PGeom, PGeomPose, QueryMode, TraceChannel, Params, ResponseParams, ObjectParams, OutOverlaps);
		}
#endif // ENABLE_COLLISION_ANALYZER
	}
	else
	{
		UE_LOG(LogCollision, Log, TEXT("GeomOverlapMulti : unsupported shape - only supports sphere, capsule, box"));
	}

	return bHaveBlockingHit;
}

bool GeomOverlapMulti_PhysX(const UWorld* World, const PxGeometry& PGeom, const PxTransform& PGeomPose, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	return GeomOverlapMultiImp_PhysX<EQueryInfo::GatherAll>(World, PGeom, PGeomPose, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}

#endif

template <EQueryInfo::Type InfoType>
bool GeomOverlapMultiImp(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bHaveBlockingHit = false;

#if WITH_PHYSX
	FPhysXShapeAdaptor ShapeAdaptor(Rot, CollisionShape);
	const PxGeometry& PGeom = ShapeAdaptor.GetGeometry();
	const PxTransform& PGeomPose = ShapeAdaptor.GetGeomPose(Pos);
	bHaveBlockingHit = GeomOverlapMultiImp_PhysX<InfoType>(World, PGeom, PGeomPose, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);

#endif // WITH_PHYSX

	//@TODO: BOX2D: Implement GeomOverlapMulti

	return bHaveBlockingHit;
}

bool GeomOverlapMulti(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	OutOverlaps.Reset();
	return GeomOverlapMultiImp<EQueryInfo::GatherAll>(World, CollisionShape, Pos, Rot, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}

bool GeomOverlapBlockingTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	TArray<FOverlapResult> Overlaps;	//needed only for template shared code
	return GeomOverlapMultiImp<EQueryInfo::IsBlocking>(World, CollisionShape, Pos, Rot, Overlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}

bool GeomOverlapAnyTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	TArray<FOverlapResult> Overlaps;	//needed only for template shared code
	return GeomOverlapMultiImp<EQueryInfo::IsAnything>(World, CollisionShape, Pos, Rot, Overlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}
#endif //UE_WITH_PHYSICS

//////////////////////////////////////////////////////////////////////////

#if WITH_PHYSX

static const PxQuat CapsuleRotator(0.f, 0.707106781f, 0.f, 0.707106781f );

PxQuat ConvertToPhysXCapsuleRot(const FQuat& GeomRot)
{
	// Rotation required because PhysX capsule points down X, we want it down Z
	return U2PQuat(GeomRot) * CapsuleRotator;
}

FQuat ConvertToUECapsuleRot(const PxQuat & PGeomRot)
{
	return P2UQuat(PGeomRot * CapsuleRotator.getConjugate());
}

PxTransform ConvertToPhysXCapsulePose(const FTransform& GeomPose)
{
	PxTransform PFinalPose;

	PFinalPose.p = U2PVector(GeomPose.GetTranslation());
	// Rotation required because PhysX capsule points down X, we want it down Z
	PFinalPose.q = ConvertToPhysXCapsuleRot(GeomPose.GetRotation());
	return PFinalPose;
}

//////////////////////////////////////////////////////////////////////////

PxFilterData CreateObjectQueryFilterData(const bool bTraceComplex, const int32 MultiTrace/*=1 if multi. 0 otherwise*/, const struct FCollisionObjectQueryParams & ObjectParam)
{
	/**
	 * Format for QueryData : 
	 *		word0 (meta data - ECollisionQuery. Extendable)
	 *
	 *		For object queries
	 *
	 *		word1 (object type queries)
	 *		word2 (unused)
	 *		word3 (Multi (1) or single (0) (top 8) + Flags (lower 24))
	 */

	PxFilterData PNewData;

	PNewData.word0 = ECollisionQuery::ObjectQuery;

	if (bTraceComplex)
	{
		PNewData.word3 |= EPDF_ComplexCollision;
	}
	else
	{
		PNewData.word3 |= EPDF_SimpleCollision;
	}

	// get object param bits
	PNewData.word1 = ObjectParam.GetQueryBitfield();

	// if 'nothing', then set no bits
	PNewData.word3 |= CreateChannelAndFilter((ECollisionChannel)MultiTrace, ObjectParam.IgnoreMask);

	return PNewData;
}

PxFilterData CreateTraceQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer& InCollisionResponseContainer, const FCollisionQueryParams& Params)
{
	/**
	 * Format for QueryData : 
	 *		word0 (meta data - ECollisionQuery. Extendable)
	 *
	 *		For trace queries
	 *
	 *		word1 (blocking channels)
	 *		word2 (touching channels)
	 *		word3 (MyChannel (top 8) as ECollisionChannel + Flags (lower 24))
	 */

	PxFilterData PNewData;

	PNewData.word0 = ECollisionQuery::TraceQuery;

	if (bTraceComplex)
	{
		PNewData.word3 |= EPDF_ComplexCollision;
	}
	else
	{
		PNewData.word3 |= EPDF_SimpleCollision;
	}

	// word1 encodes 'what i block', word2 encodes 'what i touch'
	for(int32 i=0; i<ARRAY_COUNT(InCollisionResponseContainer.EnumArray); i++)
	{
		if(InCollisionResponseContainer.EnumArray[i] == ECR_Block)
		{
			// if i block, set that in word1
			PNewData.word1 |= CRC_TO_BITFIELD(i);
		}
		else if(InCollisionResponseContainer.EnumArray[i] == ECR_Overlap)
		{
			// if i touch, set that in word2
			PNewData.word2 |= CRC_TO_BITFIELD(i);
		}
	}
	
	// if 'nothing', then set no bits
	PNewData.word3 |= CreateChannelAndFilter((ECollisionChannel)MyChannel, Params.IgnoreMask);

	return PNewData;
}

/** Utility for creating a PhysX PxFilterData for performing a query (trace) against the scene */
PxFilterData CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer& InCollisionResponseContainer, const struct FCollisionQueryParams& QueryParam, const struct FCollisionObjectQueryParams & ObjectParam, const bool bMultitrace)
{
	if (ObjectParam.IsValid() )
	{
		return CreateObjectQueryFilterData(bTraceComplex, (bMultitrace? TRACE_MULTI : TRACE_SINGLE), ObjectParam);
	}
	else
	{
		return CreateTraceQueryFilterData(MyChannel, bTraceComplex, InCollisionResponseContainer, QueryParam);
	}
}

PxGeometry * GetGeometryFromShape(GeometryFromShapeStorage & LocalStorage, const PxShape * PShape, bool bTriangleMeshAllowed /*= false*/)
{
	switch (PShape->getGeometryType())
	{
	case PxGeometryType::eSPHERE:
		PShape->getSphereGeometry(LocalStorage.SphereGeom);
		return &LocalStorage.SphereGeom;
	case PxGeometryType::eBOX:
		PShape->getBoxGeometry(LocalStorage.BoxGeom);
		return &LocalStorage.BoxGeom;
	case PxGeometryType::eCAPSULE:
		PShape->getCapsuleGeometry(LocalStorage.CapsuleGeom);
		return &LocalStorage.CapsuleGeom;
	case PxGeometryType::eCONVEXMESH:
		PShape->getConvexMeshGeometry(LocalStorage.ConvexGeom);
		return &LocalStorage.ConvexGeom;
	case PxGeometryType::eTRIANGLEMESH:
		if (bTriangleMeshAllowed)
		{
			PShape->getTriangleMeshGeometry(LocalStorage.TriangleGeom);
			return &LocalStorage.TriangleGeom;
		}
	case PxGeometryType::eHEIGHTFIELD:
		PShape->getHeightFieldGeometry(LocalStorage.HeightFieldGeom);
		return &LocalStorage.HeightFieldGeom;
	default:
		return NULL;
	}
}

#endif // WITH_PHYSX
