// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

class FSlateD3D11RenderingPolicy : public FSlateRenderingPolicy
{
public:
	FSlateD3D11RenderingPolicy( TSharedPtr<FSlateFontCache> InFontCache, TSharedRef<FSlateD3DTextureManager> InTextureManager );
	~FSlateD3D11RenderingPolicy();

	void UpdateBuffers( const FSlateWindowElementList& InElementList );
	void DrawElements( const FMatrix& ViewProjectionMatrix, const TArray<FSlateRenderBatch>& RenderBatches );

	virtual TSharedRef<FSlateShaderResourceManager> GetResourceManager() override;
	virtual TSharedRef<FSlateFontCache> GetFontCache() override { return FontCache.ToSharedRef(); }

private:
	void InitResources();
	void ReleaseResources();
private:
	FSlateD3DVertexBuffer VertexBuffer;
	FSlateD3DIndexBuffer IndexBuffer;
	FSlateDefaultVS* VertexShader;
	FSlateDefaultPS* PixelShader;
	FSlateShaderResource* WhiteTexture;
	TRefCountPtr<ID3D11RasterizerState> NormalRasterState;
	TRefCountPtr<ID3D11RasterizerState> WireframeRasterState;
	TRefCountPtr<ID3D11RasterizerState> ScissorRasterState;
	TRefCountPtr<ID3D11BlendState> AlphaBlendState;
	TRefCountPtr<ID3D11BlendState> NoBlendState;
	TRefCountPtr<ID3D11DepthStencilState> DSStateOff;
	TRefCountPtr<ID3D11SamplerState> PointSamplerState_Wrap;
	TRefCountPtr<ID3D11SamplerState> PointSamplerState_Clamp;
	TRefCountPtr<ID3D11SamplerState> BilinearSamplerState_Wrap;
	TRefCountPtr<ID3D11SamplerState> BilinearSamplerState_Clamp;
	TSharedPtr<FSlateFontCache> FontCache;
	TSharedRef<FSlateD3DTextureManager> TextureManager;
};
