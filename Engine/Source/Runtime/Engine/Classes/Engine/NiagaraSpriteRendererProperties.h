// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEffectRendererProperties.h"
#include "NiagaraSpriteRendererProperties.generated.h"

UCLASS()
class UNiagaraSpriteRendererProperties : public UNiagaraEffectRendererProperties
{
public:
	GENERATED_UCLASS_BODY()

	UNiagaraSpriteRendererProperties()
	{
		SubImageInfo = FVector2D(1.0f, 1.0f);
	}

	UPROPERTY(EditAnywhere, Category = "Sprite Rendering")
	FVector2D SubImageInfo;

	UPROPERTY(EditAnywhere, Category = "Sprite Rendering")
	bool bBVelocityAligned;
};
