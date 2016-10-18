// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "SkyLightComponent.generated.h"

/** 
 * A cubemap texture resource that knows how to upload the capture data from a sky capture. 
 */
class FSkyTextureCubeResource : public FTexture, private FDeferredCleanupInterface
{
	// @todo - support compression

public:

	FSkyTextureCubeResource() :
		Size(0),
		NumMips(0),
		Format(PF_Unknown),
		TextureCubeRHI(NULL),
		NumRefs(0)
	{}

	virtual ~FSkyTextureCubeResource() { check(NumRefs == 0); }

	void SetupParameters(int32 InSize, int32 InNumMips, EPixelFormat InFormat)
	{
		Size = InSize;
		NumMips = InNumMips;
		Format = InFormat;
	}

	virtual void InitRHI() override;

	virtual void ReleaseRHI() override
	{
		TextureCubeRHI.SafeRelease();
		FTexture::ReleaseRHI();
	}

	virtual uint32 GetSizeX() const override
	{
		return Size;
	}

	// PVS-Studio notices that the implementation of GetSizeX is identical to this one
	// and warns us. In this case, it is intentional, so we disable the warning:
	virtual uint32 GetSizeY() const override //-V524
	{
		return Size;
	}

	// Reference counting.
	void AddRef()
	{
		check( IsInGameThread() );
		NumRefs++;
	}

	void Release();

	// FDeferredCleanupInterface
	virtual void FinishCleanup() override
	{
		delete this;
	}

private:

	int32 Size;
	int32 NumMips;
	EPixelFormat Format;
	FTextureCubeRHIRef TextureCubeRHI;
	int32 NumRefs;
};

UENUM()
enum ESkyLightSourceType
{
	/** Construct the sky light from the captured scene, anything further than SkyDistanceThreshold from the sky light position will be included. */
	SLS_CapturedScene,
	/** Construct the sky light from the specified cubemap. */
	SLS_SpecifiedCubemap,
	SLS_MAX,
};

UCLASS(Blueprintable, ClassGroup=Lights, HideCategories=(Trigger,Activation,"Components|Activation",Physics), meta=(BlueprintSpawnableComponent))
class ENGINE_API USkyLightComponent : public ULightComponentBase
{
	GENERATED_UCLASS_BODY()

	/** Indicates where to get the light contribution from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light)
	TEnumAsByte<enum ESkyLightSourceType> SourceType;

	/** Cubemap to use for sky lighting if SourceType is set to SLS_SpecifiedCubemap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light)
	class UTextureCube* Cubemap;

	/** Angle to rotate the source cubemap when SourceType is set to SLS_SpecifiedCubemap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(UIMin = "0", UIMax = "360"))
	float SourceCubemapAngle;

	/** Maximum resolution for the very top processed cubemap mip. Must be a power of 2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	int32 CubemapResolution;

	/** 
	 * Distance from the sky light at which any geometry should be treated as part of the sky. 
	 * This is also used by reflection captures, so update reflection captures to see the impact.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light)
	float SkyDistanceThreshold;

	/** 
	 * Whether all distant lighting from the lower hemisphere should be set to LowerHemisphereColor.  
	 * Enabling this is accurate when lighting a scene on a planet where the ground blocks the sky, 
	 * However disabling it can be useful to approximate skylight bounce lighting (eg Movable light).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay, meta=(DisplayName = "Lower Hemisphere Is Solid Color"))
	bool bLowerHemisphereIsBlack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay)
	FLinearColor LowerHemisphereColor;

	/** 
	 * Max distance that the occlusion of one point will affect another.
	 * Higher values increase the cost of Distance Field AO exponentially.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=DistanceFieldAmbientOcclusion, meta=(UIMin = "200", UIMax = "1500"))
	float OcclusionMaxDistance;

	/** 
	 * Contrast S-curve applied to the computed AO.  A value of 0 means no contrast increase, 1 is a significant contrast increase.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=DistanceFieldAmbientOcclusion, meta=(UIMin = "0", UIMax = "1"))
	float Contrast;

	/** 
	 * Controls the darkest that a fully occluded area can get.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=DistanceFieldAmbientOcclusion, meta=(UIMin = "0", UIMax = "1"))
	float MinOcclusion;

	/** Tint color on occluded areas, artistic control. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=DistanceFieldAmbientOcclusion)
	FColor OcclusionTint;

	class FSkyLightSceneProxy* CreateSceneProxy() const;

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostInterpChange(UProperty* PropertyThatChanged) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void CheckForErrors() override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	//~ End UObject Interface

	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	void ApplyComponentInstanceData(class FPrecomputedSkyLightInstanceData* ComponentInstanceData);

	/** Called each tick to recapture and queued sky captures. */
	static void UpdateSkyCaptureContents(UWorld* WorldToUpdate);
	static void UpdateSkyCaptureContentsArray(UWorld* WorldToUpdate, TArray<USkyLightComponent*>& ComponentArray, bool bBlendSources);

