// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Particles/Location/ParticleModuleLocationBase.h"
#include "ParticleModuleLocationBoneSocket.generated.h"

UENUM()
enum ELocationBoneSocketSource
{
	BONESOCKETSOURCE_Bones,
	BONESOCKETSOURCE_Sockets,
	BONESOCKETSOURCE_MAX,
};

UENUM()
enum ELocationBoneSocketSelectionMethod
{
	BONESOCKETSEL_Sequential,
	BONESOCKETSEL_Random,
	BONESOCKETSEL_MAX,
};

USTRUCT()
struct FLocationBoneSocketInfo
{
	GENERATED_USTRUCT_BODY()

	/** The name of the bone/socket on the skeletal mesh */
	UPROPERTY(EditAnywhere, Category=LocationBoneSocketInfo)
	FName BoneSocketName;

	/** The offset from the bone/socket to use */
	UPROPERTY(EditAnywhere, Category=LocationBoneSocketInfo)
	FVector Offset;


	FLocationBoneSocketInfo()
		: Offset(ForceInit)
	{
	}

};

UCLASS(editinlinenew, hidecategories=Object, meta=(DisplayName = "Bone/Socket Location"))
class UParticleModuleLocationBoneSocket : public UParticleModuleLocationBase
{
	GENERATED_UCLASS_BODY()

	/**
	 *	Whether the module uses Bones or Sockets for locations.
	 *
	 *	BONESOCKETSOURCE_Bones		- Use Bones as the source locations.
	 *	BONESOCKETSOURCE_Sockets	- Use Sockets as the source locations.
	 */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	TEnumAsByte<enum ELocationBoneSocketSource> SourceType;

	/** An offset to apply to each bone/socket */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	FVector UniversalOffset;

	/** The name(s) of the bone/socket(s) to position at */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	TArray<struct FLocationBoneSocketInfo> SourceLocations;

	/**
	 *	The method by which to select the bone/socket to spawn at.
	 *
	 *	SEL_Sequential			- loop through the bone/socket array in order
	 *	SEL_Random				- randomly select a bone/socket from the array
	 */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	TEnumAsByte<enum ELocationBoneSocketSelectionMethod> SelectionMethod;

	/** If true, update the particle locations each frame with that of the bone/socket */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	uint32 bUpdatePositionEachFrame:1;

	/** If true, rotate mesh emitter meshes to orient w/ the socket */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	uint32 bOrientMeshEmitters:1;

	/** If true, particles inherit the associated bone velocity when spawned */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	uint32 bInheritBoneVelocity:1;

	/**
	 *	The parameter name of the skeletal mesh actor that supplies the SkelMeshComponent for in-game.
	 */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	FName SkelMeshActorParamName;

#if WITH_EDITORONLY_DATA
	/** The name of the skeletal mesh to use in the editor */
	UPROPERTY(EditAnywhere, Category=BoneSocket)
	class USkeletalMesh* EditorSkelMesh;

#endif // WITH_EDITORONLY_DATA

	// Begin UParticleModule Interface
	virtual void	Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void	Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual void	FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual uint32	RequiredBytes(FParticleEmitterInstance* Owner = NULL) override;
	virtual uint32	RequiredBytesPerInstance(FParticleEmitterInstance* Owner = NULL) override;
	virtual uint32	PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData) override;
	virtual bool	TouchesMeshRotation() const override { return true; }
	virtual void	AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp) override;
	virtual bool CanTickInAnyThread() override
	{
		return false;
	}
#if WITH_EDITOR
	virtual int32 GetNumberOfCustomMenuOptions() const override;
	virtual bool GetCustomMenuEntryDisplayString(int32 InEntryIndex, FString& OutDisplayString) const override;
	virtual bool PerformCustomMenuEntry(int32 InEntryIndex) override;
#endif
	// End UParticleModule Interface

	/**
	 *	Retrieve the skeletal mesh component source to use for the current emitter instance.
	 *
	 *	@param	Owner						The particle emitter instance that is being setup
	 *
	 *	@return	USkeletalMeshComponent*		The skeletal mesh component to use as the source
	 */
	USkeletalMeshComponent* GetSkeletalMeshComponentSource(FParticleEmitterInstance* Owner);

	/**
	 *	Retrieve the position for the given socket index.
	 *
	 *	@param	Owner					The particle emitter instance that is being setup
	 *	@param	InSkelMeshComponent		The skeletal mesh component to use as the source
	 *	@param	InBoneSocketIndex		The index of the bone/socket of interest
	 *	@param	OutPosition				The position for the particle location
	 *	@param	OutRotation				Optional orientation for the particle (mesh emitters)
	 *	
	 *	@return	bool					true if successful, false if not
	 */
	bool GetParticleLocation(FParticleEmitterInstance* Owner, USkeletalMeshComponent* InSkelMeshComponent, int32 InBoneSocketIndex, FVector& OutPosition, FQuat* OutRotation);
};



