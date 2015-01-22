// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VectorFieldVolume: Volume encompassing a vector field.
=============================================================================*/

#pragma once

#include "VectorFieldVolume.generated.h"

UCLASS(hidecategories=(Object, Advanced, Collision), MinimalAPI)
class AVectorFieldVolume : public AActor
{
	GENERATED_UCLASS_BODY()

private_subobject:
	DEPRECATED_FORGAME(4.6, "VectorFieldComponent should not be accessed directly, please use GetVectorFieldComponent() function instead. VectorFieldComponent will soon be private and your code will not compile.")
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = VectorFieldVolume, meta = (AllowPrivateAccess = "true"))
	class UVectorFieldComponent* VectorFieldComponent;

#if WITH_EDITORONLY_DATA
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
#endif

public:
	/** Returns VectorFieldComponent subobject **/
	ENGINE_API class UVectorFieldComponent* GetVectorFieldComponent() const;
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const;
#endif
};



