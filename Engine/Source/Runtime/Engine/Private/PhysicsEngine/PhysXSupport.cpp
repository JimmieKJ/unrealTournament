// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysXSupport.cpp: PhysX
=============================================================================*/

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/ConvexElem.h"

#if WITH_PHYSX

#include "PhysXSupport.h"
#include "Components/DestructibleComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"

PxFoundation*			GPhysXFoundation = NULL;
PxProfileZoneManager*	GPhysXProfileZoneManager = NULL;
PxPhysics*				GPhysXSDK = NULL;
#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
PxCooking*				GPhysXCooking = NULL;
#endif
FPhysXAllocator*		GPhysXAllocator = NULL;

#if WITH_APEX
ENGINE_API NxApexSDK*				GApexSDK = NULL;
ENGINE_API NxModuleDestructible*	GApexModuleDestructible = NULL;	

#if WITH_APEX_LEGACY
ENGINE_API NxModule*				GApexModuleLegacy = NULL;	
#endif // WITH_APEX_LEGACY

#if WITH_APEX_CLOTHING
ENGINE_API NxModuleClothing*		GApexModuleClothing		= NULL;	
#endif //WITH_APEX_CLOTHING

TMap<int32, NxApexScene*>				GPhysXSceneMap;
FApexNullRenderResourceManager		GApexNullRenderResourceManager;
FApexResourceCallback				GApexResourceCallback;
FApexPhysX3Interface				GApexPhysX3Interface;
FApexChunkReport					GApexChunkReport;
#else	// #if WITH_APEX
TMap<int32, PxScene*>		GPhysXSceneMap;
#endif	// #if WITH_APEX

int32						GNumPhysXConvexMeshes = 0;

TArray<PxConvexMesh*>	GPhysXPendingKillConvex;
TArray<PxTriangleMesh*>	GPhysXPendingKillTriMesh;
TArray<PxHeightField*>	GPhysXPendingKillHeightfield;
TArray<PxMaterial*>		GPhysXPendingKillMaterial;

///////////////////// Unreal to PhysX conversion /////////////////////

PxTransform UMatrix2PTransform(const FMatrix& UTM)
{
	PxQuat PQuat = U2PQuat(UTM.ToQuat());
	PxVec3 PPos = U2PVector(UTM.GetOrigin());

	PxTransform Result(PPos, PQuat);

	return Result;
}

PxTransform U2PTransform(const FTransform& UTransform)
{
	PxQuat PQuat = U2PQuat(UTransform.GetRotation());
	PxVec3 PPos = U2PVector(UTransform.GetLocation());

	PxTransform Result(PPos, PQuat);

	return Result;
}

PxVec3 U2PVector(const FVector& UVec)
{
	return PxVec3(UVec.X, UVec.Y, UVec.Z);
}

PxQuat U2PQuat(const FQuat& UQuat)
{
	return PxQuat( UQuat.X, UQuat.Y, UQuat.Z, UQuat.W );
}

PxMat44 U2PMatrix(const FMatrix& UTM)
{
	PxMat44 Result;

	const physx::PxMat44 *MatPtr = (const physx::PxMat44 *)(&UTM);
	Result = *MatPtr;

	return Result;
}

PxPlane U2PPlane(FPlane& Plane)
{
	return PxPlane(Plane.X, Plane.Y, Plane.Z, -Plane.W);
}

UCollision2PGeom::UCollision2PGeom(const FCollisionShape& CollisionShape)
{
	switch (CollisionShape.ShapeType)
	{
		case ECollisionShape::Box:
		{
			new (Storage)PxBoxGeometry(U2PVector(CollisionShape.GetBox()));
			break;
		}
		case ECollisionShape::Sphere:
		{
			new (Storage)PxSphereGeometry(CollisionShape.GetSphereRadius());
			break;
		}
		case ECollisionShape::Capsule:
		{
			new (Storage)PxCapsuleGeometry(CollisionShape.GetCapsuleRadius(), CollisionShape.GetCapsuleAxisHalfLength());
			break;
		}
		default:
			// invalid point
			ensure(false);
	}
}

///////////////////// PhysX to Unreal conversion /////////////////////

