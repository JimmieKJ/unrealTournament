// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextureAtlas.h"
#include "SlateElementIndexBuffer.h"
#include "SlateElementVertexBuffer.h"

class FSlateDynamicTextureResource;
class FSlateUTextureResource;
class FSlateMaterialResource;

struct FDynamicResourceMap
{
public:
	FDynamicResourceMap();

	TSharedPtr<FSlateDynamicTextureResource> GetDynamicTextureResource( FName ResourceName ) const;

	TSharedPtr<FSlateUTextureResource> GetUTextureResource( UTexture* TextureObject ) const;

	TSharedPtr<FSlateMaterialResource> GetMaterialResource( const UMaterialInterface* Material ) const;

	void AddUTextureResource( UTexture* TextureObject, TSharedRef<FSlateUTextureResource> InResource );
	void RemoveUTextureResource( UTexture* TextureObject );

	void AddDynamicTextureResource( FName ResourceName, TSharedRef<FSlateDynamicTextureResource> InResource);
	void RemoveDynamicTextureResource( FName ResourceName );

	void AddMaterialResource( const UMaterialInterface* Material, TSharedRef<FSlateMaterialResource> InResource );
	void RemoveMaterialResource( const UMaterialInterface* Material );

	void Empty();

	void EmptyUTextureResources();
	void EmptyMaterialResources();
	void EmptyDynamicTextureResources();

	void ReleaseResources();

	uint32 GetNumObjectResources() const { return TextureMap.Num() + MaterialMap.Num(); }

private:
	void RemoveExpiredTextureResources();
	void RemoveExpiredMaterialResources();

private:
	TMap<FName, TSharedPtr<FSlateDynamicTextureResource> > NativeTextureMap;
	
	typedef TMap<TWeakObjectPtr<UTexture>, TSharedPtr<FSlateUTextureResource> > TextureResourceMap;

	/** Map of all texture resources */
	TextureResourceMap TextureMap;

	uint64 TextureMemorySincePurge;

	typedef TMap<TWeakObjectPtr<UMaterialInterface>, TSharedPtr<FSlateMaterialResource> > MaterialResourceMap;

	/** Map of all material resources */
	MaterialResourceMap MaterialMap;

	int32 LastExpiredMaterialNumMarker;
};


struct FCachedRenderBuffers
{
	TSlateElementVertexBuffer<FSlateVertex> VertexBuffer;
	FSlateElementIndexBuffer IndexBuffer;
};


/**
 * Stores a mapping of texture names to their RHI texture resource               
 */
class FSlateRHIResourceManager : public ISlateAtlasProvider, public FSlateShaderResourceManager, public FGCObject
{
public:
	FSlateRHIResourceManager();
	virtual ~FSlateRHIResourceManager();

	/** ISlateAtlasProvider */
	virtual int32 GetNumAtlasPages() const override;
	virtual FIntPoint GetAtlasPageSize() const override;
	virtual FSlateShaderResource* GetAtlasPageResource(const int32 InIndex) const override;
	virtual bool IsAtlasPageResourceAlphaOnly() const override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/**
	 * Loads and creates rendering resources for all used textures.  
	 * In this implementation all textures must be known at startup time or they will not be found
	 */
	void LoadUsedTextures();

	void LoadStyleResources( const ISlateStyle& Style );

	/**
	 * Clears accessed UTexture and Material resources from the previous frame
	 * The accessed textures is used to determine which textures need be updated on the render thread
	 * so they can be used by slate
	 */
	void ReleaseAccessedResources();

	/**
	 * Updates texture atlases if needed
	 */
	void UpdateTextureAtlases();

	/** FSlateShaderResourceManager interface */
	virtual FSlateShaderResourceProxy* GetShaderResource( const FSlateBrush& InBrush ) override;
	virtual FSlateShaderResource* GetFontShaderResource( uint32 FontAtlasIndex, FSlateShaderResource* FontTextureAtlas, const class UObject* FontMaterial ) override;
	virtual ISlateAtlasProvider* GetTextureAtlasProvider() override;

	/**
	 * Makes a dynamic texture resource and begins use of it
	 *
	 * @param InTextureObject	The texture object to create the resource from
	 */
	TSharedPtr<FSlateUTextureResource> MakeDynamicUTextureResource( UTexture* InTextureObject);
	
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

	FCachedRenderBuffers* FindCachedBuffersForHandle(const FSlateRenderDataHandle* RenderHandle) const
	{
		return CachedBuffers.FindRef(RenderHandle);
	}

	FCachedRenderBuffers* FindOrCreateCachedBuffersForHandle(const TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe>& RenderHandle);
	void ReleaseCachedRenderData(FSlateRenderDataHandle* InRenderHandle);
	void ReleaseCachingResourcesFor(const ILayoutCache* Cacher);

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
	 * @param InMaterial	The material object
	 */
	FSlateMaterialResource* GetMaterialResource( const UObject* InMaterial, FVector2D ImageSize, FSlateShaderResource* TextureMask );

	/**
	 * Called when the application exists before the UObject system shuts down so we can free object resources
	 */
	void OnAppExit();

	/**
	 * Get or create the bad resource texture.
	 */
	UTexture* GetBadResourceTexture();

private:
	/** Map of all active dynamic resources being used by brushes */
	FDynamicResourceMap DynamicResourceMap;
	/** Set of dynamic textures that are currently being accessed */
	TSet<UTexture*> AccessedUTextures;
	/** Set of dynamic materials that are current being accessed */
	TSet<UMaterialInterface*> AccessedMaterials;
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
	/** Needed for displaying an error texture when we end up with bad resources. */
	UTexture* BadResourceTexture;

	typedef TMap< FSlateRenderDataHandle*, FCachedRenderBuffers* > TCachedBufferMap;
	TCachedBufferMap CachedBuffers;

	typedef TMap< const ILayoutCache*, TArray< FCachedRenderBuffers* > > TCachedBufferPoolMap;
	TCachedBufferPoolMap CachedBufferPool;
};

