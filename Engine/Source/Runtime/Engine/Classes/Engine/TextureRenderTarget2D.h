// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "RenderUtils.h"
#include "TextureRenderTarget.h"
#include "TextureRenderTarget2D.generated.h"

/**
 * TextureRenderTarget2D
 *
 * 2D render target texture resource. This can be used as a target
 * for rendering as well as rendered as a regular 2D texture resource.
 *
 */
UCLASS(hidecategories=Object, hidecategories=Texture, MinimalAPI)
class UTextureRenderTarget2D : public UTextureRenderTarget
{
	GENERATED_UCLASS_BODY()

	/** The width of the texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	int32 SizeX;

	/** The height of the texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	int32 SizeY;

	/** the color the texture is cleared to */
	UPROPERTY()
	FLinearColor ClearColor;

	/** The addressing mode to use for the X axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	TEnumAsByte<enum TextureAddress> AddressX;

	/** The addressing mode to use for the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	TEnumAsByte<enum TextureAddress> AddressY;

	/** True to force linear gamma space for this render target */
	UPROPERTY()
	uint32 bForceLinearGamma:1;

	/** Whether to support storing HDR values, which requires more memory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	uint32 bHDR:1;

	/** Whether to support GPU sharing of the underlying native texture resource. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TextureRenderTarget2D, meta=(DisplayName = "Shared"), AssetRegistrySearchable, AdvancedDisplay)
	uint32 bGPUSharedFlag : 1;

	/** Whether to support Mip maps for this render target texture */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	uint32 bAutoGenerateMips:1;

	/** Normally the format is derived from bHDR, this allows code to set the format explicitly. */
	UPROPERTY()
	TEnumAsByte<enum EPixelFormat> OverrideFormat;

	/**
	 * Initialize the settings needed to create a render target texture
	 * and create its resource
	 * @param	InSizeX - width of the texture
	 * @param	InSizeY - height of the texture
	 * @param	InFormat - format of the texture
	 * @param	bInForceLinearGame - forces render target to use linear gamma space
	 */
	ENGINE_API void InitCustomFormat(uint32 InSizeX, uint32 InSizeY, EPixelFormat InOverrideFormat, bool bInForceLinearGamma);

	/** Initializes the render target, the format will be derived from the value of bHDR. */
	ENGINE_API void InitAutoFormat(uint32 InSizeX, uint32 InSizeY);

	/**
	 * Utility for creating a new UTexture2D from a TextureRenderTarget2D
	 * TextureRenderTarget2D must be square and a power of two size.
	 * @param Outer - Outer to use when constructing the new Texture2D.
	 * @param NewTexName - Name of new UTexture2D object.
	 * @param ObjectFlags - Flags to apply to the new Texture2D object
	 * @param Flags - Various control flags for operation (see EConstructTextureFlags)
	 * @param AlphaOverride - If specified, the values here will become the alpha values in the resulting texture
	 * @return New UTexture2D object.
	 */
	ENGINE_API UTexture2D* ConstructTexture2D(UObject* InOuter, const FString& NewTexName, EObjectFlags InObjectFlags, uint32 Flags=CTF_Default, TArray<uint8>* AlphaOverride=NULL);

	/**
	 * Updates (resolves) the render target texture immediately.
	 * Optionally clears the contents of the render target to green.
	 */
	ENGINE_API void UpdateResourceImmediate(bool bClearRenderTarget=true);

	//~ Begin UTexture Interface.
	virtual float GetSurfaceWidth() const override { return SizeX; }
	virtual float GetSurfaceHeight() const override { return SizeY; }
	ENGINE_API virtual FTextureResource* CreateResource() override;
	ENGINE_API virtual EMaterialValueType GetMaterialType() override;
	//~ End UTexture Interface.

	//~ Begin UObject Interface
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	ENGINE_API virtual FString GetDesc() override;
	//~ End UObject Interface

	FORCEINLINE int32 GetNumMips() const
	{
		return NumMips;
	}


	FORCEINLINE EPixelFormat GetFormat() const
	{
		if (OverrideFormat == PF_Unknown)
		{
			return bHDR ? PF_FloatRGBA : PF_B8G8R8A8;
		}
		else
		{
			return OverrideFormat;
		}
	}

private:
	int32	NumMips;
};



