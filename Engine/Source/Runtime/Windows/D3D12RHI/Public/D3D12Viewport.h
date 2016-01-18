// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Viewport.h: D3D viewport RHI definitions.
=============================================================================*/

#pragma once

static DXGI_FORMAT GetRenderTargetFormat(EPixelFormat PixelFormat)
{
	DXGI_FORMAT	DXFormat = (DXGI_FORMAT)GPixelFormats[PixelFormat].PlatformFormat;
	switch (DXFormat)
	{
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_BC1_TYPELESS:			return DXGI_FORMAT_BC1_UNORM;
	case DXGI_FORMAT_BC2_TYPELESS:			return DXGI_FORMAT_BC2_UNORM;
	case DXGI_FORMAT_BC3_TYPELESS:			return DXGI_FORMAT_BC3_UNORM;
	case DXGI_FORMAT_R16_TYPELESS:			return DXGI_FORMAT_R16_UNORM;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:		return DXGI_FORMAT_R8G8B8A8_UNORM;
	default: 								return DXFormat;
	}
}

class FD3D12Viewport : public FRHIViewport, public FD3D12DeviceChild
{
public:

	FD3D12Viewport(class FD3D12Device* InParent, HWND InWindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat InPixelFormat);

    void Init(IDXGIFactory4* Factory, bool AssociateWindow = true);

	~FD3D12Viewport();

	void Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen);

	/**
	 * If the swap chain has been invalidated by DXGI, resets the swap chain to the expected state; otherwise, does nothing.
	 * Called once/frame by the game thread on all viewports.
	 * @param bIgnoreFocus - Whether the reset should happen regardless of whether the window is focused.
	 */
	void ConditionalResetSwapChain(bool bIgnoreFocus);

	/** Presents the swap chain.
	 * Returns true if Present was done by Engine.
	 */
	bool Present(bool bLockToVsync);

	// Accessors.
	FIntPoint GetSizeXY() const { return FIntPoint(SizeX, SizeY); }
	SIZE_T GetBackBufferIndex() const { return SwapChain3->GetCurrentBackBufferIndex(); }

	FD3D12Texture2D* GetBackBuffer() const { return BackBuffers[GetBackBufferIndex()].GetReference(); }
	FD3D12Texture2D* GetBackBuffer(uint32 Index) const { return BackBuffers[Index % NumBackBuffers].GetReference(); }

	IDXGISwapChain3* GetSwapChain() const { return SwapChain3; }

	virtual void* GetNativeSwapChain() const override { return GetSwapChain(); }
	virtual void* GetNativeBackBufferTexture() const override { return GetBackBuffer()->GetResource(); }
	virtual void* GetNativeBackBufferRT() const override { return GetBackBuffer()->GetRenderTargetView(0, 0); }

	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override
	{
		CustomPresent = InCustomPresent;
	}
	virtual FRHICustomPresent* GetCustomPresent() const { return CustomPresent; }

	virtual void* GetNativeWindow(void** AddParam = nullptr) const override { return (void*)WindowHandle; }

    uint32 GetNumBackBuffers(){ return NumBackBuffers; }

private:

	/** Presents the frame synchronizing with DWM. */
	void PresentWithVsyncDWM();

	/**
	 * Presents the swap chain checking the return result.
	 * Returns true if Present was done by Engine.
	 */
	bool PresentChecked(int32 SyncInterval);

	uint64 LastFlipTime;
	uint64 LastFrameComplete;
	uint64 LastCompleteTime;
	int32 SyncCounter;
	bool bSyncedLastFrame;
	HWND WindowHandle;
    uint32 MaximumFrameLatency;
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	EPixelFormat PixelFormat;
	bool bIsValid;
	TRefCountPtr<IDXGISwapChain3> SwapChain3;

	static const uint32 MaxNumBackBuffers = 6;
	static const uint32 DefaultNumBackBuffers = 3;
	TRefCountPtr<FD3D12Texture2D> BackBuffers[MaxNumBackBuffers];
	uint32 NumBackBuffers;
	bool bIsBenchmarkMode;

	FCustomPresentRHIRef CustomPresent;

	DXGI_MODE_DESC SetupDXGI_MODE_DESC() const;
};

template<>
struct TD3D12ResourceTraits<FRHIViewport>
{
	typedef FD3D12Viewport TConcreteType;
};
