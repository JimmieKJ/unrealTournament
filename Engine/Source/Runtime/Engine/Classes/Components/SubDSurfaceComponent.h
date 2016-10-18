// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Components/PrimitiveComponent.h"
#include "SubDSurfaceComponent.generated.h"

/**
 * Subdivision Surface Component (Experimental, Early work in progress)
 */
UCLASS(ClassGroup=Rendering, hidecategories=(Object,LOD,Physics,TextureStreaming,Activation,"Components|Activation",Collision), editinlinenew, meta=(BlueprintSpawnableComponent = ""))
class ENGINE_API USubDSurfaceComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	// asset
private:
	UPROPERTY(EditAnywhere, Category=StaticMesh)
	class USubDSurface* Mesh;
public:

	/* Refinement Level of the SubD mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=StaticMesh)
	int32 DebugLevel;

	/** Change the SubDSurface used by this instance. */
	UFUNCTION(BlueprintCallable, Category="Components|Mesh")
	virtual bool SetMesh(class USubDSurface* NewMesh);

	UPROPERTY()
	UStaticMeshComponent* DisplayMeshComponent;

public:

	// -----------------------------

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject interface.

	// Begin UPrimitiveComponent interface.
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;
	virtual int32 GetNumMaterials() const;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial);
	// End UPrimitiveComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// Begin USceneComponent interface.

	// --

	void SetDisplayMeshComponent(UStaticMeshComponent* InDisplayMeshComponent);
	void RecreateMeshData();
};





