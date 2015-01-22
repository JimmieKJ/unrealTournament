// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "SlateUTextureResource.h"

TSharedPtr<FSlateUTextureResource> FSlateUTextureResource::NullResource = MakeShareable( new FSlateUTextureResource(nullptr) );

FSlateUTextureResource::FSlateUTextureResource(UTexture2D* InTexture)
	: Proxy(new FSlateShaderResourceProxy)
	, TextureObject(InTexture)
{
	if(TextureObject)
	{
		Proxy->ActualSize = FIntPoint(TextureObject->GetSizeX(), TextureObject->GetSizeY());
		Proxy->Resource = this;
	}
}

FSlateUTextureResource::~FSlateUTextureResource()
{
	if(Proxy)
	{
		delete Proxy;
	}
}

void FSlateUTextureResource::UpdateRenderResource(FTexture* InFTexture)
{
	ShaderResource = InFTexture ? FTexture2DRHIRef(InFTexture->TextureRHI->GetTexture2D()) : FTexture2DRHIRef();
}

uint32 FSlateUTextureResource::GetWidth() const
{ 
	return TextureObject->GetSizeX();
}

uint32 FSlateUTextureResource::GetHeight() const 
{ 
	return TextureObject->GetSizeY(); 
}

ESlateShaderResource::Type FSlateUTextureResource::GetType() const
{ 
	return ESlateShaderResource::TextureObject;
}
