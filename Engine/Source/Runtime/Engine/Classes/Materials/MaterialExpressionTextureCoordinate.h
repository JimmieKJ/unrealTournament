// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionTextureCoordinate.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionTextureCoordinate : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Texture coordinate index */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureCoordinate)
	int32 CoordinateIndex;

	/** Controls how much the texture tiles horizontally, by scaling the U component of the vertex UVs by the specified amount. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureCoordinate)
	float UTiling;

	/** Controls how much the texture tiles vertically, by scaling the V component of the vertex UVs by the specified amount. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureCoordinate)
	float VTiling;

	/** Would like to unmirror U or V 
	 *  - if the texture is mirrored and if you would like to undo mirroring for this texture sample, use this to unmirror */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureCoordinate)
	uint32 UnMirrorU:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureCoordinate)
	uint32 UnMirrorV:1;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



