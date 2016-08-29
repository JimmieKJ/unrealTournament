// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSlateD3DTextureManager;
class FSlateD3D11RenderingPolicy;

struct FSlateD3DViewport
{
	FMatrix ProjectionMatrix;
	D3D11_VIEWPORT ViewportInfo;
	TRefCountPtr<IDXGISwapChain> D3DSwapChain;
	TRefCountPtr<ID3D11Texture2D> BackBufferTexture;
	TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
	TRefCountPtr<ID3D11DepthStencilView> DepthStencilView;
	bool bFullscreen;

	FSlateD3DViewport()
		: bFullscreen( false )
	{

	}

	~FSlateD3DViewport()
	{	
		BackBufferTexture.SafeRelease();
		RenderTargetView.SafeRelease();
		DepthStencilView.SafeRelease();
		D3DSwapChain.SafeRelease();
	}

};

extern TRefCountPtr<ID3D11Device> GD3DDevice;
extern TRefCountPtr<ID3D11DeviceContext> GD3DDeviceContext;

class FSlateD3DRenderer : public FSlateRenderer
{
public:
	STANDALONERENDERER_API FSlateD3DRenderer( const ISlateStyle &InStyle );
	~FSlateD3DRenderer();

	/** FSlateRenderer Interface */
	virtual bool Initialize() override;
	virtual void Destroy() override;
	virtual FSlateDrawBuffer& GetDrawBuffer() override; 
	virtual void DrawWindows( FSlateDrawBuffer& InWindowDrawBuffer ) override;
	virtual void OnWindowDestroyed( const TSharedRef<SWindow>& InWindow ) override;
	virtual void CreateViewport( const TSharedRef<SWindow> InWindow ) override;
	virtual void RequestResize( const TSharedPtr<SWindow>& InWindow, uint32 NewSizeX, uint32 NewSizeY ) override;
	virtual void UpdateFullscreenState( const TSharedRef<SWindow> InWindow, uint32 OverrideResX, uint32 OverrideResY ) override;
	virtual void RestoreSystemResolution(const TSharedRef<SWindow> InWindow) override {}
	virtual void ReleaseDynamicResource( const FSlateBrush& Brush ) override;
	virtual bool GenerateDynamicImageResource(FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes) override;
	virtual FSlateResourceHandle GetResourceHandle( const FSlateBrush& Brush ) override;
	virtual void RemoveDynamicBrushResource( TSharedPtr<FSlateDynamicImageBrush> BrushToRemove ) override;
	virtual void LoadStyleResources( const ISlateStyle& Style ) override;
	virtual FSlateUpdatableTexture* CreateUpdatableTexture(uint32 Width, uint32 Height) override;
	virtual void ReleaseUpdatableTexture(FSlateUpdatableTexture* Texture) override;
	virtual ISlateAtlasProvider* GetTextureAtlasProvider() override;
	
	bool CreateDevice();
	void CreateDepthStencilBuffer( FSlateD3DViewport& Viewport );

private:
	void Private_CreateViewport( TSharedRef<SWindow> InWindow, const FVector2D& WindowSize );
	void Private_ResizeViewport( const TSharedRef<SWindow> InWindow, uint32 Width, uint32 Height, bool bFullscreen );
	void CreateBackBufferResources( TRefCountPtr<IDXGISwapChain>& InSwapChain, TRefCountPtr<ID3D11Texture2D>& OutBackBuffer, TRefCountPtr<ID3D11RenderTargetView>& OutRTV );
private:
	FMatrix ViewMatrix;
	TMap<const SWindow*, FSlateD3DViewport> WindowToViewportMap;
	FSlateDrawBuffer DrawBuffer;
	TSharedPtr<FSlateElementBatcher> ElementBatcher;
	TSharedPtr<FSlateD3DTextureManager> TextureManager;
	TSharedPtr<FSlateD3D11RenderingPolicy> RenderingPolicy;
	TArray<TSharedPtr<FSlateDynamicImageBrush>> DynamicBrushesToRemove;
};
