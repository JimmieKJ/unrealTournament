// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTCTFFlagBase.generated.h"

UCLASS()
class AUTCTFFlagBase : public AUTGameObjective
{
	GENERATED_UCLASS_BODY()

	// Holds a reference to the flag
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = GameObject)
	AUTCTFFlag* MyFlag;

	// The mesh that makes up this base.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GameObject)
	TSubobjectPtr<UStaticMeshComponent> Mesh;

	virtual FName GetFlagState();
	virtual AUTPlayerState* GetFlagHolder();
	virtual void RecallFlag();

protected:
	virtual void CreateCarriedObject();
};