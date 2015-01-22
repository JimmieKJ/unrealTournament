// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PhysxUserData.h"
#include "DestructibleComponent.generated.h"

#if WITH_PHYSX
namespace physx
{
#if WITH_APEX
	namespace apex
	{
		class  NxDestructibleActor;
		struct NxApexDamageEventReportData;
		struct NxApexChunkStateEventData;
	}
#endif
	class PxRigidDynamic;
	class PxRigidActor;
}

/** Mapping info for destructible chunk user data. */
struct FDestructibleChunkInfo
{
	/** Index of this chunk info */
	int32 Index;
	/** Index of the chunk this data belongs to*/
	int32 ChunkIndex;
	/** Component owning this chunk info*/
	TWeakObjectPtr<class UDestructibleComponent> OwningComponent;
	/** Physx actor */
	physx::PxRigidDynamic* Actor;
};
#endif // WITH_PHYSX 

/** Delegate for notification when fracture occurs */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FComponentFractureSignature, const FVector &, HitPoint, const FVector &, HitDirection);

/**
 *	This component holds the physics data for a DestructibleActor.
 *
 *	The USkeletalMesh pointer in the base class (SkinnedMeshComponent) MUST be a DestructibleMesh
 */
UCLASS(ClassGroup=Physics, hidecategories=(Object,Mesh,"Components|SkinnedMesh",Mirroring,Activation,"Components|Activation"), config=Engine, editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API UDestructibleComponent : public USkinnedMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** If set, use this actor's fracture effects instead of the asset's fracture effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DestructibleComponent)
	uint32 bFractureEffectOverride:1;

	/** Fracture effects for each fracture level. Used only if Fracture Effect Override is set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, editfixedsize, Category=DestructibleComponent)
	TArray<struct FFractureEffect> FractureEffects;

	/**
	 *	Enable "hard sleeping" for destruction-generated PxActors.  This means that they turn kinematic
	 *	when they sleep, but can be made dynamic again by application of enough damage.
	 */	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DestructibleComponent)
	bool bEnableHardSleeping;

	/**
	 * The minimum size required to treat chunks as Large.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DestructibleComponent)
	float LargeChunkThreshold;

#if WITH_EDITORONLY_DATA
	/** Provide a blueprint interface for setting the destructible mesh */
	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category=DestructibleComponent)
	class UDestructibleMesh* DestructibleMesh;
#endif // WITH_EDITORONLY_DATA

#if WITH_PHYSX
	/** Per chunk info */
	TIndirectArray<FDestructibleChunkInfo> ChunkInfos;
#endif // WITH_PHYSX 

#if WITH_EDITOR
	// Begin UObject interface.
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void Serialize(FArchive& Ar) override;
	// End UObject interface.

	// Take damage
	UFUNCTION(BlueprintCallable, Category="Components|Destructible")
	void ApplyDamage(float DamageAmount, const FVector& HitLocation, const FVector& ImpulseDir, float ImpulseStrength);

	// Take radius damage
	UFUNCTION(BlueprintCallable, Category="Components|Destructible")
	void ApplyRadiusDamage(float BaseDamage, const FVector& HurtOrigin, float DamageRadius, float ImpulseStrength, bool bFullDamage);

	UFUNCTION(BlueprintCallable, Category="Components|Destructible")
	void SetDestructibleMesh(class UDestructibleMesh* NewMesh);

	UFUNCTION(BlueprintCallable, Category="Components|Destructible")
	class UDestructibleMesh * GetDestructibleMesh();

	/** Called when a component is touched */
	UPROPERTY(BlueprintAssignable, Category = "Components|Destructible")
	FComponentFractureSignature OnComponentFracture;

public:
#if WITH_APEX
	/** The NxDestructibleActor instantated from an NxDestructibleAsset, which contains the runtime physical state. */
	physx::apex::NxDestructibleActor* ApexDestructibleActor;
#endif	//WITH_APEX

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void OnUpdateTransform(bool bSkipPhysicsMove) override;
	virtual void Activate(bool bReset=false) override;
	virtual void Deactivate() override;
	// End USceneComponent interface.

	// Begin UActorComponent interface.
	virtual void CreatePhysicsState() override;
	virtual void DestroyPhysicsState() override;
	virtual class UBodySetup* GetBodySetup() override;
	// End UActorComponent interface.

	// Begin UPrimitiveComponent interface.
	virtual FBodyInstance* GetBodyInstance(FName BoneName = NAME_None, bool bGetWelded = true) const override;
	virtual bool CanEditSimulatePhysics() override;
	virtual bool IsAnySimulatingPhysics() const override;

	virtual void AddImpulse(FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false) override;
	virtual void AddImpulseAtLocation(FVector Impulse, FVector Position, FName BoneName = NAME_None) override;
	virtual void AddForce(FVector Force, FName BoneName = NAME_None) override;
	virtual void AddForceAtLocation(FVector Force, FVector Location, FName BoneName = NAME_None) override;
	virtual void AddRadialImpulse(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange=false) override;
	virtual void AddRadialForce(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff) override;
	virtual void ReceiveComponentDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual bool LineTraceComponent( FHitResult& OutHit, const FVector Start, const FVector End, const FCollisionQueryParams& Params ) override;
	virtual bool SweepComponent( FHitResult& OutHit, const FVector Start, const FVector End, const FCollisionShape& CollisionShape, bool bTraceComplex=false) override;
	virtual void SetEnableGravity(bool bGravityEnabled) override;
	// End UPrimitiveComponent interface.

	// Begin SkinnedMeshComponent interface.
	virtual bool ShouldUpdateTransform(bool bLODHasChanged) const override;
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = NULL) override;
	virtual void SetSkeletalMesh(USkeletalMesh* InSkelMesh) override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	// End SkinnedMeshComponent interface.


	// Begin DestructibleComponent interface.
