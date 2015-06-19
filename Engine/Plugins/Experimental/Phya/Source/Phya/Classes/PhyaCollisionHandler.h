// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhyaCollisionHandler.generated.h"

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

	// Begin UPhysicsCollisionHandler interface
	virtual void InitCollisionHandler() override;
	virtual void HandlePhysicsCollisions_AssumesLocked(const TArray<FCollisionNotifyInfo>& PendingCollisionNotifies) override;
	// End UPhysicsCollisionHandler interface

	void TestImpact();

	void StreamingWaveUnderflow(USoundWaveStreaming* InStreamingWave, int32 SamplesRequired);

	UPROPERTY()
	UAudioComponent* AudioComp;

	UPROPERTY()
	USoundWaveStreaming* StreamingWave;

	TMap< FPhyaBodyInstancePair, TSharedPtr<FPhyaPairInfo> >	PairHash;

	float LastTestImpactTime;

	class paWhiteFun* Whitefun;
	class paFunSurface* Whitesurf;

	class paModalData* ModalData;
	TArray<class paBody*> Bodies;
};
