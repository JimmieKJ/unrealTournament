// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 *	Utility class designed to allow you to connect a MaterialInterface to a Matinee action.
 */

#pragma once
#include "MaterialInstanceActor.generated.h"

UCLASS(hidecategories=Movement, hidecategories=Advanced, hidecategories=Collision, hidecategories=Display, hidecategories=Actor, hidecategories=Attachment, MinimalAPI)
class AMaterialInstanceActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Pointer to actors that we want to control paramters of using Matinee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialInstanceActor)
	TArray<class AActor*> TargetActors;

#if WITH_EDITORONLY_DATA
private_subobject:
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	// Reference to actor sprite
	UBillboardComponent* SpriteComponent;
#endif

public:

	// Begin UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface

#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const;
#endif
};