FMatrix P2UMatrix(const PxMat44& PMat)
{
	FMatrix Result;
	// we have to use Memcpy instead of typecasting, becuase PxMat44's are not aligned like FMatrix is
	FMemory::Memcpy(&Result, &PMat, sizeof(PMat));
	return Result;
}

FMatrix PTransform2UMatrix(const PxTransform& PTM)
{
	FQuat UQuat = P2UQuat(PTM.q);
	FVector UPos = P2UVector(PTM.p);

	FMatrix Result = FQuatRotationTranslationMatrix(UQuat, UPos);

	return Result;
}

FTransform P2UTransform(const PxTransform& PTM)
{
	FQuat UQuat = P2UQuat(PTM.q);
	FVector UPos = P2UVector(PTM.p);

	FTransform Result = FTransform(UQuat, UPos);

	return Result;
}

FVector P2UVector(const PxVec3& PVec)
{
	return FVector(PVec.x, PVec.y, PVec.z);
}

FQuat P2UQuat(const PxQuat& PQuat)
{
	return FQuat(PQuat.x, PQuat.y, PQuat.z, PQuat.w);
}

FPlane P2UPlane(PxReal P[4])
{
	return FPlane(P[0], P[1], P[2], -P[3]);
}

FPlane P2UPlane(PxPlane& Plane)
{
	return FPlane(Plane.n.x, Plane.n.y, Plane.n.z, -Plane.d);
}
///////////////////// Utils /////////////////////


#if WITH_APEX

PxScene* GetPhysXSceneFromIndex(int32 InSceneIndex)
{
	NxApexScene** ScenePtr = GPhysXSceneMap.Find(InSceneIndex);
	if(ScenePtr != NULL)
	{
		return (*ScenePtr)->getPhysXScene();
	}

	return NULL;
}

NxApexScene* GetApexSceneFromIndex(int32 InSceneIndex)
{
	NxApexScene** ScenePtr = GPhysXSceneMap.Find(InSceneIndex);
	if(ScenePtr != NULL)
	{
		return *ScenePtr;
	}

	return NULL;
}

#else	// #if WITH_APEX

PxScene* GetPhysXSceneFromIndex(int32 InSceneIndex)
{
	PxScene** ScenePtr = GPhysXSceneMap.Find(InSceneIndex);
	if(ScenePtr != NULL)
	{
		return *ScenePtr;
	}

	return NULL;
}

#endif	// #if WITH_APEX


