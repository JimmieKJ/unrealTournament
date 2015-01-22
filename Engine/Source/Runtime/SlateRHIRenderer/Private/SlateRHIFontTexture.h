// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Override for font textures that saves data between Init and ReleaseDynamicRHI to 
 * ensure all characters in the font texture exist if the rendering resource has to be recreated 
 * between caching new characters
 */
class FSlateFontTextureRHI : public TSlateTexture<FTexture2DRHIRef>, public FTextureResource
{
public:
	/** Constructor.  Initializes the texture
	 *
	 * @param InWidth The width of the texture
	 * @param InHeight The height of the texture
	 */
	FSlateFontTextureRHI( uint32 InWidth, uint32 InHeight );

	/** FSlateShaderResource interface */
	virtual uint32 GetWidth() const override { return Width; }
	virtual uint32 GetHeight() const override { return Height; }

	/** FTextureResource interface */
	virtual uint32 GetSizeX() const override { return Width; }
	virtual uint32 GetSizeY() const override { return Height; }
	virtual FString GetFriendlyName() const override { return TEXT("FSlateFontTextureRHI"); }

	/** FRenderResource interface */
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;
private:
	/** Width of this texture */
	uint32 Width;
	/** Height of this texture */
	uint32 Height;
	/** Temporary data stored between Release and InitDynamicRHI */
	TArray<uint8> TempData;
};

/** 
 * Representation of a texture for fonts in which characters are packed tightly based on their bounding rectangle 
 */
class FSlateFontAtlasRHI : public FSlateFontAtlas
{
public:
	FSlateFontAtlasRHI( uint32 Width, uint32 Height );
	~FSlateFontAtlasRHI();

	/**
	 * FSlateFontAtlas interface 
	 */
	virtual class FSlateShaderResource* GetSlateTexture() override { return FontTexture.Get(); }
	virtual class FTextureResource* GetEngineTexture() override { return FontTexture.Get(); }
	virtual void ConditionalUpdateTexture()  override;
	virtual void ReleaseResources() override;
private:
	TUniquePtr<FSlateFontTextureRHI> FontTexture;
};