#if WITH_APEX
	struct FFakeBodyInstanceState
	{
		physx::PxRigidActor* ActorSync;
		physx::PxRigidActor* ActorAsync;
		int32 InstanceIndex;
	};

	/** Changes the body instance to have the specified actor and instance id. */
	void SetupFakeBodyInstance(physx::PxRigidActor* NewRigidActor, int32 InstanceIdx, FFakeBodyInstanceState* PrevState = NULL);
	
	/** Resets the BodyInstance to the state that is defined in PrevState. */
	void ResetFakeBodyInstance(FFakeBodyInstanceState& PrevState);
#endif // WITH_APEX

	/** This method makes a chunk (fractured piece) visible or invisible.
	 *
	 * @param ChunkIndex - Which chunk to affect.  ChunkIndex must lie in the range: 0 <= ChunkIndex < ((DestructibleMesh*)USkeletalMesh)->ApexDestructibleAsset->chunkCount().
	 * @param bVisible - If true, the chunk will be made visible.  Otherwise, the chunk is made invisible.
	 */
	void SetChunkVisible( int32 ChunkIndex, bool bVisible );

#if WITH_APEX
	/** This method takes a collection of active actors and updates the chunks in one pass. Saves a lot of duplicate work instead of calling each individual chunk
	 * 
	 *  @param ActiveActors - The array of actors that need their transforms updated
	 *
     */
	static void UpdateDestructibleChunkTM(TArray<const physx::PxRigidActor*> ActiveActors);
#endif


	/** This method sets a chunk's (fractured piece's) world rotation and translation.
	 *
	 * @param ChunkIndex - Which chunk to affect.  ChunkIndex must lie in the range: 0 <= ChunkIndex < ((DestructibleMesh*)USkeletalMesh)->ApexDestructibleAsset->chunkCount().
	 * @param WorldRotation - The orienation to give to the chunk in world space, represented as a quaternion.
	 * @param WorldRotation - The world space position to give to the chunk.
	 */
	void SetChunkWorldRT( int32 ChunkIndex, const FQuat& WorldRotation, const FVector& WorldTranslation );

#if WITH_APEX
	/** Trigger any fracture effects after a damage event is recieved */
	void SpawnFractureEffectsFromDamageEvent(const physx::apex::NxApexDamageEventReportData& InDamageEvent);

	/** Callback from physics system to notify the actor that it has been damaged */
	void OnDamageEvent(const physx::apex::NxApexDamageEventReportData& InDamageEvent);

	/** Callback from physics system to notify the actor that a chunk's visibilty has changed */
	void OnVisibilityEvent(const physx::apex::NxApexChunkStateEventData & InDamageEvent);
#endif // WITH_APEX

	// End DestructibleComponent interface.

	virtual bool DoCustomNavigableGeometryExport(struct FNavigableGeometryExport* GeomExport) const override;

	FORCEINLINE static int32 ChunkIdxToBoneIdx(int32 ChunkIdx) { return ChunkIdx + 1; }
	FORCEINLINE static int32 BoneIdxToChunkIdx(int32 BoneIdx) { return FMath::Max(BoneIdx - 1, 0); }
private:

	struct FUpdateChunksInfo
	{
		int32 ChunkIndex;
		FTransform WorldTM;

		FUpdateChunksInfo(int32 InChunkIndex, const FTransform& InWorldTM) : ChunkIndex(InChunkIndex), WorldTM(InWorldTM){}

	};

	void SetChunksWorldTM(const TArray<FUpdateChunksInfo>& UpdateInfos);

	/** Collision response used for chunks */
	FCollisionResponse LargeChunkCollisionResponse;
	FCollisionResponse SmallChunkCollisionResponse;
#if WITH_PHYSX
	/** User data wrapper for this component passed to physx */
	FPhysxUserData PhysxUserData;

	/** User data wrapper for the chunks passed to physx */
	TIndirectArray<FPhysxUserData> PhysxChunkUserData;
	bool IsChunkLarge(int32 ChunkIdx) const;
	void SetCollisionResponseForActor(physx::PxRigidDynamic* Actor, int32 ChunkIdx);
#endif
};



