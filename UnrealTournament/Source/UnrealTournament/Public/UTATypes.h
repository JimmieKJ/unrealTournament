// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.generated.h"

UENUM()
namespace EGameStage
{
	enum Type
	{
		Initializing,
		PreGame, 
		GameInProgress,
		GameOver,
		MAX,
	};
}

UENUM()
namespace ETextHorzPos
{
	enum Type
	{
		Left,
		Center, 
		Right,
		MAX,
	};
}

UENUM()
namespace ETextVertPos
{
	enum Type
	{
		Top,
		Center,
		Bottom,
		MAX,
	};
}

USTRUCT(BlueprintType)
struct FTextureUVs
{

	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureUVs)
	float U;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureUVs)
	float V;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureUVs)
	float UL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureUVs)
	float VL;

	FTextureUVs() : U(0), V(0), UL(0), VL(0){};

	FTextureUVs(float inU, float inV, float inUL, float inVL)
		: U(inU)
		, V(inV)
		, UL(inUL)
		, VL(inVL)
	{};

};