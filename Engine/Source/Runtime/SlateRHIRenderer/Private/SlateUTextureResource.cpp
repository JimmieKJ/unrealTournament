// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "SlateUTextureResource.h"

TSharedPtr<FSlateUTextureResource> FSlateUTextureResource::NullResource = MakeShareable( new FSlateUTextureResource(nullptr) );

FSlateUTextureResource::FSlateUTextureResource(UTexture* InTexture)
	: Proxy(new FSlateShaderResourceProxy)
	, TextureObject(InTexture)
{
	if(TextureObject)
	{
		Proxy->ActualSize = FIntPoint(InTexture->GetSurfaceWidth(), InTexture->GetSurfaceHeight());
		Proxy->Resource = this;
	}
}

FSlateUTextureResource::~FSlateUTextureResource()
{
	if ( Proxy )
	{
		delete Proxy;
	}
}

void FSlateUTextureResource::UpdateRenderResource(FTexture* InFTexture)
{
	if ( InFTexture )
	{
		// If the RHI data has changed, it's possible the underlying size of the texture has changed,
		// if that's true we need to update the actual size recorded on the proxy as well, otherwise 
		// the texture will continue to render using the wrong size.
		Proxy->ActualSize = FIntPoint(InFTexture->GetSizeX(), InFTexture->GetSizeY());
	}
}


uint32 FSlateUTextureResource::GetWidth() const
{ 
	return TextureObject->GetSurfaceWidth();
}

uint32 FSlateUTextureResource::GetHeight() const 
{ 
	return TextureObject->GetSurfaceHeight(); 
}

ESlateShaderResource::Type FSlateUTextureResource::GetType() const
{ 
	return ESlateShaderResource::TextureObject;
}
