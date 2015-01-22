// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * This UV node generates texture coordinates in view space centered on the particle system's MacroUVPosition, with tiling controlled by the particle system's MacroUVRadius.
 * It is useful for mapping a 'macro' noise texture in a continuous manner onto all particles of a particle system.
 */

#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionParticleMacroUV.generated.h"

UCLASS()
class UMaterialExpressionParticleMacroUV : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



