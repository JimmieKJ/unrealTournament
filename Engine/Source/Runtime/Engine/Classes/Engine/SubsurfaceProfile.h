// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "../Curves/CurveFloat.h"
#include "SubsurfaceProfile.generated.h"

// struct with all the settings we want in USubsurfaceProfile, separate to make it easer to pass this data around in the engine.
USTRUCT(BlueprintType)
struct FSubsurfaceProfileStruct
{
	GENERATED_USTRUCT_BODY()
	
	/** in world/unreal units (cm) */
	UPROPERTY(Category = "SubsurfaceProfileStruct", EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", UIMax = "50.0", ClampMax = "1000.0"))
	float ScatterRadius;

	/**
	* Specifies the how much of the diffuse light gets into the material,
	* can be seen as a per-channel mix factor between the original image,
	* and the SSS-filtered image (called "strength" in SeparableSSS, default there: 0.48, 0.41, 0.28)
	*/
	UPROPERTY(Category = "SubsurfaceProfileStruct", EditAnywhere, BlueprintReadOnly, meta = (HideAlphaChannel))
	FLinearColor SubsurfaceColor;

	/**
	* defines the per-channel falloff of the gradients
	* produced by the subsurface scattering events, can be used to fine tune the color of the gradients
	* (called "falloff" in SeparableSSS, default there: 1, 0.37, 0.3)
	*/
	UPROPERTY(Category = "SubsurfaceProfileStruct", EditAnywhere, BlueprintReadOnly, meta = (HideAlphaChannel))
	FLinearColor FalloffColor;

	// constructor
	FSubsurfaceProfileStruct()
	{
		// defaults from SeparableSSS.h and the demo
		ScatterRadius = 1.2f;
		SubsurfaceColor = FLinearColor(0.48f, 0.41f, 0.28f);
		FalloffColor = FLinearColor(1.0f, 0.37f, 0.3f);
	}

	void Invalidate()
	{
		// nicer for debugging / VisualizeSSS
		ScatterRadius = 0.0f;
		SubsurfaceColor = FLinearColor(0, 0, 0);
		FalloffColor = FLinearColor(0, 0, 0);
	}
};

/**
 * Subsurface Scattering profile asset, can be specified at the material. Only for "Subsurface Profile" materials, is use during Screenspace Subsurface Scattering
 * Don't change at runtime. All properties in here are per material - texture like variations need to come from properties that are in the GBuffer.
 */
UCLASS(autoexpandcategories = SubsurfaceProfile, MinimalAPI)
class USubsurfaceProfile : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = USubsurfaceProfile, EditAnywhere, meta = (ShowOnlyInnerProperties))
	struct FSubsurfaceProfileStruct Settings;

	// Begin UObject interface
	virtual void BeginDestroy();
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
	// End UObject interface
};

// For render thread side, to identify the right index in a map, is never dereferenced
typedef void* USubsurfaceProfilePointer;


// render thread
class FSubsurfaceProfileTexture : public FRenderResource
{
public:
	// constructor runs on main thread (global variable)
	FSubsurfaceProfileTexture();

	// destructor
	~FSubsurfaceProfileTexture();

	void SetRendererModule(class IRendererModule* InRendererModule) { RendererModule = InRendererModule; }

	// convenience, can be optimized 
	int32 AddOrUpdateProfile(const FSubsurfaceProfileStruct Settings, const USubsurfaceProfilePointer Profile)
	{
		int32 AllocationId = FindAllocationId(Profile); 
		
		if (AllocationId != -1)
		{
			UpdateProfile(AllocationId, Settings);
		}
		else
		{
			AllocationId = AddProfile(Settings, Profile);
		}

		return AllocationId;
	}

	// O(n) n is a small number
	// @param Profile must not be 0
	// @return AllocationId -1: no allocation, should be deallocated with DeallocateSubsurfaceProfile()
	int32 AddProfile(const FSubsurfaceProfileStruct Settings, const USubsurfaceProfilePointer Profile);

	// O(n) to find the element, n is the SSProfile count and usually quite small
	void RemoveProfile(const USubsurfaceProfilePointer InProfile);

	// @param InRendererModule first call needs to have it != 0
	void UpdateProfile(const FSubsurfaceProfileStruct Settings, const USubsurfaceProfilePointer Profile) { int32 AllocationId = FindAllocationId(Profile); UpdateProfile(AllocationId, Settings); }

	// @param InRendererModule first call needs to have it != 0
	void UpdateProfile(int32 AllocationId, const FSubsurfaceProfileStruct Settings);

	// @return can be 0 if there is no SubsurfaceProfile
	const struct IPooledRenderTarget* GetTexture(FRHICommandListImmediate& RHICmdList);


	// FRenderResource interface.
	/**
	* Release textures when device is lost/destroyed.
	*/
	virtual void ReleaseDynamicRHI() override;

	// for debugging, can be removed
	void Dump();

	// for debugging / VisualizeSSS
	ENGINE_API bool GetEntryString(uint32 Index, FString& Out) const;

	// @return -1 if not found
	int32 FindAllocationId(const USubsurfaceProfilePointer InProfile) const;

private:

	struct FSubsurfaceProfileEntry
	{
		FSubsurfaceProfileEntry(FSubsurfaceProfileStruct InSubsurfaceProfileStruct, const USubsurfaceProfilePointer InGameThreadObject)
			: Settings(InSubsurfaceProfileStruct)
			, GameThreadObject(InGameThreadObject)
		{
		}

		FSubsurfaceProfileStruct Settings;

		// not referenced, void and not USubsurfaceProfile* to prevent accidential access, 0 if the entry can be reused or it's [0] which is used as default
		USubsurfaceProfilePointer GameThreadObject;
	};

	// 0 if there if no user of the feature
	IRendererModule* RendererModule;
	//
	TArray<FSubsurfaceProfileEntry> SubsurfaceProfileEntries;

	// Could be optimized but should not happen too often (during level load or editor operations).
	void CreateTexture(FRHICommandListImmediate& RHICmdList);

};

// lives on the render thread
extern ENGINE_API TGlobalResource<FSubsurfaceProfileTexture> GSubsufaceProfileTextureObject;
