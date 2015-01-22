// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A resource for rendering a UTexture object in Slate */
class FSlateUTextureResource : public TSlateTexture<FTexture2DRHIRef>
{
public:
	static TSharedPtr<FSlateUTextureResource> NullResource;

	FSlateUTextureResource(UTexture2D* InTexture);
	~FSlateUTextureResource();

	/**
	 * Updates the renderng resource with a new UTexture resource
	 */
	void UpdateRenderResource(FTexture* InFTexture);

	/**
	 * FSlateShaderRsourceInterface
	 */
	virtual uint32 GetWidth() const;
	virtual uint32 GetHeight() const;
	virtual ESlateShaderResource::Type GetType() const;
	
public:
	/** Slate rendering proxy */
	FSlateShaderResourceProxy* Proxy;

	/** Texture UObject.  Note: lifetime is managed externally */
	UTexture2D* TextureObject;
};
