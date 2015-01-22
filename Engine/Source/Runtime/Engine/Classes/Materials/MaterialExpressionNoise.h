// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionNoise.generated.h"

UENUM()
enum ENoiseFunction
{
	/** fast (~94 instructions per level) */
	NOISEFUNCTION_Simplex UMETA(DisplayName="Simplex"),
	/** fast (~77 instructions per level) but low quality*/
	NOISEFUNCTION_Perlin UMETA(DisplayName="Perlin"),
	/** very slow (~393 instructions per level) */
	NOISEFUNCTION_Gradient UMETA(DisplayName="Gradient"),
	/** very fast (1 texture lookup, ~33 instructions per level), need to test more on every hardware, requires high quality texture filtering for bump mapping */
	NOISEFUNCTION_FastGradient UMETA(DisplayName="FastGradient"),
	NOISEFUNCTION_MAX,
};

UCLASS()
class UMaterialExpressionNoise : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** 2 to 3 dimensional vector */
	UPROPERTY()
	FExpressionInput Position;

	/** scalar, to clamp the Levels at pixel level, can be computed like this: max(length(ddx(Position)), length(ddy(Position)) */
	UPROPERTY()
	FExpressionInput FilterWidth;

	/** can also be done with a multiply on the Position */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionNoise)
	float Scale;

	/** 0 = fast, allows to pick a different implementation, can affect performance and look */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionNoise, meta=(UIMin = "1", UIMax = "10"))
	int32 Quality;

	/** Noise function, affects performance and look */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionNoise, meta=(DisplayName = "Function"))
	TEnumAsByte<enum ENoiseFunction> NoiseFunction;
	
	/** How multiple frequencies are getting combined */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionNoise)
	uint32 bTurbulence:1;

	/** 1 = fast but little detail, .. larger numbers cost more performance, only used for turbulence */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionNoise, meta=(UIMin = "1", UIMax = "10"))
	int32 Levels;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionNoise)
	float OutputMin;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionNoise)
	float OutputMax;

	/** usually 2 but higher values allow efficient use of few levels */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionNoise, meta=(UIMin = "2", UIMax = "8"))
	float LevelScale;

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