void AddRadialImpulseToPxRigidBody_AssumesLocked(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange)
{
#if WITH_PHYSX
	if (!(PRigidBody.getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
	{
		float Mass = PRigidBody.getMass();
		PxTransform PCOMTransform = PRigidBody.getGlobalPose().transform(PRigidBody.getCMassLocalPose());
		PxVec3 PCOMPos = PCOMTransform.p; // center of mass in world space
		PxVec3 POrigin = U2PVector(Origin); // origin of radial impulse, in world space
		PxVec3 PDelta = PCOMPos - POrigin; // vector from origin to COM

		float Mag = PDelta.magnitude(); // Distance from COM to origin, in Unreal scale : @todo: do we still need conversion scale?

		// If COM is outside radius, do nothing.
		if (Mag > Radius)
		{
			return;
		}

		PDelta.normalize();

		// Scale by U2PScale here, because units are velocity * mass. 
		float ImpulseMag = Strength;
		if (Falloff == RIF_Linear)
		{
			ImpulseMag *= (1.0f - (Mag / Radius));
		}

		PxVec3 PImpulse = PDelta * ImpulseMag;

		PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
		PRigidBody.addForce(PImpulse, Mode);
	}
#endif // WITH_PHYSX
}

void AddRadialForceToPxRigidBody_AssumesLocked(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange)
{
#if WITH_PHYSX
	if (!(PRigidBody.getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
	{
		float Mass = PRigidBody.getMass();
		PxTransform PCOMTransform = PRigidBody.getGlobalPose().transform(PRigidBody.getCMassLocalPose());
		PxVec3 PCOMPos = PCOMTransform.p; // center of mass in world space
		PxVec3 POrigin = U2PVector(Origin); // origin of radial impulse, in world space
		PxVec3 PDelta = PCOMPos - POrigin; // vector from

		float Mag = PDelta.magnitude(); // Distance from COM to origin, in Unreal scale : @todo: do we still need conversion scale?

		// If COM is outside radius, do nothing.
		if (Mag > Radius)
		{
			return;
		}

		PDelta.normalize();

		// If using linear falloff, scale with distance.
		float ForceMag = Strength;
		if (Falloff == RIF_Linear)
		{
			ForceMag *= (1.0f - (Mag / Radius));
		}

		// Apply force
		PxVec3 PImpulse = PDelta * ForceMag;
		PRigidBody.addForce(PImpulse, bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE);
	}
#endif // WITH_PHYSX
}

bool IsRigidBodyNonKinematic_AssumesLocked(const PxRigidBody* PRigidBody)
{
	if (PRigidBody)
	{
		return !(PRigidBody->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// PHYSXSIMFILTERSHADER 

/** Util to return a string for the type of a query (for debugging) */
FString ObjTypeToString(PxFilterObjectAttributes PAtt)
{
	PxFilterObjectType::Enum Type = PxGetFilterObjectType(PAtt);

	if(Type == PxFilterObjectType::eRIGID_STATIC)
	{
		return TEXT("rigid static");
	}
	else if(Type == PxFilterObjectType::eRIGID_DYNAMIC)
	{
		return TEXT("rigid dynamic");
	}

	return TEXT("unknown");
}

PxFilterFlags PhysXSimFilterShader(	PxFilterObjectAttributes attributes0, PxFilterData filterData0, 
									PxFilterObjectAttributes attributes1, PxFilterData filterData1,
									PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize )
{
	//UE_LOG(LogPhysics, Log, TEXT("filterData0 (%s): %x %x %x %x"), *ObjTypeToString(attributes0), filterData0.word0, filterData0.word1, filterData0.word2, filterData0.word3);
	//UE_LOG(LogPhysics, Log, TEXT("filterData1 (%s): %x %x %x %x"), *ObjTypeToString(attributes1), filterData1.word0, filterData1.word1, filterData1.word2, filterData1.word3);


	bool k0 = PxFilterObjectIsKinematic(attributes0);
	bool k1 = PxFilterObjectIsKinematic(attributes1);

	// Find out which channels the objects are in
	ECollisionChannel Channel0 = (ECollisionChannel)(filterData0.word3 >> 24);
	ECollisionChannel Channel1 = (ECollisionChannel)(filterData1.word3 >> 24);

	// ignore kinematic-kinematic interactions which don't involve a destructible
	if(k0 && k1 && (Channel0 != ECC_Destructible) && (Channel1 != ECC_Destructible))
	{
		return PxFilterFlag::eKILL;
	}

	bool s0 = PxGetFilterObjectType(attributes0) == PxFilterObjectType::eRIGID_STATIC;
	bool s1 = PxGetFilterObjectType(attributes1) == PxFilterObjectType::eRIGID_STATIC;

	//ignore static-kinematic (this assumes that statics can't be flagged as kinematics)
	// should return eSUPPRESS here instead eKILL so that kinematics vs statics will still be considered once kinematics become dynamic (dying ragdoll case)
	if((k0 || k1) && (s0 || s1))
	{
		return PxFilterFlag::eSUPPRESS;
	}

	// if these bodies are from the same skeletal mesh component, use the disable table to see if we should disable collision
	if((filterData0.word2 == filterData1.word2) && (filterData0.word2 != 0))
	{
		check(constantBlockSize == sizeof(FPhysSceneShaderInfo));
		const FPhysSceneShaderInfo* PhysSceneShaderInfo = (const FPhysSceneShaderInfo*) constantBlock;
		check(PhysSceneShaderInfo);
		FPhysScene * PhysScene = PhysSceneShaderInfo->PhysScene;
		check(PhysScene);

		const TMap<uint32, TMap<FRigidBodyIndexPair, bool> *> & CollisionDisableTableLookup = PhysScene->GetCollisionDisableTableLookup();
		TMap<FRigidBodyIndexPair, bool>* const * DisableTablePtrPtr = CollisionDisableTableLookup.Find(filterData1.word2);
		if (DisableTablePtrPtr)		//Since collision table is deferred during sub-stepping it's possible that we won't get the collision disable table until the next frame
		{
			TMap<FRigidBodyIndexPair, bool>* DisableTablePtr = *DisableTablePtrPtr;
			FRigidBodyIndexPair BodyPair(filterData0.word0, filterData1.word0); // body indexes are stored in word 0
			if (DisableTablePtr->Find(BodyPair))
			{
				return PxFilterFlag::eKILL;
			}

		}
	}
	
	// see if 0/1 would like to block the other 
	PxU32 BlockFlagTo1 = (ECC_TO_BITFIELD(Channel1) & filterData0.word1);
	PxU32 BlockFlagTo0 = (ECC_TO_BITFIELD(Channel0) & filterData1.word1);

	bool bDoesWantToBlock = (BlockFlagTo1 && BlockFlagTo0);

	// if don't want to block, suppress
	if ( !bDoesWantToBlock )
	{
		return PxFilterFlag::eSUPPRESS;
	}

	PxU32 FilterFlags0 = (filterData0.word3 & 0xFFFFFF);
	PxU32 FilterFlags1 = (filterData1.word3 & 0xFFFFFF);

	pairFlags = PxPairFlag::eCONTACT_DEFAULT;

	//todo enabling CCD objects against everything else for now
	if(!(k0 && k1) && ((FilterFlags0&EPDF_CCD) || (FilterFlags1&EPDF_CCD)))
	{
		pairFlags |= PxPairFlag::eCCD_LINEAR;
	}


	if((FilterFlags0&EPDF_ContactNotify) || (FilterFlags1&EPDF_ContactNotify))
	{
		pairFlags |= (PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_CONTACT_POINTS );
	}


	if ((FilterFlags0&EPDF_ModifyContacts) || (FilterFlags1&EPDF_ModifyContacts))
	{
		pairFlags |= (PxPairFlag::eMODIFY_CONTACTS);
	}

	return PxFilterFlags();
}

///////// FPhysXSimEventCallback //////////////////////////////////

void FPhysXSimEventCallback::onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NumPairs)
{
	// Check actors are not destroyed
	if( PairHeader.flags & (PxContactPairHeaderFlag::eDELETED_ACTOR_0 | PxContactPairHeaderFlag::eDELETED_ACTOR_1) )
	{
		UE_LOG(LogPhysics, Log, TEXT("%d onContact(): Actors have been deleted!"), GFrameCounter );
		return;
	}

	const PxActor* PActor0 = PairHeader.actors[0];
	const PxActor* PActor1 = PairHeader.actors[1];
	check(PActor0 && PActor1);

	const PxRigidBody* PRigidBody0 = PActor0->isRigidBody();
	const PxRigidBody* PRigidBody1 = PActor1->isRigidBody();

	const FBodyInstance* BodyInst0 = FPhysxUserData::Get<FBodyInstance>(PActor0->userData);
	const FBodyInstance* BodyInst1 = FPhysxUserData::Get<FBodyInstance>(PActor1->userData);
	
	bool bEitherDestructible = false;

	// check if it's a destructible actor
	if (BodyInst0 == NULL)
	{
		if (const FDestructibleChunkInfo* DestructibleChunkInfo = FPhysxUserData::Get<FDestructibleChunkInfo>(PActor0->userData))
		{
			bEitherDestructible = true;
			BodyInst0 = DestructibleChunkInfo->OwningComponent.IsValid() ? &DestructibleChunkInfo->OwningComponent->BodyInstance : NULL;
		}
	}

	if (BodyInst1 == NULL)
	{
		if (const FDestructibleChunkInfo* DestructibleChunkInfo = FPhysxUserData::Get<FDestructibleChunkInfo>(PActor1->userData))
		{
			bEitherDestructible = true;
			BodyInst1 = DestructibleChunkInfo->OwningComponent.IsValid() ? &DestructibleChunkInfo->OwningComponent->BodyInstance : NULL;
		}
	}

	//if nothing valid just exit
	//if a destructible mesh you can get chunks that hit other chunks from the same body... this causes a lot of spam and doesn't seem like a very useful notification so I'm turning it off
	if(BodyInst0 == NULL || BodyInst1 == NULL || BodyInst0 == BodyInst1)
	{
		return;
	}

	//destruction applies damage when it hits something. Unfortunately it relies on the same flag that generates onContact.
	//We only want onContact events to happen if the user actually selected bNotifyRigidBodyCollision so we have to check if this is the case
	if (bEitherDestructible)
	{
		if (BodyInst0->bNotifyRigidBodyCollision == false && BodyInst1->bNotifyRigidBodyCollision == false)
		{
			return;
		}
	}

	// Get the Scene. 
	const PxScene* PScene = PActor0->getScene();
	check(PScene == PActor1->getScene());
	FPhysScene* PhysScene = FPhysxUserData::Get<FPhysScene>(PScene->userData);
	check(PhysScene);

	const EPhysicsSceneType SceneType = PhysScene->GetPhysXScene(PST_Sync) == PScene ? PST_Sync : PST_Async;
	TArray<FCollisionNotifyInfo>& PendingCollisionNotifies = PhysScene->GetPendingCollisionNotifies(SceneType);

	uint32 PreAddingCollisionNotify = PendingCollisionNotifies.Num() - 1;
	TArray<int32> PairNotifyMapping = FBodyInstance::AddCollisionNotifyInfo(BodyInst0, BodyInst1, Pairs, NumPairs, PendingCollisionNotifies);

	// Iterate through contact points
	for(uint32 PairIdx=0; PairIdx<NumPairs; PairIdx++)
	{
		int32 NotifyIdx = PairNotifyMapping[PairIdx];
		if (NotifyIdx == -1)	//the body instance this pair belongs to is not listening for events
		{
			continue;
		}

		FCollisionNotifyInfo * NotifyInfo = &PendingCollisionNotifies[NotifyIdx];
		FCollisionImpactData* ImpactInfo = &(NotifyInfo->RigidCollisionData);

		const PxContactPair* Pair = Pairs + PairIdx;

		// Get the two shapes that are involved in the collision
		const PxShape* Shape0 = Pair->shapes[0];
		check(Shape0);
		const PxShape* Shape1 = Pair->shapes[1];
		check(Shape1);

		// Get materials
		PxMaterial* Material0 = NULL;
		Shape0->getMaterials(&Material0, 1);
		UPhysicalMaterial* PhysMat0 = (Material0 != NULL) ? FPhysxUserData::Get<UPhysicalMaterial>(Material0->userData) : NULL;

		PxMaterial* Material1 = NULL;
		Shape1->getMaterials(&Material1, 1);
		UPhysicalMaterial* PhysMat1 = (Material1 != NULL) ? FPhysxUserData::Get<UPhysicalMaterial>(Material1->userData) : NULL;

		// Iterate over contact points
		PxContactPairPoint ContactPointBuffer[16];
		int32 NumContactPoints = Pair->extractContacts(ContactPointBuffer, 16);
		for(int32 PointIdx=0; PointIdx<NumContactPoints; PointIdx++)
		{
			const PxContactPairPoint& Point = ContactPointBuffer[PointIdx];

			const PxVec3 NormalImpulse = Point.impulse.dot(Point.normal) * Point.normal; // project impulse along normal
			ImpactInfo->TotalNormalImpulse += P2UVector(NormalImpulse);
			ImpactInfo->TotalFrictionImpulse += P2UVector(Point.impulse - NormalImpulse); // friction is component not along contact normal

			new(ImpactInfo->ContactInfos) FRigidBodyContactInfo(
				P2UVector(Point.position), 
				P2UVector(Point.normal), 
				-1.f * Point.separation, 
				PhysMat0, 
				PhysMat1);
		}	
	}

	for (int32 NotifyIdx = PreAddingCollisionNotify + 1; NotifyIdx < PendingCollisionNotifies.Num(); NotifyIdx++)
	{
		FCollisionNotifyInfo * NotifyInfo = &PendingCollisionNotifies[NotifyIdx];
		FCollisionImpactData* ImpactInfo = &(NotifyInfo->RigidCollisionData);
		// Discard pairs that don't generate any force (eg. have been rejected through a modify contact callback).
		if (ImpactInfo->TotalNormalImpulse.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			PendingCollisionNotifies.RemoveAt(NotifyIdx);
			NotifyIdx--;
		}
	}
}

void FPhysXSimEventCallback::onConstraintBreak( PxConstraintInfo* constraints, PxU32 count )
{
	for (int32 i=0; i < (int32)count; ++i)
	{
		PxJoint* Joint = (PxJoint*)(constraints[i].externalReference);

		if (Joint && Joint->userData)
		{
			FConstraintInstance* Constraint = FPhysxUserData::Get<FConstraintInstance>(Joint->userData);

			if (Constraint)
			{
				Constraint->OnConstraintBroken();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FPhysXCPUDispatcher


class FPhysXTask
{
	PxBaseTask&	Task;

public:
	FPhysXTask(PxBaseTask* InTask)
		: Task(*InTask)
	{		
	}

	~FPhysXTask()
	{
		Task.release();
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysXTask, STATGROUP_Physics);
	}
	static ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode() 
	{ 
		return ESubsequentsMode::TrackSubsequents; 
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FPlatformMisc::BeginNamedEvent(FColor::Black, Task.getName());
		Task.run();
		FPlatformMisc::EndNamedEvent();
	}
};

void FPhysXCPUDispatcher::submitTask( PxBaseTask& Task ) 
{
	TGraphTask<FPhysXTask>::CreateTask(NULL).ConstructAndDispatchWhenReady(&Task);
}

PxU32 FPhysXCPUDispatcher::getWorkerCount() const 
{
	return FTaskGraphInterface::Get().GetNumWorkerThreads();
}


DECLARE_CYCLE_STAT(TEXT("PhysX Single Thread Task"),STAT_PhysXSingleThread,STATGROUP_Physics);

static TArray<PxBaseTask*> TaskStack;

void FPhysXCPUDispatcherSingleThread::submitTask( PxBaseTask& Task ) 
{
	SCOPE_CYCLE_COUNTER(STAT_PhysXSingleThread);
	check(IsInGameThread());

	TaskStack.Push(&Task);
	if (TaskStack.Num() > 1)
	{
		return;
	}
	Task.run();
	Task.release();
	while (TaskStack.Num() > 1)
	{
		PxBaseTask& ChildTask = *TaskStack.Pop();
		ChildTask.run();
		ChildTask.release();
	}
	verify(&Task == TaskStack.Pop() && !TaskStack.Num());
}

PxU32 FPhysXCPUDispatcherSingleThread::getWorkerCount() const 
{
	return 1;
}

//////////////////////////////////////////////////////////////////////////
// FPhysXFormatDataReader

FPhysXFormatDataReader::FPhysXFormatDataReader( FByteBulkData& InBulkData )
	: TriMesh( NULL )
	, TriMeshNegX( NULL )
{
	// Read cooked physics data
	uint8* DataPtr = (uint8*)InBulkData.Lock( LOCK_READ_ONLY );
	FBufferReader Ar( DataPtr, InBulkData.GetBulkDataSize(), false );
	
	uint8 bLittleEndian = true;
	int32 NumConvexElementsCooked = 0;
	int32 NumMirroredElementsCooked = 0;
	uint8 bTriMeshCooked = false;
	uint8 bMirroredTriMeshCooked = false;

	Ar << bLittleEndian;
	Ar.SetByteSwapping( PLATFORM_LITTLE_ENDIAN ? !bLittleEndian : !!bLittleEndian );
	Ar << NumConvexElementsCooked;	
	Ar << NumMirroredElementsCooked;
	Ar << bTriMeshCooked;
	Ar << bMirroredTriMeshCooked;
	
	ConvexMeshes.Empty( NumConvexElementsCooked );
	ConvexMeshesNegX.Empty( NumMirroredElementsCooked );

	for( int32 ElementIndex = 0; ElementIndex < NumConvexElementsCooked; ElementIndex++ )
	{
		PxConvexMesh* ConvexMesh = ReadConvexMesh( Ar, DataPtr, InBulkData.GetBulkDataSize() );
		ConvexMeshes.Add( ConvexMesh );
	}

	for( int32 ElementIndex = 0; ElementIndex < NumMirroredElementsCooked; ElementIndex++ )
	{
		PxConvexMesh* ConvexMeshNegX = ReadConvexMesh( Ar, DataPtr, InBulkData.GetBulkDataSize() );
		ConvexMeshesNegX.Add( ConvexMeshNegX );
	}

	if( bTriMeshCooked )
	{
		TriMesh = ReadTriMesh( Ar, DataPtr, InBulkData.GetBulkDataSize() );
		check(TriMesh);	
	}

	if( bMirroredTriMeshCooked )
	{
		TriMeshNegX = ReadTriMesh( Ar, DataPtr, InBulkData.GetBulkDataSize() );
		check(TriMeshNegX);	
	}

	InBulkData.Unlock();
}

PxConvexMesh* FPhysXFormatDataReader::ReadConvexMesh( FBufferReader& Ar, uint8* InBulkDataPtr, int32 InBulkDataSize )
{
	PxConvexMesh* CookedMesh = NULL;
	uint8 IsMeshCooked = false;
	Ar << IsMeshCooked;
	if( IsMeshCooked )
	{
		FPhysXInputStream Buffer( InBulkDataPtr + Ar.Tell(), InBulkDataSize - Ar.Tell() );		
		CookedMesh = GPhysXSDK->createConvexMesh( Buffer );
		check( CookedMesh != NULL );
		Ar.Seek( Ar.Tell() + Buffer.ReadPos );
	}
	return CookedMesh;
}

PxTriangleMesh* FPhysXFormatDataReader::ReadTriMesh( FBufferReader& Ar, uint8* InBulkDataPtr, int32 InBulkDataSize )
{
	FPhysXInputStream Buffer( InBulkDataPtr + Ar.Tell(), InBulkDataSize - Ar.Tell() );
	PxTriangleMesh* CookedMesh = GPhysXSDK->createTriangleMesh(Buffer);
	check(CookedMesh);
	Ar.Seek( Ar.Tell() + Buffer.ReadPos );
	return CookedMesh;
}

SIZE_T GetPhysxObjectSize(PxBase* Obj, const PxCollection* SharedCollection)
{
	PxSerializationRegistry* Sr = PxSerialization::createSerializationRegistry(*GPhysXSDK);
	PxCollection* Collection = PxCreateCollection();

	Collection->add(*Obj);
	PxSerialization::complete(*Collection, *Sr, SharedCollection);	// chase all other stuff (shared shaps, materials, etc) needed to serialize this collection

	FPhysXCountMemoryStream Out;
	PxSerialization::serializeCollectionToBinary(Out, *Collection, *Sr, SharedCollection);

	Collection->release();
	Sr->release();

	return Out.UsedMemory;
}

#if WITH_APEX
///////// FApexChunkReport //////////////////////////////////

void FApexChunkReport::onDamageNotify(const NxApexDamageEventReportData& damageEvent)
{
	UDestructibleComponent* DestructibleComponent = Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(damageEvent.destructible->userData));
	check(DestructibleComponent);

	if (DestructibleComponent->IsPendingKill())	//don't notify if object is being destroyed
	{
		return;
	}

	DestructibleComponent->OnDamageEvent(damageEvent);
}

void FApexChunkReport::onStateChangeNotify(const NxApexChunkStateEventData& visibilityEvent)
{
	UDestructibleComponent* DestructibleComponent = Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(visibilityEvent.destructible->userData));
	check(DestructibleComponent);

	if (DestructibleComponent->IsPendingKill())	//don't notify if object is being destroyed
	{
		return;
	}

	DestructibleComponent->OnVisibilityEvent(visibilityEvent);
}

bool FApexChunkReport::releaseOnNoChunksVisible(const NxDestructibleActor* destructible)
{
	return false;
}

///////// FApexPhysX3Interface //////////////////////////////////
void FApexPhysX3Interface::setContactReportFlags(physx::PxShape* PShape, physx::PxPairFlags PFlags, NxDestructibleActor* actor, PxU16 actorChunkIndex)
{
	UDestructibleComponent* DestructibleComponent = Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(PShape->userData));
	check(DestructibleComponent);

	DestructibleComponent->Pair(actorChunkIndex, PShape);
}

physx::PxPairFlags FApexPhysX3Interface::getContactReportFlags(const physx::PxShape* PShape) const
{
	PxFilterData FilterData = PShape->getSimulationFilterData();
	return (physx::PxPairFlags)FilterData.word3;
}

#endif	// WITH_APEX


FPhysxSharedData& FPhysxSharedData::Get()
{
	static FPhysxSharedData s_SharedData; 
	return s_SharedData;
}

void FPhysxSharedData::Add( PxBase* Obj )
{
	if(Obj) 
	{ 
		SharedObjects->add(*Obj, (PxSerialObjectId)Obj);
	}
}

void FPhysxSharedData::DumpSharedMemoryUsage(FOutputDevice* Ar)
{
	struct FSharedResourceEntry
	{
		uint64 MemorySize;
		uint64 Count;
	};

	struct FSortBySize
	{
		FORCEINLINE bool operator()( const FSharedResourceEntry& A, const FSharedResourceEntry& B ) const 
		{ 
			// Sort descending
			return B.MemorySize < A.MemorySize;
		}
	};

	TMap<FString, FSharedResourceEntry> AllocationsByType;

	uint64 OverallSize = 0;
	int32 OverallCount = 0;

	TMap<FString, TArray<PxBase*> > ObjectsByType;

	for (int32 i=0; i < (int32)SharedObjects->getNbObjects(); ++i)
	{
		PxBase& Obj = SharedObjects->getObject(i);
		FString TypeName = ANSI_TO_TCHAR(Obj.getConcreteTypeName());

		TArray<PxBase*>* ObjectsArray = ObjectsByType.Find(TypeName);
		if (ObjectsArray == NULL)
		{
			ObjectsByType.Add(TypeName, TArray<PxBase*>());
			ObjectsArray = ObjectsByType.Find(TypeName);
		}

		check(ObjectsArray);
		ObjectsArray->Add(&Obj);
	}

	TArray<FString> TypeNames;
	ObjectsByType.GetKeys(TypeNames);

	for (int32 TypeIdx=0; TypeIdx < TypeNames.Num(); ++TypeIdx)
	{
		const FString& TypeName = TypeNames[TypeIdx];
		
		TArray<PxBase*>* ObjectsArray = ObjectsByType.Find(TypeName);
		check(ObjectsArray);

		PxSerializationRegistry* Sr = PxSerialization::createSerializationRegistry(*GPhysXSDK);
		PxCollection* Collection = PxCreateCollection();
		
		for (int32 i=0; i < ObjectsArray->Num(); ++i)
		{
			Collection->add(*((*ObjectsArray)[i]));;
		}

		PxSerialization::complete(*Collection, *Sr);	// chase all other stuff (shared shaps, materials, etc) needed to serialize this collection

		FPhysXCountMemoryStream Out;
		PxSerialization::serializeCollectionToBinary(Out, *Collection, *Sr);

		Collection->release();
		Sr->release();

		OverallSize += Out.UsedMemory;
		OverallCount += ObjectsArray->Num();

		FSharedResourceEntry NewEntry;
		NewEntry.Count = ObjectsArray->Num();
		NewEntry.MemorySize = Out.UsedMemory;

		AllocationsByType.Add(TypeName, NewEntry);
	}

	Ar->Logf(TEXT(""));
	Ar->Logf(TEXT("Shared Resources:"));
	Ar->Logf(TEXT(""));

	AllocationsByType.ValueSort(FSortBySize());
	
	Ar->Logf(TEXT("%-10d %s (%d)"), OverallSize, TEXT("Overall"), OverallCount );
	
	for( auto It=AllocationsByType.CreateConstIterator(); It; ++It )
	{
		Ar->Logf(TEXT("%-10d %s (%d)"), It.Value().MemorySize, *It.Key(), It.Value().Count );
	}
}

void AddToCollection(PxCollection* PCollection, PxBase* PBase)
{
	if (PBase)
	{
		PCollection->add(*PBase);
	}
}

PxCollection* MakePhysXCollection(const TArray<UPhysicalMaterial*>& PhysicalMaterials, const TArray<UBodySetup*>& BodySetups, uint64 BaseId)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_CreateSharedData);
	PxCollection* PCollection = PxCreateCollection();
	for (UPhysicalMaterial* PhysicalMaterial : PhysicalMaterials)
	{
		if (PhysicalMaterial)
		{
			PCollection->add(*PhysicalMaterial->GetPhysXMaterial());
		}
	}

	for (UBodySetup* BodySetup : BodySetups)
	{
		AddToCollection(PCollection, BodySetup->TriMesh);
		AddToCollection(PCollection, BodySetup->TriMeshNegX);

		for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
		{
			AddToCollection(PCollection, ConvexElem.ConvexMesh);
			AddToCollection(PCollection, ConvexElem.ConvexMeshNegX);
		}
	}

	PxSerialization::createSerialObjectIds(*PCollection, PxSerialObjectId(BaseId));

	return PCollection;
}

#endif // WITH_PHYSX
