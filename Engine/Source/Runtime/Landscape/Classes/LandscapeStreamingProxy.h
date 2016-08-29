// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeStreamingProxy.generated.h"

UCLASS(MinimalAPI, notplaceable, NotBlueprintable)
class ALandscapeStreamingProxy : public ALandscapeProxy
{
	GENERATED_BODY()

public:
	ALandscapeStreamingProxy(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, Category=LandscapeProxy)
	TLazyObjectPtr<ALandscape> LandscapeActor;

	//~ Begin UObject Interface
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//~ Begin ALandscapeBase Interface
	virtual ALandscape* GetLandscapeActor() override;
#if WITH_EDITOR
	virtual UMaterialInterface* GetLandscapeMaterial() const override;
	virtual UMaterialInterface* GetLandscapeHoleMaterial() const override;
#endif
	//~ End ALandscapeBase Interface

	// Check input Landscape actor is match for this LandscapeProxy (by GUID)
	bool IsValidLandscapeActor(ALandscape* Landscape);
};
