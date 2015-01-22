// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "GameFramework/Info.h"
#include "AtmosphericFog.generated.h"

/** 
 *	A placeable fog actor that simulates atmospheric light scattering  
 *	@see https://docs.unrealengine.com/latest/INT/Engine/Actors/FogEffects/AtmosphericFog/index.html
 */
UCLASS(showcategories=(Movement, Rendering, "Utilities|Transformation", "Input|MouseInput", "Input|TouchInput"), ClassGroup=Fog, hidecategories=(Info,Object,Input), MinimalAPI)
class AAtmosphericFog : public AInfo
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Main fog component */
	DEPRECATED_FORGAME(4.6, "AtmosphericFogComponent should not be accessed directly, please use GetAtmosphericFogComponent() function instead. AtmosphericFogComponent will soon be private and your code will not compile.")
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Atmosphere, meta = (AllowPrivateAccess = "true"))
	class UAtmosphericFogComponent* AtmosphericFogComponent;

#if WITH_EDITORONLY_DATA
	/** Arrow component to indicate default sun rotation */
	DEPRECATED_FORGAME(4.6, "ArrowComponent should not be accessed directly, please use GetArrowComponent() function instead. ArrowComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UArrowComponent* ArrowComponent;
#endif

public:

#if WITH_EDITOR
	virtual void PostActorCreated() override;
#endif

	/** Returns AtmosphericFogComponent subobject **/
	ENGINE_API class UAtmosphericFogComponent* GetAtmosphericFogComponent();
#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	ENGINE_API class UArrowComponent* GetArrowComponent();
#endif
};





