// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	UMaterialExpressionParticleMotionBlurFade: Exposes a value used to fade
		particle sprites under the effects of motion blur.
==============================================================================*/

#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionParticleRandom.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionParticleRandom: public UMaterialExpression
{
	GENERATED_UCLASS_BODY()


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



