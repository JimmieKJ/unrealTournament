// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Commandlets/Commandlet.h"
#include "ListMaterialsUsedWithMeshEmittersCommandlet.generated.h"

UCLASS()
class UListMaterialsUsedWithMeshEmittersCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()
	
	/** Process the given particle system. For mesh emitters, output material paths that don't have bUsedWithParticleSprites flagged.*/
	void ProcessParticleSystem(UParticleSystem* ParticleSystem , TArray<FString> &OutMaterials);

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface
};