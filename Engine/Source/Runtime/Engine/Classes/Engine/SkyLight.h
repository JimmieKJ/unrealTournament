// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameFramework/Info.h"
#include "SkyLight.generated.h"

UCLASS(ClassGroup=Lights, hidecategories=(Input,Collision,Replication,Info), showcategories=("Rendering", "Input|MouseInput", "Input|TouchInput"), ComponentWrapperClass, ConversionRoot, Blueprintable)
class ENGINE_API ASkyLight : public AInfo
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** @todo document */
	DEPRECATED_FORGAME(4.6, "LightComponent should not be accessed directly, please use GetLightComponent() function instead. LightComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = Light, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Light,Rendering,Rendering|Components|SkyLight", AllowPrivateAccess = "true"))
	class USkyLightComponent* LightComponent;
public:

	/** replicated copy of LightComponent's bEnabled property */
	UPROPERTY(replicatedUsing=OnRep_bEnabled)
	uint32 bEnabled:1;

	/** Replication Notification Callbacks */
	UFUNCTION()
	virtual void OnRep_bEnabled();

	/** Returns LightComponent subobject **/
	class USkyLightComponent* GetLightComponent() const;
};



