// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PhysicsEngine/PhysicsCollisionHandler.h"
#include "PhyaCollisionHandler.generated.h"

class UAudioComponent;
class USoundWaveProcedural;
struct FBodyInstance;
struct FCollisionNotifyInfo;

struct FPhyaBodyInstancePair
{
	FBodyInstance* Body0; // lower pointer
	FBodyInstance* Body1; // higher pointer

	FPhyaBodyInstancePair(FBodyInstance* InBody0, FBodyInstance* InBody1)
		: Body0(InBody0)
		, Body1(InBody1)
	{}

	friend bool operator==(const FPhyaBodyInstancePair& X, const FPhyaBodyInstancePair& Y)
	{
		return X.Body0 == Y.Body0 && X.Body1 == Y.Body1;
	}
};

class FPhyaPairInfo : public TSharedFromThis<FPhyaPairInfo>
{

};

UCLASS()
class UPhyaCollisionHandler : public UPhysicsCollisionHandler
{
	GENERATED_UCLASS_BODY()

	//~ Begin UPhysicsCollisionHandler Interface
	virtual void InitCollisionHandler() override;
	virtual void HandlePhysicsCollisions_AssumesLocked(TArray<FCollisionNotifyInfo>& PendingCollisionNotifies) override;
	//~ End UPhysicsCollisionHandler Interface

	void TestImpact();

	void ProceduralWaveUnderflow(USoundWaveProcedural* InProceduralWave, int32 SamplesRequired);

	UPROPERTY()
	UAudioComponent* AudioComp;

	UPROPERTY()
	USoundWaveProcedural* ProceduralWave;

	TMap< FPhyaBodyInstancePair, TSharedPtr<FPhyaPairInfo> >	PairHash;

	float LastTestImpactTime;

	class paWhiteFun* Whitefun;
	class paFunSurface* Whitesurf;

	class paModalData* ModalData;
	TArray<class paBody*> Bodies;
};
