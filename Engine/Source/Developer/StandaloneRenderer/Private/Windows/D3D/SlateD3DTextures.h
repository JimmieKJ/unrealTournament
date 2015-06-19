// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

/**
 * Encapsulates a D3D11 texture that can be accessed by a shader                   
 */
class FSlateD3DTexture : public TSlateTexture< TRefCountPtr<ID3D11ShaderResourceView> >, public FSlateUpdatableTexture
{
public:
	FSlateD3DTexture( uint32 InSizeX, uint32 InSizeY )
		: TSlateTexture()
		, SizeX(InSizeX)
		, SizeY(InSizeY)
	{

	}
	
	~FSlateD3DTexture()
	{

	}

	void Init( DXGI_FORMAT InFormat, D3D11_SUBRESOURCE_DATA* InitalData = NULL, D3D11_USAGE Usage = D3D11_USAGE_DEFAULT, uint32 InCPUAccessFlags = 0, uint32 BindFlags = D3D11_BIND_SHADER_RESOURCE );

	uint32 GetWidth() const { return SizeX; }
	uint32 GetHeight() const { return SizeY; }

	TRefCountPtr<ID3D11Texture2D> GetTextureResource() const { return D3DTexture; }

	// FSlateUpdatableTexture interface
	virtual FSlateShaderResource* GetSlateResource() override {return this;}
	virtual void ResizeTexture( uint32 Width, uint32 Height ) override;
	virtual void UpdateTexture(const TArray<uint8>& Bytes) override;
	virtual void UpdateTextureThreadSafe(const TArray<uint8>& Bytes) override { UpdateTexture(Bytes); }
private:
	/** Actual texture.  In D3D the SRV is used by the shader so our parent class holds that */
	TRefCountPtr<ID3D11Texture2D> D3DTexture;
	uint32 SizeX;
	uint32 SizeY;
};

class FSlateTextureAtlasD3D : public FSlateTextureAtlas
{
public:
	FSlateTextureAtlasD3D( uint32 Width, uint32 Height, uint32 StrideBytes, ESlateTextureAtlasPaddingStyle PaddingStyle );
	~FSlateTextureAtlasD3D();

	void InitAtlasTexture( int32 Index );
	virtual void ConditionalUpdateTexture() override;

	FSlateD3DTexture* GetAtlasTexture() const { return AtlasTexture; }
private:
	FSlateD3DTexture* AtlasTexture;
};

/** 
 * Representation of a texture for fonts in which characters are packed tightly based on their bounding rectangle 
 */
class FSlateFontAtlasD3D : public FSlateFontAtlas
{
public:
	FSlateFontAtlasD3D( uint32 Width, uint32 Height );
	~FSlateFontAtlasD3D();

	/** FSlateFontAtlas interface */
	virtual void ConditionalUpdateTexture();
	virtual class FSlateShaderResource* GetSlateTexture() override { return FontTexture; }
	virtual class FTextureResource* GetEngineTexture() override { return nullptr; }
private:
	/** Texture used for rendering */
	FSlateD3DTexture* FontTexture;
};