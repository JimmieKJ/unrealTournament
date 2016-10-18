// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "SlateAtlasedTextureResource.h"
#include "Slate/SlateTextureAtlasInterface.h"

TSharedPtr<FSlateAtlasedTextureResource> FSlateAtlasedTextureResource::NullResource = MakeShareable( new FSlateAtlasedTextureResource(nullptr) );

FSlateAtlasedTextureResource::FSlateAtlasedTextureResource(UTexture* InTexture)
	: FSlateBaseUTextureResource(InTexture)
{
}

FSlateAtlasedTextureResource::~FSlateAtlasedTextureResource()
{
	for ( FObjectResourceMap::TIterator ProxyIt(ProxyMap); ProxyIt; ++ProxyIt )
	{
		FSlateShaderResourceProxy* Proxy = ProxyIt.Value();
		delete Proxy;
	}
}

FSlateShaderResourceProxy* FSlateAtlasedTextureResource::FindOrCreateAtlasedProxy(UObject* InAtlasedObject, const FSlateAtlasData& AtlasData)
{
	FSlateShaderResourceProxy* Proxy = ProxyMap.FindRef(InAtlasedObject);
	if ( Proxy == nullptr )
	{
		FVector2D ActualSize(TextureObject->GetSurfaceWidth(), TextureObject->GetSurfaceHeight());

		Proxy = new FSlateShaderResourceProxy();
		Proxy->Resource = this;
		Proxy->ActualSize = ActualSize.IntPoint();
		Proxy->StartUV = AtlasData.StartUV;
		Proxy->SizeUV = AtlasData.SizeUV;

		ProxyMap.Add(InAtlasedObject, Proxy);
	}

	return Proxy;
}
