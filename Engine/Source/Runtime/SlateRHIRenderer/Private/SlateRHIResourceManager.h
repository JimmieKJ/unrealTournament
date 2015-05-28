// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextureAtlas.h"

class FSlateDynamicTextureResource;
class FSlateUTextureResource;
class FSlateMaterialResource;

struct FDynamicResourceMap
{
public:
	TSharedPtr<FSlateDynamicTextureResource> GetDynamicTextureResource( FName ResourceName ) const;

	TSharedPtr<FSlateUTextureResource> GetUTextureResource( UTexture2D* TextureObject ) const;

	TSharedPtr<FSlateMaterialResource> GetMaterialResource( UMaterialInterface* Material ) const;

	void AddUTextureResource( UTexture2D* TextureObject, TSharedRef<FSlateUTextureResource> InResource );
	void RemoveUTextureResource( UTexture2D* TextureObject );

	void AddDynamicTextureResource( FName ResourceName, TSharedRef<FSlateDynamicTextureResource> InResource);
	void RemoveDynamicTextureResource( FName ResourceName );

	void AddMaterialResource( UMaterialInterface* Material, TSharedRef<FSlateMaterialResource> InResource );
	void RemoveMaterialResource( UMaterialInterface* Material );

	void Empty();

	void EmptyUTextureResources();
	void EmptyMaterialResources();
	void EmptyDynamicTextureResources();

	void ReleaseResources();

	uint32 GetNumObjectResources() const { return UTextureResourceMap.Num() + MaterialResourceMap.Num(); }
private:
	TMap<FName, TSharedPtr<FSlateDynamicTextureResource> > NativeTextureMap;
	
	TMap<TWeakObjectPtr<UTexture2D>, TSharedPtr<FSlateUTextureResource> > UTextureResourceMap;

	/** Map of all material resources */
	TMap<TWeakObjectPtr<UMaterialInterface>, TSharedPtr<FSlateMaterialResource> > MaterialResourceMap;
};


/**
 * Stores a mapping of texture names to their RHI texture resource               
 */
class FSlateRHIResourceManager : public ISlateAtlasProvider, public FSlateShaderResourceManager
{
public:
	FSlateRHIResourceManager();
	virtual ~FSlateRHIResourceManager();

	/** ISlateAtlasProvider */
	virtual int32 GetNumAtlasPages() const override;
	virtual FIntPoint GetAtlasPageSize() const override;
	virtual FSlateShaderResource* GetAtlasPageResource(const int32 InIndex) const override;
	virtual bool IsAtlasPageResourceAlphaOnly() const override;

	/**
	 * Loads and creates rendering resources for all used textures.  
	 * In this implementation all textures must be known at startup time or they will not be found
	 */
	void LoadUsedTextures();

	void LoadStyleResources( const ISlateStyle& Style );

	/**
	 * Clears accessed UTexture resources from the previous frame
	 * The accessed textures is used to determine which textures need be updated on the render thread
	 * so they can be used by slate
	 */
	void ClearAccessedUTextures();

	/**
	 * Updates texture atlases if needed
	 */
	void UpdateTextureAtlases();

	/** FSlateShaderResourceManager interface */
	virtual FSlateShaderResourceProxy* GetShaderResource( const FSlateBrush& InBrush ) override;
	virtual ISlateAtlasProvider* GetTextureAtlasProvider() override;

	/**
	 * Makes a dynamic texture resource and begins use of it
	 *
	 * @param InTextureObject	The texture object to create the resource from
	 */
	TSharedPtr<FSlateUTextureResource> MakeDynamicUTextureResource( UTexture2D* InTextureObject);
	
	/**
	 * Makes a dynamic texture resource and begins use of it
	 *
	 * @param ResourceName			The name identifier of the resource
	 * @param Width					The width of the resource
	 * @param Height				The height of the resource
	 * @param Bytes					The payload containing the resource
	 */
	TSharedPtr<FSlateDynamicTextureResource> MakeDynamicTextureResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes );

	/**
	 * Find a dynamic texture resource 
	 *
	 * @param ResourceName			The name identifier of the resource
	 */
	TSharedPtr<FSlateDynamicTextureResource> GetDynamicTextureResourceByName( FName ResourceName );

	/**
	 * Returns true if a texture resource with the passed in resource name is available 
	 */
	bool ContainsTexture( const FName& ResourceName ) const;

	/** Releases a specific dynamic resource */
	void ReleaseDynamicResource( const FSlateBrush& InBrush );

	/** 
	 * Creates a new texture from the given texture name
	 *
	 * @param TextureName	The name of the texture to load
	 */
	virtual bool LoadTexture( const FName& TextureName, const FString& ResourcePath, uint32& Width, uint32& Height, TArray<uint8>& DecodedImage );
	virtual bool LoadTexture( const FSlateBrush& InBrush, uint32& Width, uint32& Height, TArray<uint8>& DecodedImage );


	/**
	 * Releases rendering resources
	 */
	void ReleaseResources();

	/**
	 * Reloads texture resources for all used textures.  
	 *
	 * @param InExtraResources     Optional list of textures to create that aren't in the style.
	 */
	void ReloadTextures();

private:
	/**
	 * Deletes resources created by the manager
	 */
	void DeleteResources();

	/** 
	 * Creates textures from files on disk and atlases them if possible
	 *
	 * @param Resources	The brush resources to load images for
	 */
	void CreateTextures( const TArray< const FSlateBrush* >& Resources );

	/**
	 * Generates rendering resources for a texture
	 * 
	 * @param Info	Information on how to generate the texture
	 */
	FSlateShaderResourceProxy* GenerateTextureResource( const FNewTextureInfo& Info );
	
	/**
	 * Returns a texture rendering resource from for a dynamically loaded texture or utexture object
	 * Note: this will load the UTexture or image if needed 
	 *
	 * @param InBrush	Slate brush for the dynamic resource
	 */
	FSlateShaderResourceProxy* FindOrCreateDynamicTextureResource( const FSlateBrush& InBrush );

	/**
	 * Returns a rendering resource for a material
	 *
	 * @param InBrush	Slate brush for the material
	 */
	FSlateShaderResourceProxy* GetMaterialResource( const FSlateBrush& InBrush );

	/**
	 * Called when the application exists before the UObject system shuts down so we can free object resources
	 */
	void OnAppExit();

private:
	/** Map of all active dynamic resources being used by brushes */
	FDynamicResourceMap DynamicResourceMap;
	/** Set of dynamic textures that are currently being accessed */
	TSet< TWeakObjectPtr<UTexture2D> > AccessedUTextures;
	/** List of old utexture resources that are free to use as new resources */
	TArray< TSharedPtr<FSlateUTextureResource> > UTextureFreeList;
	/** List of old dynamic resources that are free to use as new resources */
	TArray< TSharedPtr<FSlateDynamicTextureResource> > DynamicTextureFreeList;\
	/** List of old material resources that are free to use as new resources */
	TArray< TSharedPtr<FSlateMaterialResource> > MaterialResourceFreeList;
	/** Static Texture atlases which have been created */
	TArray<class FSlateTextureAtlasRHI*> TextureAtlases;
	/** Static Textures created that are not atlased */	
	TArray<class FSlateTexture2DRHIRef*> NonAtlasedTextures;
	/** The size of each texture atlas (square) */
	uint32 AtlasSize;
	/** This max size of each texture in an atlas */
	FIntPoint MaxAltasedTextureSize;

};

