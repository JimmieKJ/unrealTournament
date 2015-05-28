// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

// Forward declarations
class ALandscapeProxy;
class ULandscapeComponent;

namespace MaterialExportUtils
{
	struct UNREALED_API FFlattenMaterial
	{
		FFlattenMaterial()
			: DiffuseSize(512, 512)
			, NormalSize(512, 512)
			, MetallicSize(0, 0)
			, RoughnessSize(0, 0)
			, SpecularSize(0, 0)
		{}
		
		FGuid			MaterialId;
		
		FIntPoint		DiffuseSize;
		FIntPoint		NormalSize;
		FIntPoint		MetallicSize;	
		FIntPoint		RoughnessSize;	
		FIntPoint		SpecularSize;	
			
		TArray<FColor>	DiffuseSamples;
		TArray<FColor>	NormalSamples;
		TArray<FColor>	MetallicSamples;
		TArray<FColor>	RoughnessSamples;
		TArray<FColor>	SpecularSamples;
	};
	
	/**
	 * Renders specified material property into texture
	 *
	 * @param InWorld				World object to use for material property rendering
	 * @param InMaterial			Target material
	 * @param InMaterialProperty	Material property to render
	 * @param InRenderTarget		Render target to render to
	 * @param OutBMP				Output array of rendered samples 
	 * @return						Whether operation was successful
	 */
	UNREALED_API bool ExportMaterialProperty(UWorld* InWorld, UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, UTextureRenderTarget2D* InRenderTarget, TArray<FColor>& OutBMP);
	UNREALED_API bool ExportMaterialProperty(UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, UTextureRenderTarget2D* InRenderTarget, TArray<FColor>& OutBMP);

	/**
	 * Flattens specified material
	 *
	 * @param InWorld				World object to use for material rendering
	 * @param InMaterial			Target material
	 * @param OutFlattenMaterial	Output flattened material
	 * @return						Whether operation was successful
	 */
	UNREALED_API bool ExportMaterial(UWorld* InWorld, UMaterialInterface* InMaterial, FFlattenMaterial& OutFlattenMaterial);
	UNREALED_API bool ExportMaterial(UMaterialInterface* InMaterial, FFlattenMaterial& OutFlattenMaterial);
	
	/**
	 * Flattens specified landscape material
	 *
	 * @param InLandscape			Target landscape
	 * @param HiddenPrimitives		Primitives to hide while rendering scene to texture
	 * @param OutFlattenMaterial	Output flattened material
	 * @return						Whether operation was successful
	 */
	UNREALED_API bool ExportMaterial(ALandscapeProxy* InLandscape, const TSet<FPrimitiveComponentId>& HiddenPrimitives, FFlattenMaterial& OutFlattenMaterial);

	/**
	 * Creates UMaterial object from a flatten material
	 *
	 * @param Outer					Outer for the material and texture objects, if NULL new packages will be created for each asset
	 * @param BaseName				BaseName for the material and texture objects, should be a long package name in case Outer is not specified
	 * @param Flags					Object flags for the material and texture objects.
	 * @param OutGeneratedAssets	List of generated assets - material, textures
	 * @return						Returns a pointer to the constructed UMaterial object.
	 */
	UNREALED_API UMaterial* CreateMaterial(const FFlattenMaterial& InFlattenMaterial, UPackage* InOuter, const FString& BaseName, EObjectFlags Flags, TArray<UObject*>& OutGeneratedAssets);


	/**
 	 * Generates a texture from an array of samples 
	 *
	 * @param Outer					Outer for the material and texture objects, if NULL new packages will be created for each asset
	 * @param AssetLongName			Long asset path for the new texture
	 * @param Size					Resolution of the texture to generate (must match the number of samples)
	 * @param Samples				Color data for the texture
	 * @param CompressionSettings	Compression settings for the new texture
	 * @param LODGroup				LODGroup for the new texture
	 * @param Flags					ObjectFlags for the new texture
	 * @param bSRGB					Whether to set the bSRGB flag on the new texture
	 * @param SourceGuidHash		(optional) Hash (stored as Guid) to use part of the texture source's DDC key.
	 * @return						The new texture.
	 */
	UNREALED_API UTexture2D* CreateTexture(UPackage* Outer, const FString& AssetLongName, FIntPoint Size, const TArray<FColor>& Samples, TextureCompressionSettings CompressionSettings, TextureGroup LODGroup, EObjectFlags Flags, bool bSRGB, const FGuid& SourceGuidHash = FGuid());

	/**
	* Creates bakes textures for a ULandscapeComponent
	*
	* @param LandscapeComponent		The component to bake textures for
	* @return						Whether operation was successful
	*/
	UNREALED_API bool ExportBaseColor(ULandscapeComponent* LandscapeComponent, int32 TextureSize, TArray<FColor>& OutSamples);
}