	/** Computes an irradiance environment map using only emissive contribution from the sky light. */
	void CaptureEmissiveIrradianceEnvironmentMap(FSHVectorRGB3& OutIrradianceMap) const;

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|SkyLight")
	void SetIntensity(float NewIntensity);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Light")
	void SetIndirectLightingIntensity(float NewIntensity);

	/** Set color of the light */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|SkyLight")
	void SetLightColor(FLinearColor NewLightColor);

	/** Sets the cubemap used when SourceType is set to SpecifiedCubemap, and causes a skylight update on the next tick. */
	UFUNCTION(BlueprintCallable, Category="SkyLight")
	void SetCubemap(UTextureCube* NewCubemap);

	/** 
	 * Creates sky lighting from a blend between two cubemaps, which is only valid when SourceType is set to SpecifiedCubemap. 
	 * This can be used to seamlessly transition sky lighting between different times of day.
	 * The caller should continue to update the blend until BlendFraction is 0 or 1 to reduce rendering cost.
	 * The caller is responsible for avoiding pops due to changing the source or destination.
	 */
	UFUNCTION(BlueprintCallable, Category="SkyLight")
	void SetCubemapBlend(UTextureCube* SourceCubemap, UTextureCube* DestinationCubemap, float InBlendFraction);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|SkyLight")
	void SetOcclusionTint(const FColor& InTint);

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|SkyLight")
	void SetMinOcclusion(float InMinOcclusion);

	virtual void SetVisibility(bool bNewVisibility, bool bPropagateToChildren=false) override;

	/** Indicates that the capture needs to recapture the scene, adds it to the recapture queue. */
	void SetCaptureIsDirty();
	void SetBlendDestinationCaptureIsDirty();
	void SanitizeCubemapSize();

	/** 
	 * Recaptures the scene for the skylight. 
	 * This is useful for making sure the sky light is up to date after changing something in the world that it would capture.
	 * Warning: this is very costly and will definitely cause a hitch.
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|SkyLight")
	void RecaptureSky();

	void SetIrradianceEnvironmentMap(const FSHVectorRGB3& InIrradianceEnvironmentMap)
	{
		IrradianceEnvironmentMap = InIrradianceEnvironmentMap;
	}

protected:

	/** Indicates whether the cached data stored in GetComponentInstanceData is valid to be applied in ApplyComponentInstanceData. */
	bool bSavedConstructionScriptValuesValid;

	bool bHasEverCaptured;

	TRefCountPtr<FSkyTextureCubeResource> ProcessedSkyTexture;
	FSHVectorRGB3 IrradianceEnvironmentMap;
	float AverageBrightness;

	/** If 0, no blend is present.  If > 0, BlendDestinationProcessedSkyTexture and BlendDestinationIrradianceEnvironmentMap must be generated and used for rendering. */
	float BlendFraction;

	UPROPERTY(transient)
	class UTextureCube* BlendDestinationCubemap;
	TRefCountPtr<FSkyTextureCubeResource> BlendDestinationProcessedSkyTexture;
	FSHVectorRGB3 BlendDestinationIrradianceEnvironmentMap;
	float BlendDestinationAverageBrightness;

	/** Tracks when the rendering thread has completed its writes to IrradianceEnvironmentMap. */
	FRenderCommandFence IrradianceMapFence;

	/** Fence used to track progress of releasing resources on the rendering thread. */
	FRenderCommandFence ReleaseResourcesFence;

	FSkyLightSceneProxy* SceneProxy;

	/** 
	 * List of sky captures that need to be recaptured.
	 * These have to be queued because we can only render the scene to update captures at certain points, after the level has loaded.
	 * This queue should be in the UWorld or the FSceneInterface, but those are not available yet in PostLoad.
	 */
	static TArray<USkyLightComponent*> SkyCapturesToUpdate;
	static TArray<USkyLightComponent*> SkyCapturesToUpdateBlendDestinations;

	//~ Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	//~ Begin UActorComponent Interface

	void UpdateLimitedRenderingStateFast();

	friend class FSkyLightSceneProxy;
};



