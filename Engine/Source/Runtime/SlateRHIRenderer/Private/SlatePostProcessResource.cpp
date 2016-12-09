// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePostProcessResource.h"
#include "RenderUtils.h"

DECLARE_MEMORY_STAT(TEXT("PostProcess RenderTargets"), STAT_SLATEPPRenderTargetMem, STATGROUP_SlateMemory);

FSlatePostProcessResource::FSlatePostProcessResource(int32 InRenderTargetCount)
	: RenderTargetSize(FIntPoint::ZeroValue)
	, RenderTargetCount(InRenderTargetCount)
{
	
}

FSlatePostProcessResource::~FSlatePostProcessResource()
{

}

void FSlatePostProcessResource::Resize(const FIntPoint& NewSize)
{
	check(IsInRenderingThread());
	if(RenderTargetSize != NewSize)
	{
		RenderTargetSize = NewSize;
		UpdateRHI();
	}
}

void FSlatePostProcessResource::CleanUp()
{
	BeginReleaseResource(this);

	BeginCleanup(this);
}

void FSlatePostProcessResource::InitDynamicRHI()
{
	RenderTargets.Empty();

	PixelFormat = PF_B8G8R8A8;
	if(RenderTargetSize.X > 0 && RenderTargetSize.Y > 0)
	{
		for (int32 TexIndex = 0; TexIndex < RenderTargetCount; ++TexIndex)
		{
			FTexture2DRHIRef RenderTargetTextureRHI;
			FTexture2DRHIRef ShaderResourceUnused;
			FRHIResourceCreateInfo CreateInfo;
			RHICreateTargetableShaderResource2D(
				RenderTargetSize.X,
				RenderTargetSize.Y,
				PixelFormat,
				1,
				/*TexCreateFlags=*/0,
				TexCreate_RenderTargetable,
				/*bNeedsTwoCopies=*/false,
				CreateInfo,
				RenderTargetTextureRHI,
				ShaderResourceUnused
			);

			RenderTargets.Add(RenderTargetTextureRHI);
		}
	}

	STAT(int64 TotalMemory = RenderTargetCount * GPixelFormats[PixelFormat].BlockBytes*RenderTargetSize.X*RenderTargetSize.Y);
	SET_MEMORY_STAT(STAT_SLATEPPRenderTargetMem, TotalMemory);
}

void FSlatePostProcessResource::ReleaseDynamicRHI()
{
	SET_MEMORY_STAT(STAT_SLATEPPRenderTargetMem, 0);

	RenderTargets.Empty();
}

void FSlatePostProcessResource::FinishCleanup()
{
	delete this;
}


