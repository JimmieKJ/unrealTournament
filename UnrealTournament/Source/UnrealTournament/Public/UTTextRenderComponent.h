// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/TextRenderComponent.h"
#include "UTTextRenderComponent.generated.h"

UCLASS(CustomConstructor, ClassGroup = Rendering, hidecategories = (Object, LOD, Physics, TextureStreaming, Activation, "Components|Activation", Collision), editinlinenew, meta = (BlueprintSpawnableComponent = ""))
class UNREALTOURNAMENT_API UUTTextRenderComponent : public UTextRenderComponent
{
	GENERATED_UCLASS_BODY()

	UUTTextRenderComponent(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}
	virtual bool NeedsLoadForServer() const override
	{
		return true;
	}
};