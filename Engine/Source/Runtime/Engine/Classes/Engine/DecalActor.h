// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DecalActor.generated.h"


class UBoxComponent;

/**
* DecalActor contains a DecalComponent which can be used to render material modifications on top of existing geometry.
*
* @see https://docs.unrealengine.com/latest/INT/Engine/Actors/DecalActor
* @see UDecalComponent
*/
UCLASS(hideCategories=(Collision, Attachment, Actor, Input, Replication), showCategories=("Input|MouseInput", "Input|TouchInput"), ComponentWrapperClass,MinimalAPI)
class ADecalActor
	: public AActor
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** The decal component for this decal actor */
	DEPRECATED_FORGAME(4.6, "Decal should not be accessed directly, please use GetDecal() function instead. Decal will soon be private and your code will not compile.")
	UPROPERTY(Category = Decal, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Decal,Rendering|Components|Decal", AllowPrivateAccess = "true"))
	class UDecalComponent* Decal;

#if WITH_EDITORONLY_DATA
	/* Reference to the editor only arrow visualization component */
	DEPRECATED_FORGAME(4.6, "ArrowComponent should not be accessed directly, please use GetArrowComponent() function instead. ArrowComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UArrowComponent* ArrowComponent;

	/* Reference to the billboard component */
	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UBillboardComponent* SpriteComponent;

	/* Reference to the selected visualization box component */
	DEPRECATED_FORGAME(4.6, "BoxComponent should not be accessed directly, please use GetBoxComponent() function instead. BoxComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UBoxComponent* BoxComponent;
#endif

public:

	// BEGIN DEPRECATED (use component functions now in level script)
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Decal", meta=(DeprecatedFunction))
	void SetDecalMaterial(class UMaterialInterface* NewDecalMaterial);
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Decal", meta=(DeprecatedFunction))
	class UMaterialInterface* GetDecalMaterial() const;
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Decal", meta=(DeprecatedFunction))
	virtual class UMaterialInstanceDynamic* CreateDynamicMaterialInstance();
	// END DEPRECATED

	
#if WITH_EDITOR
	// Begin UObject Interface
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject Interface

	// Begin AActor interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	// End AActor interface.
#endif // WITH_EDITOR

public:

	/** Returns Decal subobject **/
	ENGINE_API class UDecalComponent* GetDecal() const;
#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	ENGINE_API class UArrowComponent* GetArrowComponent() const;
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const;
	/** Returns BoxComponent subobject **/
	ENGINE_API UBoxComponent* GetBoxComponent() const;
#endif
};
