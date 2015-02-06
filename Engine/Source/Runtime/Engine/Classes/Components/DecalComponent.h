// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "TimerManager.h"
#include "DecalComponent.generated.h"

class FDeferredDecalProxy;

/** 
 * A material that is rendered onto the surface of a mesh. A kind of 'bumper sticker' for a model.
 */
UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent, Activation, "Components|Activation", Mobility), ClassGroup=Rendering, meta=(BlueprintSpawnableComponent))
class ENGINE_API UDecalComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** Decal material. */
	//editable(Decal) private{private} const MaterialInterface	DecalMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Decal)
	class UMaterialInterface* DecalMaterial;

	/** 
	 * Controls the order in which decal elements are rendered.  Higher values draw later (on top). 
	 * Setting many different sort orders on many different decals prevents sorting by state and can reduce performance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Decal)
	int32 SortOrder;

	/** Sets the sort order for the decal component. Higher values draw later (on top). This will force the decal to reattach */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Decal")
	void SetSortOrder(int32 Value);

	/** setting decal material on decal component. This will force the decal to reattach */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Decal")
	void SetDecalMaterial(class UMaterialInterface* NewDecalMaterial);

	/** Accessor for decal material */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Decal")
	class UMaterialInterface* GetDecalMaterial() const;

	/** Utility to allocate a new Dynamic Material Instance, set its parent to the currently applied material, and assign it */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Decal")
	virtual class UMaterialInstanceDynamic* CreateDynamicMaterialInstance();


public:
	/** The decal proxy. */
	FDeferredDecalProxy* SceneProxy;

	/**
	 * Pushes new selection state to the render thread primitive proxy
	 */
	void PushSelectionToProxy();

protected:
	/** Handle for efficient management of DestroyDecalComponent timer */
	FTimerHandle TimerHandle_DestroyDecalComponent;

	/** Called when the life span of the decal has been exceeded */
	void LifeSpanCallback();

public:
	
	void SetLifeSpan(const float LifeSpan);

	/**
	 * Retrieves the materials used in this component
	 *
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;
	
	virtual FDeferredDecalProxy* CreateSceneProxy();
	virtual int32 GetNumMaterials() const
	{
		return 1; // DecalMaterial
	}

	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const
	{
		return (ElementIndex == 0) ? DecalMaterial : NULL;
	}
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
	{
		if (ElementIndex == 0)
		{
			SetDecalMaterial(InMaterial);
		}
	}
	
	// Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	// End UActorComponent Interface

	// Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// End USceneComponent Interface

};

