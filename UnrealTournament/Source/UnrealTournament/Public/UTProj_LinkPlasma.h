// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_LinkPlasma.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTProj_LinkPlasma : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkBolt)
	float OverlapSphereGrowthRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkBolt)
	float MaxOverlapSphereSize;

	virtual void Tick(float DeltaTime) override;
};
