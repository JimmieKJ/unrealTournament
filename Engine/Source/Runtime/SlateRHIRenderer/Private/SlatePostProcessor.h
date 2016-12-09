// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "Layout/SlateRect.h"

class FSlatePostProcessResource;
class IRendererModule;

struct FPostProcessRectParams
{
	FTexture2DRHIRef SourceTexture;
	FSlateRect SourceRect;
	FSlateRect DestRect;
	FIntPoint SourceTextureSize;
	TFunction<void()> RestoreStateFunc; 
};

struct FBlurRectParams
{
	int32 KernelSize;
	int32 DownsampleAmount;
	float Strength;
};

class FSlatePostProcessor
{
public:
	FSlatePostProcessor();
	~FSlatePostProcessor();

	void BlurRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FBlurRectParams& Params, const FPostProcessRectParams& RectParams);

private:
	void DownsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize);
	void UpsampleRect(FRHICommandListImmediate& RHICmdList, IRendererModule& RendererModule, const FPostProcessRectParams& Params, const FIntPoint& DownsampleSize);
	int32 ComputeBlurWeights(int32 KernelSize, float StdDev, TArray<FVector4>& OutWeightsAndOffsets);
private:
	FSlatePostProcessResource* IntermediateTargets;
};