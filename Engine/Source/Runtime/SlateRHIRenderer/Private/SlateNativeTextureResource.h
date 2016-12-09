// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateShaderResourceProxy;
class FSlateTexture2DRHIRef;

/** Represents a dynamic texture resource for rendering in Slate*/
class FSlateDynamicTextureResource
{
public:
	static TSharedPtr<FSlateDynamicTextureResource> NullResource;

	FSlateDynamicTextureResource(FSlateTexture2DRHIRef* ExistingTexture);
	~FSlateDynamicTextureResource();

	FSlateShaderResourceProxy* Proxy;
	FSlateTexture2DRHIRef* RHIRefTexture;
};
