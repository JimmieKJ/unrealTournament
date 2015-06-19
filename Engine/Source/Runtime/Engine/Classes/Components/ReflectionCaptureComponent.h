// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ReflectionCaptureComponent.generated.h"

class FReflectionCaptureProxy;

class FReflectionCaptureFullHDRDerivedData
{
public:
	/** 
	 * The compressed full HDR capture data, with all mips and faces laid out linearly. 
	 * This is read from the rendering thread directly after load, so the game thread must not write to it again.
	 * This is kept compressed because it must persist even after creating the texture for rendering,
	 * Because it is used with a texture array so must support multiple uploads.
	 */
	TArray<uint8> CompressedCapturedData;

	/** Destructor. */
	~FReflectionCaptureFullHDRDerivedData();

	/** Initializes the compressed data from an uncompressed source. */
	ENGINE_API void InitializeFromUncompressedData(const TArray<uint8>& UncompressedData);

	/** Decompresses the capture data. */
	ENGINE_API void GetUncompressedData(TArray<uint8>& UncompressedData) const;

	ENGINE_API TArray<uint8>& GetCapturedDataForSM4Load() 
	{
		GetUncompressedData(CapturedDataForSM4Load);
		return CapturedDataForSM4Load;
	}

	/** Constructs a key string for the DDC that uniquely identifies a FReflectionCaptureFullHDRDerivedData. */
	static FString GetDDCKeyString(const FGuid& StateId);
	static FString GetLegacyDDCKeyString(const FGuid& StateId);

private:

	/** 
	 * This is used to pass the uncompressed capture data to the RT on load for SM4. 
	 * The game thread must not modify it again after sending read commands to the RT.
	 */
	TArray<uint8> CapturedDataForSM4Load;
};

struct FReflectionCaptureEncodedHDRDerivedData : FRefCountedObject
{
	/** 
	 * The uncompressed encoded HDR capture data, with all mips and faces laid out linearly. 
	 * This is read and written from the rendering thread directly after load, so the game thread must not access it again.
	 */
	TArray<uint8> CapturedData;

	/** Destructor. */
	virtual ~FReflectionCaptureEncodedHDRDerivedData();

	/** Generates encoded HDR data from full HDR data and saves it in the DDC, or loads an already generated version from the DDC. */
	static TRefCountPtr<FReflectionCaptureEncodedHDRDerivedData> GenerateEncodedHDRData(const FReflectionCaptureFullHDRDerivedData& FullHDRData, const FGuid& StateId, float Brightness);

private:

	/** Constructs a key string for the DDC that uniquely identifies a FReflectionCaptureEncodedHDRDerivedData. */
	static FString GetDDCKeyString(const FGuid& StateId);

	/** Encodes the full HDR data of FullHDRDerivedData. */
	void GenerateFromDerivedDataSource(const FReflectionCaptureFullHDRDerivedData& FullHDRDerivedData, float Brightness);
};

// -> will be exported to EngineDecalClasses.h
UCLASS(abstract, hidecategories=(Collision, Object, Physics, SceneComponent, Activation, "Components|Activation", Mobility), MinimalAPI)
class UReflectionCaptureComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** A brightness control to scale the captured scene's reflection intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=ReflectionCapture, meta=(UIMin = ".5", UIMax = "4"))
	float Brightness;

	/** The rendering thread's mirror of this reflection capture. */
	FReflectionCaptureProxy* SceneProxy;

	/** Callback to create the rendering thread mirror. */
	ENGINE_API FReflectionCaptureProxy* CreateSceneProxy();

	/** Called to update the preview shapes when something they are dependent on has changed. */
	virtual void UpdatePreviewShape() {}

	/** Indicates that the capture needs to recapture the scene, adds it to the recapture queue. */
	ENGINE_API void SetCaptureIsDirty();

	/** 
	 * Reads reflection capture contents back from the scene and saves the results to the DDC.
	 * Note: this requires a valid scene and RHI and therefore can't be done while cooking.
	 */
	void ReadbackFromGPUAndSaveDerivedData(UWorld* WorldToUpdate);

	/** Marks this component has having been recaptured. */
	void SetCaptureCompleted() { bCaptureDirty = false; }

	/** Gets the radius that bounds the shape's influence, used for culling. */
	virtual float GetInfluenceBoundingRadius() const PURE_VIRTUAL(UReflectionCaptureComponent::GetInfluenceBoundingRadius,return 0;);

	/** Called each tick to recapture and queued reflection captures. */
	ENGINE_API static void UpdateReflectionCaptureContents(UWorld* WorldToUpdate);

	ENGINE_API const FReflectionCaptureFullHDRDerivedData* GetCachedFullHDRDerivedData() const
	{
		return FullHDRDerivedData;
	}

	// Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	// End UActorComponent Interface

	// Begin UObject Interface
	virtual void PostInitProperties() override;	
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PreSave() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual void PreFeatureLevelChange(ERHIFeatureLevel::Type PendingFeatureLevel) override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;
	// End UObject Interface

private:

	/** Whether the reflection capture needs to re-capture the scene. */
	bool bCaptureDirty;

	/** Whether DerivedData is up to date. */
	bool bDerivedDataDirty;

	UPROPERTY()
	FGuid StateId;

	/**
	 * The full HDR capture data to use for rendering.
	 * If loading cooked, this will be loaded from inlined data.
	 * If loading uncooked, this will be loaded from the DDC.
	 * Can be NULL, which indicates there is no up-to-date cached derived data
	 * The rendering thread reads directly from the contents of this object to avoid an extra data copy, so it must be deleted in a thread safe way.
	 */
	FReflectionCaptureFullHDRDerivedData* FullHDRDerivedData;

	/** Only used in SM4, since cubemap texture arrays are not available. */
	class FReflectionTextureCubeResource* SM4FullHDRCubemapTexture;

	/**
	 * The encoded HDR capture data to use for rendering.
	 * If loading cooked, this will be loaded from inlined data.
	 * If loading uncooked, this will be generated from FullHDRDerivedData or loaded from the DDC.
	 * The rendering thread reads directly from the contents of this object to avoid an extra data copy, so it must be deleted in a thread safe way.
	 */
	TRefCountPtr<FReflectionCaptureEncodedHDRDerivedData> EncodedHDRDerivedData;

	/** Cubemap texture resource used for rendering with the encoded HDR values. */
	class FReflectionTextureCubeResource* EncodedHDRCubemapTexture;

	/** Fence used to track progress of releasing resources on the rendering thread. */
	FRenderCommandFence ReleaseResourcesFence;

	/** 
	 * List of reflection captures that need to be recaptured.
	 * These have to be queued because we can only render the scene to update captures at certain points, after the level has loaded.
	 * This queue should be in the UWorld or the FSceneInterface, but those are not available yet in PostLoad.
	 */
	static TArray<UReflectionCaptureComponent*> ReflectionCapturesToUpdate;

	/** 
	 * List of reflection captures that need to be recaptured because they were dirty on load.
	 * These have to be queued because we can only render the scene to update captures at certain points, after the level has loaded.
	 * This queue should be in the UWorld or the FSceneInterface, but those are not available yet in PostLoad.
	 */
	static TArray<UReflectionCaptureComponent*> ReflectionCapturesToUpdateForLoad;

	void UpdateDerivedData(FReflectionCaptureFullHDRDerivedData* NewDerivedData);
	void SerializeSourceData(FArchive& Ar);

	friend class FReflectionCaptureProxy;
};

