// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


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