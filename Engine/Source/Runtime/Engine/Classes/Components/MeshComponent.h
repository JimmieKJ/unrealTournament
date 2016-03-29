// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Components/PrimitiveComponent.h"
#include "RHIDefinitions.h"
#include "MeshComponent.generated.h"

struct FMaterialRelevance;

/**
 * MeshComponent is an abstract base for any component that is an instance of a renderable collection of triangles.
 *
 * @see UStaticMeshComponent
 * @see USkeletalMeshComponent
 */
UCLASS(abstract)
class ENGINE_API UMeshComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	/** Per-Component material overrides.  These must NOT be set directly or a race condition can occur between GC and the rendering thread. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Rendering, Meta=(ToolTip="Material overrides."))
	TArray<class UMaterialInterface*> OverrideMaterials;
	
	UFUNCTION(BlueprintCallable, Category="Components|Mesh")
	virtual TArray<class UMaterialInterface*> GetMaterials() const;

	/** Returns override Materials count */
	virtual int32 GetNumOverrideMaterials() const;

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	//~ Begin UPrimitiveComponent Interface
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const override;	
	//~ End UPrimitiveComponent Interface

	/** Accesses the scene relevance information for the materials applied to the mesh. Valid from game thread only. */
	FMaterialRelevance GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const;

	/**
	 *	Tell the streaming system whether or not all mip levels of all textures used by this component should be loaded and remain loaded.
	 *	@param bForceMiplevelsToBeResident		Whether textures should be forced to be resident or not.
	 */
	virtual void SetTextureForceResidentFlag( bool bForceMiplevelsToBeResident );

	/**
	 *	Tell the streaming system to start loading all textures with all mip-levels.
	 *	@param Seconds							Number of seconds to force all mip-levels to be resident
	 *	@param bPrioritizeCharacterTextures		Whether character textures should be prioritized for a while by the streaming system
	 *	@param CinematicTextureGroups			Bitfield indicating which texture groups that use extra high-resolution mips
	 */
	virtual void PrestreamTextures( float Seconds, bool bPrioritizeCharacterTextures, int32 CinematicTextureGroups = 0 );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * Output to the log which materials and textures are used by this component.
	 * @param Indent	Number of tabs to put before the log.
	 */
	virtual void LogMaterialsAndTextures(FOutputDevice& Ar, int32 Indent) const;
#endif

public:
	/** Material parameter setting and caching */

	/** Set all occurrences of Scalar Material Parameters with ParameterName in the set of materials of the SkeletalMesh to ParameterValue */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetScalarParameterValueOnMaterials(const FName ParameterName, const float ParameterValue);

	/** Set all occurrences of Vector Material Parameters with ParameterName in the set of materials of the SkeletalMesh to ParameterValue */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetVectorParameterValueOnMaterials(const FName ParameterName, const FVector ParameterValue);
protected:
	/** Retrieves all the (scalar/vector-)parameters from within the used materials on the SkeletalMesh, and stores material index vs parameter names */
	void CacheMaterialParameterNameIndices();

	/** Mark cache parameters map as dirty, cache will be rebuild once SetScalar/SetVector functions are called */
	void MarkCachedMaterialParameterNameIndicesDirty();
	
	/** Map containing material indices for the retrieved scalar material parameter names */
	TMap<FName, TArray<int32>> ScalarParameterMaterialIndices;
	/** Map containing material indices for the retrieved vector material parameter names */
	TMap<FName, TArray<int32>> VectorParameterMaterialIndices;

	/** Flag whether or not the cached material parameter indices map is dirty (defaults to true, and is set from SetMaterial/Set(Skeletal)Mesh */
	uint32 bCachedMaterialParameterIndicesAreDirty : 1;
};
