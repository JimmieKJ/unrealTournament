// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A resource for rendering a UTexture object in Slate */
class FSlateUTextureResource : public FSlateShaderResource
{
public:
	static TSharedPtr<FSlateUTextureResource> NullResource;

	FSlateUTextureResource(UTexture* InTexture);
	~FSlateUTextureResource();

	/**
	 * Updates the rendering resource with a potentially new texture
	 */
	void UpdateRenderResource(FTexture* InFTexture);

	/**
	 * FSlateShaderRsourceInterface
	 */
	virtual uint32 GetWidth() const;
	virtual uint32 GetHeight() const;
	virtual ESlateShaderResource::Type GetType() const;
	
	/** Gets the RHI resource used for rendering and updates the last render time for texture streaming */
	FTextureRHIRef AccessRHIResource() 
	{
		if (TextureObject && TextureObject->Resource)
		{
			FTexture* TextureResource = TextureObject->Resource;
			TextureResource->LastRenderTime = FApp::GetCurrentTime();

			return TextureResource->TextureRHI;
		}

		return FTextureRHIRef();
	}
		
	
public:
	/** Slate rendering proxy */
	FSlateShaderResourceProxy* Proxy;

	/** Texture UObject.  Note: lifetime is managed externally */
	UTexture* TextureObject;
};
