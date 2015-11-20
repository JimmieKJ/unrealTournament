// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Viewport.cpp: D3D viewport RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RenderCore.h"

#include "AllowWindowsPlatformTypes.h"
#include <dwmapi.h>

extern FD3D12Texture2D* GetSwapChainSurface(FD3D12Device* Parent, EPixelFormat PixelFormat, IDXGISwapChain3* SwapChain, const uint32 &backBufferIndex);

FD3D12Viewport::FD3D12Viewport(class FD3D12Device* InParent, HWND InWindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat) :
	LastFlipTime(0),
	LastFrameComplete(0),
	LastCompleteTime(0),
	SyncCounter(0),
	bSyncedLastFrame(false),
	WindowHandle(InWindowHandle),
    MaximumFrameLatency(3),
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen),
	PixelFormat(InPreferredPixelFormat),
	bIsValid(true),
	bIsBenchmarkMode(false),
	NumBackBuffers(DefaultNumBackBuffers),
    FD3D12DeviceChild(InParent)
{
	check(IsInGameThread());
    GetParentDevice()->GetViewports().Add(this);
}

//Init for a Viewport that will do the presenting
void FD3D12Viewport::Init(IDXGIFactory4* Factory, bool AssociateWindow)
{
    // Ensure that the D3D devices have been created.
    // TODO: is this really necessary?
    //D3DRHI->InitD3DDevices();

	bIsBenchmarkMode = FParse::Param(FCommandLine::Get(), TEXT("benchmark"));

	DXGI_SWAP_CHAIN_FLAG swapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (bIsBenchmarkMode)
	{
		swapChainFlags = static_cast<DXGI_SWAP_CHAIN_FLAG>(static_cast<uint32>(swapChainFlags) | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
		NumBackBuffers = MaxNumBackBuffers;
	}

	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	FMemory::Memzero(&SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

	// Create the swapchain.
	{
		TRefCountPtr<IDXGISwapChain> SwapChain;
		SwapChainDesc.BufferDesc = SetupDXGI_MODE_DESC();
		// MSAA Sample count
		SwapChainDesc.SampleDesc.Count = 1;
		SwapChainDesc.SampleDesc.Quality = 0;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
		// 1:single buffering, 2:double buffering, 3:triple buffering
		SwapChainDesc.BufferCount = NumBackBuffers;
		SwapChainDesc.OutputWindow = WindowHandle;
		SwapChainDesc.Windowed = !bIsFullscreen;
		// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.Flags = swapChainFlags;
		ID3D12CommandQueue* CommandQueue = GetParentDevice()->GetCommandListManager().GetD3DCommandQueue();

		VERIFYD3D11RESULT(Factory->CreateSwapChain(CommandQueue, &SwapChainDesc, SwapChain.GetInitReference()));

		VERIFYD3D11RESULT(SwapChain->QueryInterface(IID_PPV_ARGS(SwapChain3.GetInitReference())));
	}

	if (bIsBenchmarkMode)
	{
		VERIFYD3D11RESULT(SwapChain3->SetMaximumFrameLatency(NumBackBuffers - 1));
	}

    if (AssociateWindow)
    {
        // Set the DXGI message hook to not change the window behind our back.
        Factory->MakeWindowAssociation(WindowHandle, DXGI_MWA_NO_WINDOW_CHANGES);
    }

    // Create a RHI surface to represent the viewport's back buffer.
    for (uint32 i = 0; i < NumBackBuffers; ++i)
    {
        BackBuffers[i] = GetSwapChainSurface(GetParentDevice(), PixelFormat, SwapChain3, i);
    }

    if (AssociateWindow)
    {
        // Tell the window to redraw when they can.
        // @todo: For Slate viewports, it doesn't make sense to post WM_PAINT messages (we swallow those.)
        ::PostMessage(WindowHandle, WM_PAINT, 0, 0);
    }
}

void FD3D12Viewport::ConditionalResetSwapChain(bool bIgnoreFocus)
{
}

#include "HideWindowsPlatformTypes.h"
