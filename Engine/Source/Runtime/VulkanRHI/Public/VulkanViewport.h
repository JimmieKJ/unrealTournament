// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanViewport.h: Vulkan viewport RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"

class FVulkanDynamicRHI;
struct FVulkanSwapChain;
struct FVulkanQueue;

class FVulkanViewport : public FRHIViewport
{
public:
	enum { NUM_BUFFERS = 3 };

	FVulkanViewport(FVulkanDynamicRHI* InRHI, void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat);
	~FVulkanViewport();

	FVulkanTexture2D* GetBackBuffer() const { return BackBuffers[CurrentBackBuffer]; }
	uint32 GetBackBufferIndex() const { return CurrentBackBuffer; }

	//bool Present(VkImage& Image, VkQueue& Queue, bool bLockToVsync);

	//#todo-rco
	void WaitForFrameEventCompletion() {}

	//#todo-rco
	void IssueFrameEvent() {}

	FIntPoint GetSizeXY() const { return FIntPoint(SizeX, SizeY); }

protected:
	TRefCountPtr<FVulkanTexture2D> BackBuffers[NUM_BUFFERS];
	FVulkanDynamicRHI* RHI;
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	EPixelFormat PixelFormat;
	int CurrentBackBuffer;
	FVulkanSwapChain* SwapChain;

	friend class FVulkanDynamicRHI;
	friend class FVulkanCommandListContext;
};

template<>
struct TVulkanResourceTraits<FRHIViewport>
{
	typedef FVulkanViewport TConcreteType;
};
